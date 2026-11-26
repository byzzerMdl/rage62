#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rage62 {

class Rpf9Archive;

struct Rage9Page {
    std::size_t offset; // In the decoded entry, before splitting it into segments.
    std::size_t size;
    std::size_t chunks;
};

struct Rage9DirectoryRoot {
    std::uint64_t word_20;
    std::array<std::uint64_t, 4> pointers;
    std::array<std::uint16_t, 4> fields;

    std::size_t record_count() const { return fields[2]; }
};

struct Rage9DirectoryRecord {
    std::uint32_t word_0;
    std::uint32_t size_hint;
    std::uint64_t pointer;
};

class Rage9Resource {
public:
    bool load(Rpf9Archive& archive, std::size_t entry, std::uint64_t stored_size,
              std::string_view table, std::string& error);

    std::size_t system_size() const { return system_size_; }
    std::size_t graphics_size() const { return bytes_.size() - system_size_; }
    std::size_t system_page_count() const { return system_pages_; }
    std::size_t graphics_page_count() const { return pages_.size() - system_pages_; }
    std::size_t page_map_offset() const { return page_map_offset_; }
    bool is_0drp() const { return system_size_ >= 20 && bytes_.compare(16, 4, "0DRP") == 0; }
    const std::vector<Rage9Page>& pages() const { return pages_; }

    // Resolves a stored resource pointer, returning a view owned by this resource.
    std::optional<std::string_view> resolve(std::uint64_t pointer, std::size_t length) const;

    bool read_0drp_root(Rage9DirectoryRoot& root, std::string& error) const;
    bool read_0drp_record(std::size_t index, Rage9DirectoryRecord& record, std::string& error) const;

private:
    std::string bytes_;
    std::vector<Rage9Page> pages_;
    std::size_t system_pages_ = 0;
    std::size_t system_size_ = 0;
    std::size_t page_map_offset_ = 0;
};

}
