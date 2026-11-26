#pragma once

#include <algorithm>
#include <cstring>

namespace rage62::detail {

inline void set_error(char* output, std::size_t capacity, const char* message) {
    if (!output || capacity == 0) return;
    const auto length = std::min(std::strlen(message), capacity - 1);
    std::memcpy(output, message, length);
    output[length] = '\0';
}

}
