#include "rage62/rpf9_chunks.h"
#include "archive_io.h"
#include "zlib_member.h"

#include <utility>

namespace rage62 {
namespace {

constexpr std::size_t table_header_size = 8;
constexpr std::size_t word_size = 8;
using detail::fail;
using detail::little_word;

struct Chunk {
    std::size_t logical_size;
    std::size_t stored_size;
    std::size_t padding;
    unsigned method;
};

Chunk decode_chunk(unsigned layout, std::uint64_t word) {
    switch (layout) {
    case 2:
        return {std::size_t(word & 0xffffff) << 4, std::size_t((word >> 32) & 0xffffff) << 4,
                std::size_t((word >> 58) & 0xf), unsigned(word >> 62)};
    case 3: {
        const auto coded = std::size_t((word >> 16) & 0xffff);
        return {0x10000 - std::size_t(word & 0xffff), 0x10000 - (coded & 0xfff0), coded & 0xf,
                unsigned((word >> 60) & 3)};
    }
    default: // Layout 4, validated by the caller.
        return {std::size_t(word & 0xfffffff), std::size_t((word >> 28) & 0xffffff) << 4,
                std::size_t((word >> 52) & 0xf), unsigned((word >> 56) & 3)};
    }
}

}

bool read_rpf9_chunks(Rpf9Archive& archive, std::size_t entry, std::uint64_t stored_size,
                      std::string_view table, std::string& decoded, std::string& error) {
    if (entry >= archive.entries().size()) return fail(error, "RPF9 chunked entry index out of range");
    if (stored_size > archive.entries()[entry].span || stored_size > detail::max_resource_bytes) {
        return fail(error, "RPF9 chunked entry exceeds its stored span or the 64 MiB limit");
    }
    if (table.size() < table_header_size) return fail(error, "truncated RPF9 chunk table header");

    const auto* bytes = reinterpret_cast<const unsigned char*>(table.data());
    const auto offset = little_word(bytes);
    const auto count = unsigned(bytes[4]) | (unsigned(bytes[5]) << 8);
    const auto layout = unsigned(bytes[7]);
    if (layout < 2 || layout > 4) return fail(error, "unsupported RPF9 chunk encoding");
    if (offset < table_header_size || offset > table.size() || count == 0 ||
        count > (table.size() - offset) / word_size) {
        return fail(error, "invalid RPF9 chunk table record range");
    }

    std::uint64_t position = layout == 2 ? 16 : 0;
    if (position > stored_size) return fail(error, "truncated RPF9 chunked payload prefix");
    std::string encoded;
    std::string result;
    const auto* record = bytes + offset;
    for (unsigned index = 0; index < count; ++index, record += word_size) {
        const auto word = (std::uint64_t(little_word(record + 4)) << 32) | little_word(record);
        const auto chunk = decode_chunk(layout, word);
        if (chunk.stored_size <= chunk.padding || chunk.stored_size > stored_size - position ||
            chunk.logical_size == 0 || chunk.logical_size > detail::max_resource_bytes - result.size()) {
            return fail(error, "RPF9 chunk exceeds the declared sizes");
        }
        const auto used = chunk.stored_size - chunk.padding;
        if (chunk.method != 3 && used != chunk.logical_size) return fail(error, "RPF9 copy chunk size mismatch");
        encoded.resize(used);
        std::size_t received;
        if (!archive.read_span(entry, position, encoded.data(), used, received, error)) return false;
        if (received != used) return fail(error, "truncated RPF9 chunk payload");

        if (chunk.method == 3) {
            const auto expected = result.size() + chunk.logical_size;
            if (!detail::append_zlib_member(encoded, result, expected, error)) return false;
            if (result.size() != expected) return fail(error, "RPF9 decoded chunk size mismatch");
        } else {
            result.append(encoded);
        }
        position += chunk.stored_size;
    }
    if (position != stored_size) return fail(error, "RPF9 chunks do not cover the stored entry size");
    decoded = std::move(result);
    error.clear();
    return true;
}

}
