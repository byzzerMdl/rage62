#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace rage62 {

class Rpf9Archive;

struct PsinChunk {
    std::array<char, 4> tag;
    std::size_t offset;
    std::size_t size;
};

struct PsinResource {
    std::string data;
    std::array<PsinChunk, 7> chunks;
};

// stored_size is the exact stored length, excluding the RPF9 span's padding.
// Accepts a raw seven-chunk PSIN resource or one zlib member containing it.
bool load_psin(Rpf9Archive& archive, std::size_t entry, std::uint64_t stored_size,
               PsinResource& resource, std::string& error);

}
