#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace rage62 {
enum class ArchiveVersion { rpf7, rpf9 };
struct ArchiveHeader {
    ArchiveVersion version;
    std::uint64_t file_size;
    std::uint32_t first;
    std::uint32_t second;
    std::uint32_t third;
    bool big_endian;
};
bool read_archive_header(const std::filesystem::path& path, ArchiveHeader& header, std::string& error);

enum class Rpf7EntryKind { directory, binary, resource };

struct Rpf7Entry {
    std::string path;
    Rpf7EntryKind kind;
    std::uint64_t offset;
    std::uint32_t stored_size;
    std::uint32_t decoded_size;
    bool compressed;
    bool encrypted;
};

class Rpf7Archive {
public:
    // Reads the directory of a little-endian RPF7 with an OPEN or NONE index.
    bool open(const std::filesystem::path& path, std::string& error);
    const std::vector<Rpf7Entry>& entries() const { return entries_; }

    // Streams an unencrypted binary entry to output. Compressed entries require zlib.
    // Paths use '/' and match the spelling in the catalog.
    bool copy_file(const std::string& path, std::ostream& output, std::string& error);

private:
    std::ifstream stream_;
    std::vector<Rpf7Entry> entries_;
};
}
