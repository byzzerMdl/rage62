#include "rage62/psin.h"
#include "rage62/rpf9.h"

#include <charconv>
#include <iostream>

namespace {

template <typename Number>
bool parse_number(const std::string& text, Number& result) {
    const auto* end = text.data() + text.size();
    const auto parsed = std::from_chars(text.data(), end, result);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

int startup(const std::filesystem::path& archive_path, const std::filesystem::path& catalog_path,
            std::size_t entry, std::uint64_t stored_size) {
    rage62::Rpf9Archive archive;
    std::string error;
    if (!archive.open(archive_path, error) || !archive.load_catalog(catalog_path, error)) {
        std::cerr << "archive startup: " << error << '\n';
        return 1;
    }
    rage62::PsinResource resource;
    if (!rage62::load_psin(archive, entry, stored_size, resource, error)) {
        std::cerr << "PSIN load: " << error << '\n';
        return 1;
    }

    std::cout << "PSIN loaded entry=" << entry << " bytes=" << resource.data.size() << '\n';
    for (const auto& chunk : resource.chunks) {
        std::cout.write(chunk.tag.data(), chunk.tag.size());
        std::cout << " offset=" << chunk.offset << " length=" << chunk.size << '\n';
    }
    std::cout << "schema structures=" << resource.structures.size() << " enums=" << resource.enums.size() << '\n';
    std::cout.flush();
    return std::cout ? 0 : 1;
}

}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    std::size_t entry = 0;
    std::uint64_t stored_size = 0;
    if (argc != 5 || !parse_number(std::filesystem::path(argv[3]).u8string(), entry) ||
        !parse_number(std::filesystem::path(argv[4]).u8string(), stored_size)) {
        std::cerr << "usage: rage62_host <rpf9-archive> <plain-catalog> <entry-index> <stored-bytes>\n";
        return 2;
    }
    return startup(argv[1], argv[2], entry, stored_size);
}
