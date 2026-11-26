#pragma once

#include "rage62/rpf9.h"
#include <algorithm>
#include <array>

namespace rage62::detail {

inline bool fail(std::string& error, const char* message) {
    error = message;
    return false;
}

inline std::uint32_t little_word(const unsigned char* bytes) {
    return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8) |
           (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
}

inline bool copy_bytes(std::istream& input, std::ostream& output, std::uint64_t remaining,
                       std::string& error) {
    std::array<char, 64 * 1024> buffer;
    while (remaining != 0) {
        const auto size = std::min<std::uint64_t>(remaining, buffer.size());
        const auto length = static_cast<std::streamsize>(size);
        if (!input.read(buffer.data(), length)) return fail(error, "cannot read archive payload");
        if (!output.write(buffer.data(), length)) return fail(error, "cannot write archive payload");
        remaining -= size;
    }
    error.clear();
    return true;
}

bool read_rpf9_header(std::istream& stream, std::uint64_t file_size, Rpf9Header& header, std::string& error);

}
