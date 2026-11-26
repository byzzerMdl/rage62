#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
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

struct PsinResource {
    std::string data;
    std::array<PsinChunk, 7> chunks;
    std::vector<PsinStructure> structures;
    std::vector<PsinEnum> enums;
};

// stored_size is the exact stored length, excluding the RPF9 span's padding.
// Accepts a raw seven-chunk PSIN resource or one zlib member containing it.
bool load_psin(Rpf9Archive& archive, std::size_t entry, std::uint64_t stored_size,
               PsinResource& resource, std::string& error);

}
