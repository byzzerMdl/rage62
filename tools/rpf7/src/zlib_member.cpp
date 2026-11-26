#include "zlib_member.h"
#include "archive_io.h"

#ifdef RAGE62_HAS_ZLIB
#include <zlib.h>
#endif

namespace rage62::detail {

bool append_zlib_member(std::string& encoded, std::string& decoded, std::size_t output_limit,
                        std::string& error) {
#ifdef RAGE62_HAS_ZLIB
    if (decoded.size() > output_limit) return fail(error, "zlib output already exceeds the expected limit");
    z_stream inflater{};
    if (inflateInit2(&inflater, MAX_WBITS) != Z_OK) return fail(error, "cannot initialize zlib decoder");
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
        if (status != Z_OK && status != Z_STREAM_END) return fail(error, "invalid zlib stream");
        const auto count = buffer.size() - inflater.avail_out;
        if (count > output_limit - decoded.size()) return fail(error, "zlib output exceeds the expected limit");
        decoded.append(buffer.data(), count);
    } while (status != Z_STREAM_END);
    return inflater.avail_in == 0 || fail(error, "trailing bytes after zlib stream");
#else
    (void)encoded;
    (void)decoded;
    (void)output_limit;
    return fail(error, "compressed data requires a build with zlib");
#endif
}

}
