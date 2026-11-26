#pragma once

#include <cstddef>
#include <string>

namespace rage62::detail {

inline constexpr std::size_t max_resource_bytes = 64 * 1024 * 1024;

bool append_zlib_member(std::string& encoded, std::string& decoded, std::size_t output_limit,
                        std::string& error);

}
