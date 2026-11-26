#include "archive_io.h"

#include <numeric>
#include <utility>

namespace rage62 {
namespace {
constexpr std::size_t header_size = 64;
constexpr std::size_t record_size = 24;
constexpr std::uint64_t alignment = 16;
constexpr std::uint64_t max_catalog_size = 64ull * 1024 * 1024;
using detail::fail;
using detail::little_word;
}

bool detail::read_rpf9_header(std::istream& stream, std::uint64_t file_size,
                              Rpf9Header& header, std::string& error) {
    std::array<unsigned char, header_size> bytes;
    if (file_size < bytes.size() || !stream.seekg(0) ||
        !stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) {
        return fail(error, "truncated RPF9 header");
    }
    if (little_word(bytes.data()) != 0x52504639) return fail(error, "expected RPF9 archive");

    Rpf9Header result{};
    result.file_size = file_size;
    result.payload_start = little_word(bytes.data() + 0x04);
    result.table_split = little_word(bytes.data() + 0x08);
    result.trailer_size = little_word(bytes.data() + 0x0c);
    if (result.table_split < header_size || result.table_split > result.payload_start ||
        result.payload_start > file_size || result.trailer_size > file_size - result.payload_start ||
        result.table_split % alignment != 0 || result.payload_start % alignment != 0) {
        return fail(error, "invalid RPF9 region boundaries");
    }
    result.payload_end = file_size - result.trailer_size;
    result.archive_id = little_word(bytes.data() + 0x10) |
                        (std::uint64_t(little_word(bytes.data() + 0x14)) << 32);
    result.layout_word = little_word(bytes.data() + 0x20);
    result.entry_count = little_word(bytes.data() + 0x24);
    result.flags = little_word(bytes.data() + 0x2c);
    header = result;
    error.clear();
    return true;
}

bool Rpf9Archive::open(const std::filesystem::path& path, std::string& error) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) return fail(error, "cannot open RPF9 archive");
    const auto end = stream.tellg();
    if (end < 0) return fail(error, "cannot determine RPF9 archive size");
    Rpf9Header header;
    if (!detail::read_rpf9_header(stream, static_cast<std::uint64_t>(end), header, error)) return false;
    stream_ = std::move(stream);
    header_ = header;
    entries_.clear();
    return true;
}

bool Rpf9Archive::load_catalog(const std::filesystem::path& path, std::string& error) {
    if (!stream_.is_open()) return fail(error, "no RPF9 archive is open");
    const auto expected_size = std::uint64_t(header_.entry_count) * record_size;
    if (expected_size > max_catalog_size) return fail(error, "RPF9 catalog exceeds the 64 MiB reader limit");
    std::ifstream table(path, std::ios::binary | std::ios::ate);
    if (!table) return fail(error, "cannot open plain RPF9 catalog");
    const auto end = table.tellg();
    if (end < 0 || static_cast<std::uint64_t>(end) != expected_size || !table.seekg(0)) {
        return fail(error, "RPF9 catalog length must equal entry count times 24");
    }

    std::vector<Rpf9Entry> entries;
    entries.reserve(header_.entry_count);
    for (std::uint32_t index = 0; index < header_.entry_count; ++index) {
        std::array<unsigned char, record_size> bytes;
        if (!table.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) {
            return fail(error, "truncated RPF9 location record");
        }
        if (little_word(bytes.data() + 4) != 0 || little_word(bytes.data() + 20) != 0) {
            return fail(error, "unsupported RPF9 location record layout");
        }
        const auto offset = std::uint64_t(little_word(bytes.data() + 8)) * alignment;
        if (offset < header_.payload_start || offset >= header_.payload_end) {
            return fail(error, "RPF9 entry offset lies outside the payload region");
        }
        entries.push_back({offset, 0, little_word(bytes.data()), little_word(bytes.data() + 12),
                           little_word(bytes.data() + 16)});
    }

    std::vector<std::size_t> sorted(entries.size());
    std::iota(sorted.begin(), sorted.end(), std::size_t{0});
    std::sort(sorted.begin(), sorted.end(), [&](std::size_t a, std::size_t b) {
        return entries[a].offset > entries[b].offset;
    });
    auto boundary = header_.payload_end;
    auto previous = header_.payload_end;
    for (const auto index : sorted) {
        auto& entry = entries[index];
        if (entry.offset != previous) {
            boundary = previous;
            previous = entry.offset;
        }
        entry.span = boundary - entry.offset;
    }

    entries_ = std::move(entries);
    error.clear();
    return true;
}

bool Rpf9Archive::copy_span(std::size_t index, std::ostream& output, std::string& error) {
    if (index >= entries_.size()) return fail(error, "RPF9 entry index out of range");
    const auto& entry = entries_[index];
    stream_.clear();
    if (!stream_.seekg(static_cast<std::streamoff>(entry.offset))) {
        return fail(error, "cannot seek to RPF9 entry");
    }
    return detail::copy_bytes(stream_, output, entry.span, error);
}

}
