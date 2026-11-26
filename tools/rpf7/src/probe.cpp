#include "rage62/archive.h"
#include "rage62/rpf9.h"
#include <charconv>
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

bool binary_stdout() {
#ifdef _WIN32
    return _setmode(_fileno(stdout), _O_BINARY) != -1;
#else
    return true;
#endif
}

int finish_output() {
    std::cout.flush();
    return std::cout ? 0 : report("cannot write to stdout");
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
    return finish_output();
}

int cat(const std::filesystem::path& path, const std::string& entry) {
    rage62::Rpf7Archive archive;
    std::string error;
    if (!archive.open(path, error)) return report(error);
    if (!binary_stdout()) return report("cannot set binary output mode");
    if (!archive.copy_file(entry, std::cout, error)) return report(error);
    return finish_output();
}

int rpf9_info(const std::filesystem::path& path) {
    rage62::Rpf9Archive archive;
    std::string error;
    if (!archive.open(path, error)) return report(error);
    const auto& header = archive.header();
    std::cout << "RPF9 size=" << header.file_size << " entries=" << header.entry_count << '\n'
              << "toc_start=64 toc_split=" << header.table_split << " toc_end=" << header.payload_start << '\n'
              << "payload_start=" << header.payload_start << " payload_end=" << header.payload_end
              << " trailer_size=" << header.trailer_size << '\n'
              << std::hex << "archive_id=0x" << header.archive_id << " layout_word=0x" << header.layout_word
              << " flags=0x" << header.flags << std::dec << '\n';
    return finish_output();
}

int rpf9_entries(const std::filesystem::path& path, const std::filesystem::path& table,
                 const std::string* selection = nullptr) {
    std::size_t index = 0;
    if (selection) {
        const auto* end = selection->data() + selection->size();
        const auto result = std::from_chars(selection->data(), end, index);
        if (result.ec != std::errc{} || result.ptr != end) return report("invalid RPF9 entry index");
    }

    rage62::Rpf9Archive archive;
    std::string error;
    if (!archive.open(path, error) || !archive.load_catalog(table, error)) return report(error);
    if (selection) {
        if (!binary_stdout()) return report("cannot set binary output mode");
        if (!archive.copy_span(index, std::cout, error)) return report(error);
    } else {
        std::cout << "index\toffset\tspan\tsize_hint\tflags\tidentifier\n";
        for (const auto& entry : archive.entries()) {
            std::cout << index++ << '\t' << entry.offset << '\t' << entry.span << '\t' << entry.size_hint << '\t'
                      << std::hex << "0x" << entry.flags << "\t0x" << entry.identifier << std::dec << '\n';
        }
    }
    return finish_output();
}
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    if (argc == 2) return probe(argv[1]);
    if (argc == 3 && std::filesystem::path(argv[1]) == "--list") return list(argv[2]);
    if (argc == 3 && std::filesystem::path(argv[1]) == "--rpf9-info") return rpf9_info(argv[2]);
    if (argc == 4 && std::filesystem::path(argv[1]) == "--cat") {
        return cat(argv[2], std::filesystem::path(argv[3]).u8string());
    }
    if (argc == 4 && std::filesystem::path(argv[1]) == "--rpf9-list") return rpf9_entries(argv[2], argv[3]);
    if (argc == 5 && std::filesystem::path(argv[1]) == "--rpf9-copy-span") {
        const auto index = std::filesystem::path(argv[4]).u8string();
        return rpf9_entries(argv[2], argv[3], &index);
    }
    std::cerr << "usage: rage62_rpf <archive>\n"
              << "       rage62_rpf --list <archive>\n"
              << "       rage62_rpf --cat <archive> <entry/path>\n"
              << "       rage62_rpf --rpf9-info <archive>\n"
              << "       rage62_rpf --rpf9-list <archive> <plain-catalog>\n"
              << "       rage62_rpf --rpf9-copy-span <archive> <plain-catalog> <index>\n";
    return 2;
}
