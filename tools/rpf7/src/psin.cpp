#include "rage62/psin.h"
#include "archive_io.h"

#include <string_view>
#include <utility>

#ifdef RAGE62_HAS_ZLIB
#include <zlib.h>
#endif

namespace rage62 {
namespace {

constexpr std::size_t resource_limit = 64 * 1024 * 1024;
constexpr std::size_t chunk_header_size = 8;
constexpr std::array<std::string_view, 7> chain = {"PSIN", "LSCH", "LENM", "LSNS", "LGID", "LRSC", "CHKS"};
using detail::fail;

bool inflate_zlib(std::string& encoded, std::string& decoded, std::string& error) {
#ifdef RAGE62_HAS_ZLIB
    z_stream inflater{};
    if (inflateInit2(&inflater, MAX_WBITS) != Z_OK) return fail(error, "cannot initialize PSIN inflater");
    struct InflaterEnd {
        z_stream& stream;
        ~InflaterEnd() { inflateEnd(&stream); }
    } cleanup{inflater};
    inflater.next_in = reinterpret_cast<Bytef*>(encoded.data());
    inflater.avail_in = static_cast<uInt>(encoded.size());
    std::array<char, 32 * 1024> buffer;
    int status;
    do {
        inflater.next_out = reinterpret_cast<Bytef*>(buffer.data());
        inflater.avail_out = static_cast<uInt>(buffer.size());
        status = inflate(&inflater, Z_NO_FLUSH);
        if (status != Z_OK && status != Z_STREAM_END) return fail(error, "invalid PSIN zlib stream");
        const auto count = buffer.size() - inflater.avail_out;
        if (count > resource_limit - decoded.size()) return fail(error, "decoded PSIN exceeds the 64 MiB limit");
        decoded.append(buffer.data(), count);
    } while (status != Z_STREAM_END);
    return inflater.avail_in == 0 || fail(error, "trailing bytes after PSIN zlib stream");
#else
    (void)encoded;
    (void)decoded;
    return fail(error, "compressed PSIN requires a build with zlib");
#endif
}

bool read_chunks(PsinResource& resource, std::string& error) {
    const std::string_view bytes(resource.data);
    std::size_t offset = 0;
    for (std::size_t index = 0; index < chain.size(); ++index) {
        if (bytes.size() - offset < chunk_header_size) return fail(error, "truncated PSIN chunk header");
        if (bytes.substr(offset, 4) != chain[index]) return fail(error, "unsupported PSIN chunk order");
        const auto* length = reinterpret_cast<const unsigned char*>(bytes.data() + offset + 4);
        const auto size = (std::uint32_t(length[0]) << 24) | (std::uint32_t(length[1]) << 16) |
                          (std::uint32_t(length[2]) << 8) | length[3];
        if (size < chunk_header_size || size > bytes.size() - offset) return fail(error, "invalid PSIN chunk length");
        auto& chunk = resource.chunks[index];
        std::copy_n(bytes.data() + offset, chunk.tag.size(), chunk.tag.begin());
        chunk.offset = offset;
        chunk.size = size;
        offset += size;
    }
    return offset == bytes.size() || fail(error, "trailing bytes after PSIN chunk chain");
}

}

bool load_psin(Rpf9Archive& archive, std::size_t entry, std::uint64_t stored_size,
               PsinResource& resource, std::string& error) {
    if (entry >= archive.entries().size()) return fail(error, "PSIN entry index out of range");
    if (stored_size == 0 || stored_size > archive.entries()[entry].span || stored_size > resource_limit) {
        return fail(error, "PSIN stored size must fit its RPF9 span and the 64 MiB limit");
    }

    std::string encoded(static_cast<std::size_t>(stored_size), '\0');
    std::size_t received;
    if (!archive.read_span(entry, 0, encoded.data(), encoded.size(), received, error)) return false;
    if (received != encoded.size()) return fail(error, "truncated PSIN resource");

    PsinResource result;
    if (encoded.compare(0, 4, "PSIN") == 0) {
        result.data = std::move(encoded);
    } else {
        const auto* header = reinterpret_cast<const unsigned char*>(encoded.data());
        if (encoded.size() < 2 || (header[0] & 0xf) != 8 || (header[0] >> 4) > 7 ||
            ((std::uint32_t(header[0]) << 8) | header[1]) % 31 != 0) {
            return fail(error, "expected PSIN resource or zlib member");
        }
        if (!inflate_zlib(encoded, result.data, error)) return false;
    }
    if (!read_chunks(result, error)) return false;

    resource = std::move(result);
    error.clear();
    return true;
}

}
