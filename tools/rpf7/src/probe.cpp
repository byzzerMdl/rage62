#include "rage62/archive.h"
#include <iostream>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {
int report(const std::string& error) {
    std::cerr << error << '\n';
    return 1;
}

int probe(const std::filesystem::path& path) {
    rage62::ArchiveHeader header{};
    std::string error;
    if (!rage62::read_archive_header(path, header, error)) return report(error);
    std::cout << (header.version == rage62::ArchiveVersion::rpf7 ? "RPF7" : "RPF9")
              << " size=" << header.file_size << " field1=" << header.first << '\n';
    return 0;
}

int list(const std::filesystem::path& path) {
    rage62::Rpf7Archive archive;
    std::string error;
    if (!archive.open(path, error)) return report(error);
    for (const auto& entry : archive.entries()) {
        using Kind = rage62::Rpf7EntryKind;
        const char* kind = "directory";
        if (entry.kind == Kind::binary) kind = "binary";
        if (entry.kind == Kind::resource) kind = "resource";
        std::cout << kind << '\t' << entry.path << '\t' << entry.stored_size;
        if (entry.compressed) std::cout << "\tcompressed";
        if (entry.encrypted) std::cout << "\tencrypted";
        std::cout << '\n';
    }
    std::cout.flush();
    return std::cout ? 0 : report("cannot write RPF7 catalog");
}

int cat(const std::filesystem::path& path, const std::string& entry) {
    rage62::Rpf7Archive archive;
    std::string error;
    if (!archive.open(path, error)) return report(error);
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return report("cannot set binary output mode");
#endif
    if (!archive.copy_file(entry, std::cout, error)) return report(error);
    std::cout.flush();
    return std::cout ? 0 : report("cannot write RPF7 file data");
}
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    if (argc == 2) return probe(argv[1]);
    if (argc == 3 && std::filesystem::path(argv[1]) == "--list") return list(argv[2]);
    if (argc == 4 && std::filesystem::path(argv[1]) == "--cat") {
        return cat(argv[2], std::filesystem::path(argv[3]).u8string());
    }
    std::cerr << "usage: rage62_rpf <archive>\n"
              << "       rage62_rpf --list <archive>\n"
              << "       rage62_rpf --cat <archive> <entry/path>\n";
    return 2;
}
