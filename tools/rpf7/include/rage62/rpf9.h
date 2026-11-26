#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace rage62 {

struct Rpf9Header {
    std::uint64_t file_size;
    std::uint64_t payload_end;
    std::uint32_t payload_start;
    std::uint32_t table_split;
    std::uint32_t trailer_size;
    std::uint64_t archive_id;
    std::uint32_t layout_word;
    std::uint32_t entry_count;
    std::uint32_t flags;
};

struct Rpf9Entry {
    std::uint64_t offset;
    std::uint64_t span; // Physical range, which can include alignment padding.
    std::uint32_t size_hint;
    std::uint32_t flags;
    std::uint32_t identifier;
};

class Rpf9Archive {
public:
    bool open(const std::filesystem::path& path, std::string& error);
    const Rpf9Header& header() const { return header_; }

    // Loads a caller-provided table of plain 24-byte location records.
    bool load_catalog(const std::filesystem::path& path, std::string& error);
    const std::vector<Rpf9Entry>& entries() const { return entries_; }

    bool copy_span(std::size_t index, std::ostream& output, std::string& error);

private:
    std::ifstream stream_;
    Rpf9Header header_{};
    std::vector<Rpf9Entry> entries_;
};

}
