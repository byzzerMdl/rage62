#include "rage62/rpf9.h"
#include "rage62/rpf9_chunks.h"

#include <iostream>

namespace {

std::string table(unsigned encoding, std::uint64_t first, std::uint64_t second) {
    std::string bytes(32, '\0');
    bytes[0] = 16; // Records begin after a gap following the header.
    bytes[4] = 2;
    bytes[7] = static_cast<char>(encoding);
    for (unsigned i = 0; i < 8; ++i) {
        bytes[16 + i] = static_cast<char>((first >> (8 * i)) & 0xff);
        bytes[24 + i] = static_cast<char>((second >> (8 * i)) & 0xff);
    }
    return bytes;
}

bool write_file(const std::filesystem::path& path, const std::string& bytes) {
    std::ofstream output(path, std::ios::binary);
    output.write(bytes.data(), bytes.size());
    output.close();
    return bool(output);
}

bool run(const std::filesystem::path& image_path, const std::filesystem::path& index_path, std::string& error) {
    const std::string first(16, 'A');
    const std::string last(16, 'B');
    constexpr char compressed[] = "\x78\x9c\x73\x74\x44\x05\x00\x22\x98\x04\x11";
    std::string image(208, 'T');
    image.replace(0, 64, 64, '\0');
    image.replace(0, 4, "9FPR");
    image[4] = 64;
    image[8] = 64;
    image[12] = 16;
    image[36] = 3;
    image.replace(64, 16, 16, '\0');
    image.replace(80, 16, first);
    image.replace(96, 16, last);
    image.replace(112, 16, first);
    image.replace(128, 16, last);
    image.replace(144, 16, 16, '\0');
    image.replace(160, 11, compressed, 11);
    image.replace(176, 16, last);
    std::string index(72, '\0');
    index[8] = 64 / 16;
    index[32] = 112 / 16;
    index[56] = 144 / 16;

    rage62::Rpf9Archive archive;
    if (!write_file(image_path, image) || !write_file(index_path, index) ||
        !archive.open(image_path, error) || !archive.load_catalog(index_path, error)) return false;

    const auto layout2 = table(2, 0x0000000100000001ull, 0x8000000100000001ull);
    const auto layout3 = table(3, 0x00000000fff0fff0ull, 0x10000000fff0fff0ull);
    const auto layout4 = table(4, 0x0000000010000010ull, 0x0200000010000010ull);
    const auto mixed = table(2, 0xd400000100000001ull, 0x0000000100000001ull);
    std::string result;
    if (!rage62::read_rpf9_chunks(archive, 0, 48, layout2, result, error) || result != first + last ||
        !rage62::read_rpf9_chunks(archive, 1, 32, layout3, result, error) || result != first + last ||
        !rage62::read_rpf9_chunks(archive, 1, 32, layout4, result, error) || result != first + last) return false;

    auto rejects = [&](std::size_t entry, std::uint64_t stored, std::string_view bad, const char* diagnostic) {
        return !rage62::read_rpf9_chunks(archive, entry, stored, bad, result, error) &&
               error.find(diagnostic) != std::string::npos && result == first + last;
    };
    if (!rejects(0, 47, layout2, "declared sizes") || !rejects(1, 31, layout4, "declared sizes") ||
        !rejects(0, 48, layout2.substr(0, 31), "record range") ||
        !rejects(1, 32, table(5, 0, 0), "unsupported") ||
        !rejects(1, 32, table(4, 0x0010000010000010ull, 0x10000010), "copy chunk size") ||
        !rejects(0, 48, layout4, "cover")) return false;

#ifdef RAGE62_HAS_ZLIB
    return rage62::read_rpf9_chunks(archive, 2, 48, mixed, result, error) && result == first + last &&
           rejects(2, 48, table(2, 0xd400000100000002ull, 0x0000000100000001ull), "decoded chunk size");
#else
    return rejects(2, 48, mixed, "zlib");
#endif
}

}

int main() {
    const std::filesystem::path image = "rage62_chunks_test.rpf";
    const std::filesystem::path index = "rage62_chunks_test.catalog";
    std::string error;
    const bool passed = run(image, index, error);
    std::filesystem::remove(image);
    std::filesystem::remove(index);
    if (!passed) std::cerr << "RPF9 chunk payload check failed: " << error << '\n';
    return passed ? 0 : 1;
}
