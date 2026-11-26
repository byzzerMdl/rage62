#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace rage62 {

class Rpf9Archive;

struct PsinChunk {
    std::array<char, 4> tag;
    std::size_t offset;
    std::size_t size;
};

struct PsinField {
    std::uint32_t name_hash;
    std::uint32_t offset;
    std::uint32_t type_word;
    std::uint32_t reference;
};

struct PsinStructure {
    std::uint32_t name_hash;
    std::uint32_t size;
    std::vector<PsinField> fields;
};

struct PsinEnumValue {
    std::int64_t value;
    std::uint32_t name_hash;
};

struct PsinEnum {
    std::uint32_t name_hash;
    std::vector<PsinEnumValue> values;
};

struct PsinBlock {
    std::uint32_t id;
    std::size_t offset; // Absolute offset within PsinResource::data.
    std::size_t size;
};

struct PsinResource {
    std::string data;
    std::vector<PsinChunk> chunks;
    std::vector<PsinBlock> blocks;
    std::vector<PsinBlock> schema_blocks; // Opaque PSCH values in the PMAP family.
    std::vector<PsinStructure> structures;
    std::vector<PsinEnum> enums;
};

bool parse_psin(std::string bytes, PsinResource& resource, std::string& error);

// stored_size is the exact stored length, excluding the RPF9 span's padding.
// Accepts a raw PSIN resource or one zlib member containing it.
bool load_psin(Rpf9Archive& archive, std::size_t entry, std::uint64_t stored_size,
               PsinResource& resource, std::string& error);

bool load_chunked_psin(Rpf9Archive& archive, std::size_t entry, std::uint64_t stored_size,
                       std::string_view table, PsinResource& resource, std::string& error);

}
