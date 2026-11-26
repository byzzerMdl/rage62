#include "rage62/psin.h"
#include "rage62/rpf9.h"

#include <iostream>

namespace {

void store_word(std::string& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes.at(offset + i) = static_cast<char>(value >> (i * 8));
}

std::string chunk(const char* tag, const std::string& body) {
    const auto size = static_cast<std::uint32_t>(body.size() + 8);
    std::string bytes(tag, 4);
    for (int shift : {24, 16, 8, 0}) bytes.push_back(static_cast<char>(size >> shift));
    return bytes + body;
}

bool write_file(const std::filesystem::path& path, const std::string& bytes) {
    std::ofstream stream(path, std::ios::binary);
    stream.write(bytes.data(), bytes.size());
    stream.close();
    return bool(stream);
}

bool check_schema(const std::filesystem::path& path, const std::filesystem::path& catalog_path,
                  std::string& error) {
    std::string types(32 * 2 + 16 * 3, '\0');
    types.replace(16, 8, 8, static_cast<char>(0xaa));
    types.replace(48, 8, 8, static_cast<char>(0xaa));
    store_word(types, 0, 0x101);
    store_word(types, 24, 8);
    store_word(types, 28, 0x00020004); // Only the upper half counts fields.
    store_word(types, 32, 0x202);
    store_word(types, 56, 12);
    store_word(types, 60, 0x00010020);
    store_word(types, 64, 0x303);
    store_word(types, 80, 0x404);
    store_word(types, 84, 4);
    store_word(types, 88, 0xabcdef01);
    store_word(types, 92, 0x505);
    store_word(types, 96, 0x606);

    std::string enumerations(24 * 2 + 16 * 2, '\0');
    enumerations.replace(8, 8, 8, static_cast<char>(0xee));
    enumerations.replace(32, 8, 8, static_cast<char>(0xee));
    store_word(enumerations, 0, 0x505);
    store_word(enumerations, 16, 1);
    store_word(enumerations, 24, 0x707);
    store_word(enumerations, 40, 1);
    store_word(enumerations, 48, 0xfffffffd);
    store_word(enumerations, 52, 0xffffffff);
    store_word(enumerations, 56, 0x808);
    store_word(enumerations, 64, 0x10203040);
    store_word(enumerations, 68, 1);

    const auto prefix = chunk("PSIN", "synthetic");
    const auto suffix = chunk("LSNS", "") + chunk("LGID", "") + chunk("LRSC", "") + chunk("CHKS", "");
    const auto valid = prefix + chunk("LSCH", types) + chunk("LENM", enumerations) + suffix;
    store_word(types, 60, 0xffff0000);
    const auto bad_schema = prefix + chunk("LSCH", types) + chunk("LENM", enumerations) + suffix;
    store_word(types, 60, 0x00010020);
    store_word(enumerations, 40, 0xffffffff);
    const auto bad_values = prefix + chunk("LSCH", types) + chunk("LENM", enumerations) + suffix;

    std::string image(1040, '\0');
    image.replace(0, 4, "9FPR");
    image[4] = 64;
    image[8] = 64;
    image[12] = 16;
    image[36] = 3;
    image.replace(64, valid.size(), valid);
    image.replace(384, bad_schema.size(), bad_schema);
    image.replace(704, bad_values.size(), bad_values);
    std::string catalog(72, '\0');
    catalog[8] = 64 / 16;
    catalog[32] = 384 / 16;
    catalog[56] = 704 / 16;

    rage62::Rpf9Archive archive;
    rage62::PsinResource resource;
    if (!write_file(path, image) || !write_file(catalog_path, catalog) || !archive.open(path, error) ||
        !archive.load_catalog(catalog_path, error) || !rage62::load_psin(archive, 0, valid.size(), resource, error)) {
        return false;
    }
    const auto& types_read = resource.structures;
    const auto& enums_read = resource.enums;
    if (types_read.size() != 2 || types_read[0].name_hash != 0x101 || types_read[0].size != 8 ||
        types_read[0].fields.size() != 2 || types_read[1].fields.size() != 1 ||
        types_read[1].fields[0].name_hash != 0x606 || enums_read.size() != 2 ||
        enums_read[0].name_hash != 0x505 || enums_read[0].values.size() != 1 ||
        enums_read[1].values.size() != 1) return false;
    const auto& field = types_read[0].fields[1];
    if (field.name_hash != 0x404 || field.offset != 4 || field.type_word != 0xabcdef01 || field.reference != 0x505 ||
        enums_read[0].values[0].value != -3 || enums_read[0].values[0].name_hash != 0x808 ||
        enums_read[1].values[0].value != 0x110203040ll) return false;

    return !rage62::load_psin(archive, 1, bad_schema.size(), resource, error) && error.find("LSCH") != std::string::npos &&
           !rage62::load_psin(archive, 2, bad_values.size(), resource, error) && error.find("LENM") != std::string::npos &&
           resource.data == valid;
}

bool check_resources(const std::filesystem::path& path, const std::filesystem::path& catalog_path,
                     const std::filesystem::path& bad_path, std::string& error) {
    constexpr char source[] = "PSIN\0\0\0\x0b" "abc"
                              "LSCH\0\0\0\x08"
                              "LENM\0\0\0\x08"
                              "LSNS\0\0\0\x08"
                              "LGID\0\0\0\x08"
                              "LRSC\0\0\0\x08"
                              "CHKS\0\0\0\x08";
    const std::string raw(source, sizeof(source) - 1);
    // The raw fixture encoded as one zlib member.
    constexpr char packed[] = "\x78\x9c\x0b\x08\xf6\xf4\x63\x60\x60\xe0\x4e\x4c\x4a\xf6"
                              "\x09\x76\xf6\x00\x32\x39\x7c\x5c\xfd\x7c\xc1\x74\xb0\x5f\x30"
                              "\x98\x76\xf7\x74\x01\xd3\x41\xc1\xce\x20\xda\xd9\xc3\x1b\x2c"
                              "\x0e\x00\x42\x19\x09\xaf";
    const auto packed_size = sizeof(packed) - 1;

    std::string image(288, '\0');
    image.replace(0, 4, "9FPR");
    image[4] = 64;
    image[8] = 64;
    image[12] = 16;
    image[36] = 2;
    image.replace(96, raw.size(), raw);
    image.replace(192, packed_size, packed, packed_size);

    std::string catalog(48, '\0');
    catalog[8] = 96 / 16;
    catalog[32] = 192 / 16;
    rage62::Rpf9Archive archive;
    rage62::PsinResource resource;
    if (!write_file(path, image) || !write_file(catalog_path, catalog) ||
        !archive.open(path, error) || !archive.load_catalog(catalog_path, error) ||
        !rage62::load_psin(archive, 0, raw.size(), resource, error)) return false;

    if (resource.data != raw || resource.chunks[0].size != 11 || resource.chunks[1].offset != 11 ||
        std::string(resource.chunks.back().tag.data(), 4) != "CHKS" || resource.chunks.back().offset != 51) {
        return false;
    }
    if (rage62::load_psin(archive, 0, 97, resource, error) || error.find("span") == std::string::npos ||
        rage62::load_psin(archive, 0, raw.size() - 1, resource, error) || error.find("truncated") == std::string::npos ||
        rage62::load_psin(archive, 0, raw.size() + 1, resource, error) || error.find("trailing") == std::string::npos ||
        resource.data != raw) return false;

#ifdef RAGE62_HAS_ZLIB
    if (!rage62::load_psin(archive, 1, packed_size, resource, error) || resource.data != raw ||
        rage62::load_psin(archive, 1, packed_size - 1, resource, error) || error.find("invalid") == std::string::npos ||
        rage62::load_psin(archive, 1, packed_size + 1, resource, error) || error.find("trailing") == std::string::npos) {
        return false;
    }
#else
    if (rage62::load_psin(archive, 1, packed_size, resource, error) || error.find("zlib") == std::string::npos) return false;
#endif

    image[96 + 11] = 'X';
    return write_file(bad_path, image) && archive.open(bad_path, error) && archive.load_catalog(catalog_path, error) &&
           !rage62::load_psin(archive, 0, raw.size(), resource, error) && error.find("chunk order") != std::string::npos;
}

}

int main() {
    const std::filesystem::path path = "rage62_psin_test.rpf";
    const std::filesystem::path catalog = "rage62_psin_test.catalog";
    const std::filesystem::path bad = "rage62_psin_test.bad";
    std::string error;
    const bool passed = check_resources(path, catalog, bad, error) && check_schema(path, catalog, error);
    std::filesystem::remove(path);
    std::filesystem::remove(catalog);
    std::filesystem::remove(bad);
    if (!passed) std::cerr << "PSIN resource loading check failed: " << error << '\n';
    return passed ? 0 : 1;
}
