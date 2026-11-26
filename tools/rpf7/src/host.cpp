#include "rage62/psin.h"
#include "rage62/resource9.h"
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

bool read_table(const std::filesystem::path& path, std::string& content, std::string& error) {
    constexpr std::streamoff max_table_size = 1024 * 1024;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error = "cannot open plain chunk table";
        return false;
    }
    const auto size = file.tellg();
    if (size < 0 || size > max_table_size) {
        error = "cannot size chunk table or table exceeds 1 MiB";
        return false;
    }
    content.resize(static_cast<std::size_t>(size));
    if (!file.seekg(0) || !file.read(content.data(), size)) {
        error = "cannot read plain chunk table";
        return false;
    }
    return true;
}

int startup(const std::filesystem::path& archive_path, const std::filesystem::path& catalog_path,
            std::size_t entry, std::uint64_t stored_size, const std::filesystem::path& chunk_table, bool paged) {
    rage62::Rpf9Archive archive;
    std::string error;
    if (!archive.open(archive_path, error) || !archive.load_catalog(catalog_path, error)) {
        std::cerr << "archive startup: " << error << '\n';
        return 1;
    }
    std::string table;
    if (!chunk_table.empty() && !read_table(chunk_table, table, error)) {
        std::cerr << "chunk table: " << error << '\n';
        return 1;
    }
    if (paged) {
        rage62::Rage9Resource resource;
        if (!resource.load(archive, entry, stored_size, table, error)) {
            std::cerr << "resource load: " << error << '\n';
            return 1;
        }
        std::cout << "RAGE 9 resource loaded entry=" << entry << " map=" << resource.page_map_offset()
                  << " system_bytes=" << resource.system_size() << " graphics_bytes=" << resource.graphics_size()
                  << " system_pages=" << resource.system_page_count() << " graphics_pages=" << resource.graphics_page_count();
        if (resource.is_0drp()) std::cout << " signature=0DRP";
        std::cout << '\n';
        return std::cout ? 0 : 1;
    }

    rage62::PsinResource resource;
    const auto loaded = chunk_table.empty() ? rage62::load_psin(archive, entry, stored_size, resource, error) :
                                             rage62::load_chunked_psin(archive, entry, stored_size, table, resource, error);
    if (!loaded) {
        std::cerr << "PSIN load: " << error << '\n';
        return 1;
    }

    std::cout << "PSIN loaded entry=" << entry << " bytes=" << resource.data.size() << '\n';
    for (const auto& chunk : resource.chunks) {
        std::cout.write(chunk.tag.data(), chunk.tag.size());
        std::cout << " offset=" << chunk.offset << " length=" << chunk.size << '\n';
    }
    std::cout << "schema structures=" << resource.structures.size() << " enums=" << resource.enums.size() << '\n';
    std::cout << "data blocks=" << resource.blocks.size() << " opaque schema values=" << resource.schema_blocks.size() << '\n';
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
    const bool paged = argc > 1 && std::filesystem::path(argv[1]) == "--resource";
    const int base = paged ? 2 : 1;
    const int arguments = argc - base;
    if ((paged ? arguments != 5 : arguments != 4 && arguments != 5) ||
        !parse_number(std::filesystem::path(argv[base + 2]).u8string(), entry) ||
        !parse_number(std::filesystem::path(argv[base + 3]).u8string(), stored_size)) {
        std::cerr << "usage: rage62_host <rpf9-archive> <plain-catalog> <entry-index> <stored-bytes> [plain-chunk-table]\n"
                  << "       rage62_host --resource <rpf9-archive> <plain-catalog> <entry-index> <stored-bytes> <plain-chunk-table>\n";
        return 2;
    }
    return startup(argv[base], argv[base + 1], entry, stored_size,
                   arguments == 5 ? std::filesystem::path(argv[base + 4]) : std::filesystem::path{}, paged);
}
