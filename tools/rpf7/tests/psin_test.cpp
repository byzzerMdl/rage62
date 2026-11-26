#include "rage62/psin.h"
#include "rage62/rpf9.h"

#include <iostream>

namespace {

bool write_file(const std::filesystem::path& path, const std::string& bytes) {
    std::ofstream stream(path, std::ios::binary);
    stream.write(bytes.data(), bytes.size());
    stream.close();
    return bool(stream);
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
    const bool passed = check_resources(path, catalog, bad, error);
    std::filesystem::remove(path);
    std::filesystem::remove(catalog);
    std::filesystem::remove(bad);
    if (!passed) std::cerr << "PSIN resource loading check failed: " << error << '\n';
    return passed ? 0 : 1;
}
