#include "rage62/psin.h"
#include "rage62/rpf9_chunks.h"
#include "archive_io.h"
#include "zlib_member.h"

#include <limits>
#include <string_view>
#include <utility>

namespace rage62 {
namespace {

constexpr std::size_t chunk_header_size = 8;
constexpr std::size_t schema_record_size = 16;
constexpr std::array<std::string_view, 7> lsch_chain = {"PSIN", "LSCH", "LENM", "LSNS", "LGID", "LRSC", "CHKS"};
using detail::fail;
using detail::little_word;
using detail::max_resource_bytes;

struct SchemaTable {
    std::size_t header_size;
    std::size_t marker_offset;
    unsigned char marker;
    std::size_t count_offset;
    unsigned count_shift;
    const char* diagnostic;

    std::uint32_t count(const unsigned char* header) const {
        return little_word(header + count_offset) >> count_shift;
    }
};

constexpr SchemaTable structures{32, 16, 0xaa, 28, 16, "invalid PSIN LSCH table"};
constexpr SchemaTable enums{24, 8, 0xee, 16, 0, "invalid PSIN LENM table"};

std::uint32_t big_word(const unsigned char* bytes) {
    return (std::uint32_t(bytes[0]) << 24) | (std::uint32_t(bytes[1]) << 16) |
           (std::uint32_t(bytes[2]) << 8) | bytes[3];
}

std::uint64_t aligned16(std::uint32_t value) {
    return (std::uint64_t(value) + 15) & ~std::uint64_t{15};
}

bool is_tag(const PsinChunk& chunk, std::string_view tag) {
    return std::string_view(chunk.tag.data(), chunk.tag.size()) == tag;
}

bool matches_psin_chain(const std::vector<PsinChunk>& chunks) {
    if (chunks.size() < 6 || !is_tag(chunks.front(), "PSIN")) return false;
    if (is_tag(chunks[1], "LSCH")) {
        return chunks.size() == lsch_chain.size() &&
               std::equal(chunks.begin(), chunks.end(), lsch_chain.begin(), is_tag);
    }
    return is_tag(chunks[1], "PMAP") && is_tag(chunks[2], "PSCH") && is_tag(chunks[3], "PSIG") &&
           (chunks.size() == 6 || is_tag(chunks[4], "STDN") || is_tag(chunks[4], "STFN")) &&
           is_tag(chunks[chunks.size() - 2], "STNS") && is_tag(chunks.back(), "CHKS");
}

bool read_chunks(PsinResource& resource, std::string& error) {
    const std::string_view bytes(resource.data);
    std::size_t offset = 0;
    while (offset != bytes.size()) {
        if (resource.chunks.size() == lsch_chain.size()) return fail(error, "trailing bytes after PSIN chunk chain");
        if (bytes.size() - offset < chunk_header_size) return fail(error, "truncated PSIN chunk header");
        const auto* length = reinterpret_cast<const unsigned char*>(bytes.data() + offset + 4);
        const auto size = big_word(length);
        if (size < chunk_header_size || size > bytes.size() - offset) return fail(error, "invalid PSIN chunk length");
        auto& chunk = resource.chunks.emplace_back();
        std::copy_n(bytes.data() + offset, chunk.tag.size(), chunk.tag.begin());
        chunk.offset = offset;
        chunk.size = size;
        offset += size;
    }
    return matches_psin_chain(resource.chunks) || fail(error, "unsupported PSIN chunk order");
}

std::string_view chunk_body(const PsinResource& resource, std::size_t index) {
    const auto& chunk = resource.chunks[index];
    return std::string_view(resource.data).substr(chunk.offset + chunk_header_size, chunk.size - chunk_header_size);
}

bool has_descriptor_marker(const unsigned char* marker) {
    return std::all_of(marker, marker + 8, [](unsigned char byte) { return byte == 0xaa; });
}

bool read_lsch_blocks(PsinResource& resource, std::string& error) {
    const auto end = resource.chunks[0].size;
    const auto* data = reinterpret_cast<const unsigned char*>(resource.data.data());
    // Earlier framing-only fixtures and unrecognized pre-sections remain opaque.
    if (end < 24 || !has_descriptor_marker(data + 16)) return true;

    std::size_t descriptor = 8;
    std::uint64_t remaining = end - 16;
    while (remaining != 0) {
        if (remaining < 16 || !has_descriptor_marker(data + descriptor + 8)) {
            return fail(error, "invalid PSIN block descriptor table");
        }
        const auto length = little_word(data + descriptor + 4);
        const auto padded = aligned16(length);
        if (padded > remaining - 16) return fail(error, "PSIN block exceeds the data region");
        resource.blocks.push_back({little_word(data + descriptor), 0, length});
        descriptor += 16;
        remaining -= 16 + padded;
    }
    auto offset = descriptor + 8;
    for (auto& block : resource.blocks) {
        block.offset = offset;
        offset += static_cast<std::size_t>(aligned16(static_cast<std::uint32_t>(block.size)));
    }
    return true;
}

bool read_pmap_blocks(PsinResource& resource, std::string& error) {
    const auto body = chunk_body(resource, 1);
    if (body.size() < 16) return fail(error, "truncated PSIN PMAP header");
    const auto* data = reinterpret_cast<const unsigned char*>(body.data());
    const auto count = (unsigned(data[8]) << 8) | data[9];
    if (count == 0 || body.size() - 16 != std::size_t(count) * 16) return fail(error, "invalid PSIN PMAP record count");

    const auto first_section = resource.chunks[0].size;
    std::uint64_t expected = 16;
    resource.blocks.reserve(count);
    for (unsigned index = 0; index < count; ++index) {
        const auto* record = data + 16 + index * 16;
        const auto offset = big_word(record + 4);
        const auto length = big_word(record + 12);
        if (offset != expected || offset > first_section || length > first_section - offset) {
            return fail(error, "invalid PSIN PMAP block range");
        }
        resource.blocks.push_back({big_word(record), offset, length});
        expected += index + 1 == count ? length : aligned16(length);
    }
    return expected == first_section || fail(error, "PSIN PMAP blocks do not end at the first section");
}

bool read_psch_blocks(PsinResource& resource, std::string& error) {
    const auto body = chunk_body(resource, 2);
    if (body.size() < 12) return fail(error, "truncated PSIN PSCH header");
    const auto* data = reinterpret_cast<const unsigned char*>(body.data());
    const auto count = big_word(data);
    if (count > (body.size() - 12) / 8) return fail(error, "invalid PSIN PSCH record count");
    const auto values_at = 12 + std::size_t(count) * 8;
    if ((count == 0 && values_at != body.size()) || (count != 0 && big_word(data + 8) != values_at)) {
        return fail(error, "invalid PSIN PSCH value start");
    }

    const auto body_offset = resource.chunks[2].offset + chunk_header_size;
    resource.schema_blocks.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto* row = data + 4 + std::size_t(index) * 8;
        const std::size_t from = big_word(row + 4);
        const std::size_t to = index + 1 == count ? body.size() : big_word(row + 12);
        if (from >= to || to > body.size()) return fail(error, "invalid PSIN PSCH value range");
        resource.schema_blocks.push_back({big_word(row), body_offset + from, to - from});
    }
    return true;
}

bool schema_headers(std::string_view body, const SchemaTable& format, std::size_t& end, std::string& error) {
    const auto* data = reinterpret_cast<const unsigned char*>(body.data());
    end = 0;
    std::size_t record_bytes = 0;
    while (end + record_bytes != body.size()) {
        const auto remaining = body.size() - end - record_bytes;
        if (remaining < format.header_size) return fail(error, format.diagnostic);
        const auto* header = data + end;
        const auto* marker = header + format.marker_offset;
        if (std::any_of(marker, marker + 8, [&](unsigned char byte) { return byte != format.marker; })) {
            return fail(error, format.diagnostic);
        }
        const auto count = format.count(header);
        if (count > (remaining - format.header_size) / schema_record_size) return fail(error, format.diagnostic);
        record_bytes += std::size_t(count) * schema_record_size;
        end += format.header_size;
    }
    return true;
}

bool read_structures(PsinResource& resource, std::string& error) {
    const auto body = chunk_body(resource, 1);
    std::size_t header_end;
    if (!schema_headers(body, structures, header_end, error)) return false;
    const auto* header = reinterpret_cast<const unsigned char*>(body.data());
    const auto* field = header + header_end;
    resource.structures.reserve(header_end / structures.header_size);
    for (std::size_t pos = 0; pos < header_end; pos += structures.header_size, header += structures.header_size) {
        auto& type = resource.structures.emplace_back(PsinStructure{little_word(header), little_word(header + 24), {}});
        const auto count = structures.count(header);
        type.fields.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i, field += schema_record_size) {
            type.fields.push_back({little_word(field), little_word(field + 4),
                                   little_word(field + 8), little_word(field + 12)});
        }
    }
    return true;
}

std::int64_t signed_le64(const unsigned char* bytes) {
    const auto bits = (std::uint64_t(little_word(bytes + 4)) << 32) | little_word(bytes);
    if (bits <= std::uint64_t(std::numeric_limits<std::int64_t>::max())) return static_cast<std::int64_t>(bits);
    return -1 - static_cast<std::int64_t>(std::numeric_limits<std::uint64_t>::max() - bits);
}

bool read_enums(PsinResource& resource, std::string& error) {
    const auto body = chunk_body(resource, 2);
    std::size_t header_end;
    if (!schema_headers(body, enums, header_end, error)) return false;
    const auto* header = reinterpret_cast<const unsigned char*>(body.data());
    const auto* value = header + header_end;
    resource.enums.reserve(header_end / enums.header_size);
    for (std::size_t pos = 0; pos < header_end; pos += enums.header_size, header += enums.header_size) {
        auto& type = resource.enums.emplace_back(PsinEnum{little_word(header), {}});
        const auto count = enums.count(header);
        type.values.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i, value += schema_record_size) {
            type.values.push_back({signed_le64(value), little_word(value + 8)});
        }
    }
    return true;
}

}

bool parse_psin(std::string bytes, PsinResource& resource, std::string& error) {
    if (bytes.size() > max_resource_bytes) return fail(error, "PSIN exceeds the 64 MiB limit");
    PsinResource result;
    result.data = std::move(bytes);
    if (!read_chunks(result, error)) return false;
    if (is_tag(result.chunks[1], "LSCH")) {
        if (!read_lsch_blocks(result, error) || !read_structures(result, error) || !read_enums(result, error)) return false;
    } else if (!read_pmap_blocks(result, error) || !read_psch_blocks(result, error)) {
        return false;
    }
    resource = std::move(result);
    error.clear();
    return true;
}

bool load_psin(Rpf9Archive& archive, std::size_t entry, std::uint64_t stored_size,
               PsinResource& resource, std::string& error) {
    if (entry >= archive.entries().size()) return fail(error, "PSIN entry index out of range");
    if (stored_size == 0 || stored_size > archive.entries()[entry].span || stored_size > max_resource_bytes) {
        return fail(error, "PSIN stored size must fit its RPF9 span and the 64 MiB limit");
    }

    std::string encoded(static_cast<std::size_t>(stored_size), '\0');
    std::size_t received;
    if (!archive.read_span(entry, 0, encoded.data(), encoded.size(), received, error)) return false;
    if (received != encoded.size()) return fail(error, "truncated PSIN resource");

    if (encoded.compare(0, 4, "PSIN") == 0) return parse_psin(std::move(encoded), resource, error);
    const auto* header = reinterpret_cast<const unsigned char*>(encoded.data());
    if (encoded.size() < 2 || (header[0] & 0xf) != 8 || (header[0] >> 4) > 7 ||
        ((std::uint32_t(header[0]) << 8) | header[1]) % 31 != 0) {
        return fail(error, "expected PSIN resource or zlib member");
    }
    std::string decoded;
    return detail::append_zlib_member(encoded, decoded, max_resource_bytes, error) &&
           parse_psin(std::move(decoded), resource, error);
}

bool load_chunked_psin(Rpf9Archive& archive, std::size_t entry, std::uint64_t stored_size,
                       std::string_view table, PsinResource& resource, std::string& error) {
    std::string decoded;
    return read_rpf9_chunks(archive, entry, stored_size, table, decoded, error) &&
           parse_psin(std::move(decoded), resource, error);
}

}
