#include "rage62/archive.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {
void set_word(std::vector<unsigned char>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes.at(offset + i) = static_cast<unsigned char>(value >> (8 * i));
}

void set_entry(std::vector<unsigned char>& bytes, std::size_t entry, std::uint32_t first,
               std::uint32_t second, std::uint32_t third, std::uint32_t fourth) {
    const auto offset = 16 + entry * 16;
    set_word(bytes, offset, first);
    set_word(bytes, offset + 4, second);
    set_word(bytes, offset + 8, third);
    set_word(bytes, offset + 12, fourth);
}

constexpr char names[] = "\0mods\0mesh.ydr\0empty.bin\0sealed.bin\0bytes.bin\0packed.txt";
constexpr std::size_t name_table = 16 + 7 * 16;

std::vector<unsigned char> rpf7() {
    std::vector<unsigned char> bytes(2560);
    set_word(bytes, 0, 0x52504637);
    set_word(bytes, 4, 7);
    set_word(bytes, 8, sizeof(names));
    set_word(bytes, 12, 0x4e45504f);
    set_entry(bytes, 0, 0, 0x7fffff00, 1, 4);
    set_entry(bytes, 1, 1, 0x7fffff00, 5, 2);
    set_entry(bytes, 2, (16u << 16) | 6, 0x80000300, 0, 0);
    set_entry(bytes, 3, 15, 0, 0, 0);
    set_entry(bytes, 4, 25, 0x400, 1, 1);
    set_entry(bytes, 5, 36, 0x100, 5, 0);
    set_entry(bytes, 6, (7u << 16) | 46, 0x200, 5, 0);

    for (std::size_t i = 0; i < sizeof(names); ++i) bytes[name_table + i] = names[i];
    const unsigned char raw[] = {0, 10, 13, 26, 255};
    for (std::size_t i = 0; i < sizeof(raw); ++i) bytes[512 + i] = raw[i];
    const unsigned char packed[] = {0xcb, 0x48, 0xcd, 0xc9, 0xc9, 0x07, 0x00}; // Raw DEFLATE for "hello".
    for (std::size_t i = 0; i < sizeof(packed); ++i) bytes[1024 + i] = packed[i];
    return bytes;
}

bool write(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    stream.close();
    return bool(stream);
}

bool test_rpf7(const std::filesystem::path& valid, const std::filesystem::path& invalid) {
    auto bytes = rpf7();
    if (!write(valid, bytes)) return false;
    rage62::Rpf7Archive archive;
    std::string error;
    if (!archive.open(valid, error)) {
        std::cerr << error << '\n';
        return false;
    }
    const auto& entries = archive.entries();
    using Kind = rage62::Rpf7EntryKind;
    if (entries.size() != 6 || entries[0].kind != Kind::directory || entries[1].kind != Kind::resource ||
        entries[4].path != "mods/bytes.bin" || entries[4].offset != 512 || entries[4].stored_size != 5 ||
        entries[5].path != "mods/packed.txt" || !entries[5].compressed) return false;

    std::ostringstream output;
    if (!archive.copy_file("mods/bytes.bin", output, error) || output.str() != std::string("\0\n\r\x1a\xff", 5) ||
        !archive.copy_file("empty.bin", output, error) || output.str().size() != 5 ||
        archive.copy_file("mesh.ydr", output, error) || archive.copy_file("sealed.bin", output, error) ||
        archive.copy_file("missing.bin", output, error) || output.str().size() != 5) return false;

    std::ostringstream decoded;
#ifdef RAGE62_HAS_ZLIB
    if (!archive.copy_file("mods/packed.txt", decoded, error) || decoded.str() != "hello") return false;
#else
    if (archive.copy_file("mods/packed.txt", decoded, error) || error.find("zlib") == std::string::npos) return false;
#endif

    auto rejects = [&](std::size_t offset, std::uint32_t value, const std::string& diagnostic) {
        auto malformed = bytes;
        set_word(malformed, offset, value);
        return write(invalid, malformed) && !archive.open(invalid, error) && error.find(diagnostic) != std::string::npos;
    };
    if (!rejects(12, 0x0fefffff, "index mode") || !rejects(16 + 8, 0, "cycle") ||
        !rejects(16 + 12, 2, "unreachable") || !rejects(32 + 12, 4, "child range") ||
        !rejects(16 + 5 * 16, 65535, "name offset") || !rejects(16 + 5 * 16 + 4, 0x500, "data region")) return false;
    if (!archive.copy_file("mods/bytes.bin", output, error) || output.str().size() != 10) return false;

    set_word(bytes, 12, 0); // NONE also leaves the catalog clear.
    if (!write(invalid, bytes) || !archive.open(invalid, error)) return false;
#ifdef RAGE62_HAS_ZLIB
    auto altered = valid;
    altered += ".bad-deflate";
    set_word(bytes, 16 + 6 * 16 + 8, 4);
    bool result = false;
    {
        rage62::Rpf7Archive damaged;
        std::ostringstream discarded;
        result = write(altered, bytes) && damaged.open(altered, error) &&
                 !damaged.copy_file("mods/packed.txt", discarded, error) && error.find("decoded size") != std::string::npos;
    }
    std::filesystem::remove(altered);
    if (!result) return false;
#endif
    return true;
}
}

int main() {
    const auto path = std::filesystem::current_path() / "rage62_header_test.rpf";
    rage62::ArchiveHeader header{};
    std::string error;
    { std::ofstream f(path, std::ios::binary); const char bytes[16] = {'9','F','P','R',16}; f.write(bytes, 16); }
    if (rage62::read_archive_header(path, header, error)) return 1;
    { std::ofstream f(path, std::ios::binary | std::ios::trunc); f << "bad"; }
    if (rage62::read_archive_header(path, header, error)) return 2;
    const auto valid = std::filesystem::current_path() / "rage62_rpf7_test.rpf";
    const bool passed = test_rpf7(valid, path);
    std::filesystem::remove(valid);
    std::filesystem::remove(path);
    if (!passed) std::cerr << "RPF7 catalog or binary copy check failed\n";
    return passed ? 0 : 3;
}
