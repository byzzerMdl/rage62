#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace rage62 {

class Rpf9Archive;

// The caller supplies the plain chunk-table value and the exact stored length.
bool read_rpf9_chunks(Rpf9Archive& archive, std::size_t entry, std::uint64_t stored_size,
                      std::string_view table, std::string& decoded, std::string& error);

}
