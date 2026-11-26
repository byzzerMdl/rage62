#include "rage62/archive.h"
#include "rage62/rpf9.h"

#include <iostream>
#include <sstream>

namespace {

void put(std::string& bytes, std::size_t at, std::uint32_t word) {
    for (unsigned i = 0; i < 4; ++i) bytes.at(at + i) = static_cast<char>((word >> (i * 8)) & 0xff);
}

bool write_file(const std::filesystem::path& path, const std::string& bytes) {
    std::ofstream output(path, std::ios::binary);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    return bool(output);
}

bool run(const std::filesystem::path& archive_path, const std::filesystem::path& catalog_path,
         const std::filesystem::path& bad_path) {
    std::string fixture(240, 'T');
    fixture.replace(0, 64, 64, '\0');
    fixture.replace(0, 4, "9FPR");
    put(fixture, 4, 112);
    put(fixture, 8, 96);
    put(fixture, 12, 16);
    put(fixture, 16, 0x10203040);
    put(fixture, 20, 0x50607080);
    put(fixture, 32, 0x10003);
    put(fixture, 36, 3);
    put(fixture, 44, 0x90f01);
    fixture.replace(128, 64, 64, 'A');
    fixture.replace(192, 32, 32, 'B');

    std::string catalog(3 * 24, '\0');
    put(catalog, 0, 13);
    put(catalog, 8, 12);
    put(catalog, 12, 0xff00);
    put(catalog, 16, 0x123);
    put(catalog, 24 + 8, 8);
    put(catalog, 48 + 8, 8);

    std::string error;
    rage62::ArchiveHeader generic;
    rage62::Rpf9Archive archive;
    if (!write_file(archive_path, fixture) || !write_file(catalog_path, catalog) ||
        !rage62::read_archive_header(archive_path, generic, error) || generic.version != rage62::ArchiveVersion::rpf9 ||
        !archive.open(archive_path, error) || !archive.entries().empty() || !archive.load_catalog(catalog_path, error)) {
        std::cerr << error << '\n';
        return false;
    }

    const auto& header = archive.header();
    if (header.file_size != 240 || header.payload_start != 112 || header.table_split != 96 ||
        header.payload_end != 224 || header.trailer_size != 16 || header.entry_count != 3 ||
        header.archive_id != 0x5060708010203040ull || header.layout_word != 0x10003 || header.flags != 0x90f01) {
        return false;
    }

    const auto& entries = archive.entries();
    if (entries.size() != 3 || entries[0].offset != 192 || entries[0].span != 32 ||
        entries[0].size_hint != 13 || entries[0].identifier != 0x123 || entries[0].flags != 0xff00 ||
        entries[1].offset != 128 || entries[1].span != 64 || entries[2].span != 64) return false;

    auto rejects_catalog = [&](const std::string& damaged, const char* diagnostic) {
        return write_file(bad_path, damaged) && !archive.load_catalog(bad_path, error) &&
               error.find(diagnostic) != std::string::npos;
    };
    if (!rejects_catalog(catalog.substr(1), "catalog length")) return false;
    auto damaged = catalog;
    put(damaged, 4, 1);
    if (!rejects_catalog(damaged, "location record layout")) return false;
    damaged = catalog;
    put(damaged, 8, 224 / 16);
    if (!rejects_catalog(damaged, "payload region")) return false;
    put(damaged, 8, 96 / 16);
    if (!rejects_catalog(damaged, "payload region")) return false;

    auto rejects_header = [&](std::size_t at, std::uint32_t value) {
        auto invalid = fixture;
        put(invalid, at, value);
        return write_file(bad_path, invalid) && !archive.open(bad_path, error) &&
               error.find("region boundaries") != std::string::npos;
    };
    if (!rejects_header(8, 113) || !rejects_header(8, 48) || !rejects_header(8, 81) ||
        !rejects_header(4, 256) || !rejects_header(12, 129)) return false;

    std::ostringstream output;
    if (!archive.copy_span(0, output, error) || !archive.copy_span(2, output, error) ||
        archive.copy_span(3, output, error) || output.str() != std::string(32, 'B') + std::string(64, 'A')) {
        return false;
    }

    return archive.open(archive_path, error) && archive.entries().empty() && !archive.copy_span(0, output, error);
}

}

int main() {
    const std::filesystem::path archive = "rage62_rpf9_test.rpf";
    const std::filesystem::path catalog = "rage62_rpf9_test.catalog";
    const std::filesystem::path bad = "rage62_rpf9_test.bad";
    const bool passed = run(archive, catalog, bad);
    std::filesystem::remove(archive);
    std::filesystem::remove(catalog);
    std::filesystem::remove(bad);
    if (!passed) std::cerr << "RPF9 header, catalog or span check failed\n";
    return passed ? 0 : 1;
}
