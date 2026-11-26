#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace rage62 {

class Rpf9Archive;

struct Rpf9DecodedChunk {
    std::size_t size;
    bool compressed;
};

struct Rpf9ChunkedPayload {
    std::string data;
    std::vector<Rpf9DecodedChunk> chunks;
};

// The caller supplies the plain chunk-table value and the exact stored length.
bool read_rpf9_chunks(Rpf9Archive& archive, std::size_t entry, std::uint64_t stored_size,
                      std::string_view table, std::string& decoded, std::string& error);

bool read_rpf9_chunks(Rpf9Archive& archive, std::size_t entry, std::uint64_t stored_size,
                      std::string_view table, Rpf9ChunkedPayload& decoded, std::string& error);

}
