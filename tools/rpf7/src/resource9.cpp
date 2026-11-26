#include "rage62/resource9.h"
#include "rage62/rpf9_chunks.h"
#include "archive_io.h"

#include <utility>

namespace rage62 {
namespace {

constexpr std::size_t split_chunk_size = 0x2000000;
constexpr std::size_t page_unit = 0x10000;
constexpr std::size_t map_header_size = 16;
constexpr std::size_t map_slot_size = 8;
constexpr std::size_t directory_record_size = 16;
using detail::fail;
using detail::little_word;

std::uint64_t little_quad(const unsigned char* bytes) {
    return (std::uint64_t(little_word(bytes + 4)) << 32) | little_word(bytes);
}

bool page_sizes_match(const std::vector<Rage9Page>& pages, std::size_t start, std::size_t count) {
    if (count <= 1) return true;
    for (std::size_t i = start; i + 1 < start + count; ++i) {
        if (pages[i].size % page_unit != 0) return false;
    }
    return pages[start + count - 1].size <= page_unit;
}

bool group_pages(const std::vector<Rpf9DecodedChunk>& chunks, std::size_t expected,
                 std::vector<Rage9Page>& pages, std::string& error) {
    if (chunks.size() < expected) return fail(error, "RAGE 9 resource is missing declared pages");
    const bool split = chunks.size() != expected;
    std::size_t offset = 0;
    for (std::size_t index = 0; index < chunks.size();) {
        const auto begin = index;
        auto size = chunks[index].size;
        while (split && index + 1 < chunks.size() && chunks[index].compressed &&
               chunks[index].size == split_chunk_size && chunks[index + 1].compressed) {
            size += chunks[++index].size;
        }
        pages.push_back({offset, size, index - begin + 1});
        offset += size;
        ++index;
    }
    return pages.size() == expected || fail(error, "RAGE 9 resource page grouping does not match the map");
}

}

bool Rage9Resource::load(Rpf9Archive& archive, std::size_t entry, std::uint64_t stored_size,
                         std::string_view table, std::string& error) {
    if (table.size() < 8 || table[7] != 2) return fail(error, "RAGE 9 resources require a plain encoding-2 chunk table");
    std::array<char, 16> prefix;
    std::size_t received = 0;
    if (stored_size < prefix.size()) return fail(error, "truncated RAGE 9 stored resource prefix");
    if (!archive.read_span(entry, 0, prefix.data(), prefix.size(), received, error)) return false;
    if (received != prefix.size() || std::any_of(prefix.begin(), prefix.end(), [](char byte) { return byte != 0; })) {
        return fail(error, "invalid RAGE 9 stored resource prefix");
    }

    Rpf9ChunkedPayload decoded;
    if (!read_rpf9_chunks(archive, entry, stored_size, table, decoded, error)) return false;
    if (decoded.data.size() < 16) return fail(error, "truncated RAGE 9 system header");
    const auto* data = reinterpret_cast<const unsigned char*>(decoded.data.data());
    const auto pointer = little_word(data + 8);
    const auto map = std::size_t(pointer & 0x0fffffff);
    if ((pointer >> 28) != 5 || little_word(data + 12) != 0 || map < 0x40 || map % 8 != 0 ||
        map > decoded.data.size() || map_header_size > decoded.data.size() - map) {
        return fail(error, "invalid RAGE 9 page map pointer");
    }
    const auto system = std::size_t(data[map + 8]);
    const auto graphics = std::size_t(data[map + 9]);
    if (system == 0) return fail(error, "RAGE 9 page map has no system pages");

    std::vector<Rage9Page> pages;
    if (!group_pages(decoded.chunks, system + graphics, pages, error)) return false;
    if (!page_sizes_match(pages, 0, system) || !page_sizes_match(pages, system, graphics)) {
        return fail(error, "RAGE 9 page sizes do not match their segment layout");
    }
    const auto system_end = pages[system - 1].offset + pages[system - 1].size;
    const auto map_size = map_header_size + map_slot_size * (system + graphics);
    if (map > system_end || map_size > system_end - map) return fail(error, "RAGE 9 page map exceeds the system segment");

    bytes_ = std::move(decoded.data);
    pages_ = std::move(pages);
    system_pages_ = system;
    system_size_ = system_end;
    page_map_offset_ = map;
    error.clear();
    return true;
}

std::optional<std::string_view> Rage9Resource::resolve(std::uint64_t pointer, std::size_t length) const {
    const auto selector = pointer >> 28;
    if (selector != 5 && selector != 6) return std::nullopt;
    const auto offset = std::size_t(pointer & 0x0fffffff);
    const auto size = selector == 5 ? system_size() : graphics_size();
    if (offset >= size || length > size - offset) return std::nullopt;
    const auto base = selector == 5 ? 0 : system_size();
    return std::string_view(bytes_).substr(base + offset, length);
}

bool Rage9Resource::read_0drp_root(Rage9DirectoryRoot& root, std::string& error) const {
    if (!is_0drp()) return fail(error, "RAGE 9 resource has no 0DRP directory");
    constexpr std::size_t root_end = 0x50;
    const auto header = resolve(0x50000000, root_end);
    if (!header) return fail(error, "truncated 0DRP root header");
    const auto* bytes = reinterpret_cast<const unsigned char*>(header->data());
    if (bytes[0x14] != 0x7a) return fail(error, "unsupported 0DRP root version");

    Rage9DirectoryRoot parsed{};
    parsed.word_20 = little_quad(bytes + 0x20);
    for (std::size_t i = 0; i < parsed.pointers.size(); ++i) parsed.pointers[i] = little_quad(bytes + 0x28 + i * 8);
    for (std::size_t i = 0; i < parsed.fields.size(); ++i) {
        const auto* field = bytes + 0x48 + i * 2;
        parsed.fields[i] = std::uint16_t(unsigned(field[0]) | (unsigned(field[1]) << 8));
    }
    const auto table = parsed.pointers[2];
    if (parsed.record_count() != 0 &&
        ((table >> 28) != 5 || !resolve(table, parsed.record_count() * directory_record_size))) {
        return fail(error, "0DRP record table exceeds the system segment");
    }

    root = parsed;
    error.clear();
    return true;
}

bool Rage9Resource::read_0drp_record(std::size_t index, Rage9DirectoryRecord& record, std::string& error) const {
    Rage9DirectoryRoot root;
    if (!read_0drp_root(root, error)) return false;
    if (index >= root.record_count()) return fail(error, "0DRP record index out of range");
    const auto view = resolve(root.pointers[2] + index * directory_record_size, directory_record_size);
    if (!view) return fail(error, "0DRP record exceeds the system segment");
    const auto* row = reinterpret_cast<const unsigned char*>(view->data());
    record = {little_word(row), little_word(row + 4), little_quad(row + 8)};
    return true;
}

}
