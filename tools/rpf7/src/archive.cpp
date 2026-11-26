#include "rage62/archive.h"
#include <algorithm>
#include <array>
#include <utility>

#ifdef RAGE62_HAS_ZLIB
#include <zlib.h>
#endif

namespace rage62 {
namespace {
std::uint32_t word(const unsigned char* p, bool big) {
    if (big) return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) |
                    (std::uint32_t(p[2]) << 8) | p[3];
    return (std::uint32_t(p[3]) << 24) | (std::uint32_t(p[2]) << 16) |
           (std::uint32_t(p[1]) << 8) | p[0];
}

bool fail(std::string& error, const char* message) {
    error = message;
    return false;
}

bool read_header(std::ifstream& stream, ArchiveHeader& header, std::string& error) {
    const auto end = stream.tellg();
    if (end < 16) { error = "truncated header"; return false; }
    std::array<unsigned char, 16> bytes{};
    stream.seekg(0);
    if (!stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) {
        error = "cannot read header"; return false;
    }
    const bool little7 = bytes[0] == '7' && bytes[1] == 'F' && bytes[2] == 'P' && bytes[3] == 'R';
    const bool big7 = bytes[0] == 'R' && bytes[1] == 'P' && bytes[2] == 'F' && bytes[3] == '7';
    const bool nine = bytes[0] == '9' && bytes[1] == 'F' && bytes[2] == 'P' && bytes[3] == 'R';
    if (!little7 && !big7 && !nine) { error = "unknown RPF version"; return false; }
    ArchiveHeader result{nine ? ArchiveVersion::rpf9 : ArchiveVersion::rpf7,
                         static_cast<std::uint64_t>(end), word(bytes.data()+4, big7),
                         word(bytes.data()+8, big7), word(bytes.data()+12, big7), big7};
    if (nine && (result.first < 16 || result.first > result.file_size ||
                 result.third > result.file_size - result.first)) {
        error = "invalid RPF9 regions"; return false;
    }
    if (!nine && 16ull + 16ull * result.first + result.second > result.file_size) {
        error = "invalid RPF7 index size"; return false;
    }
    header = result;
    error.clear();
    return true;
}

constexpr std::uint64_t max_index_size = 64ull * 1024 * 1024;
constexpr std::uint64_t max_path_bytes = 64ull * 1024 * 1024;
constexpr std::uint32_t directory_tag = 0x7fffff00;
constexpr std::uint32_t open_index = 0x4e45504f;
constexpr std::uint32_t resource_bit = 0x80000000;

struct Rpf7Record {
    std::uint32_t name_offset;
    std::uint32_t first_child = 0;
    std::uint32_t child_count = 0;
    Rpf7Entry entry{};
};

bool decode_record(const unsigned char* bytes, std::ifstream& stream, std::uint64_t index_end,
                   std::uint64_t file_size, Rpf7Record& record, std::string& error) {
    const auto low = word(bytes, false);
    const auto high = word(bytes + 4, false);
    const auto third = word(bytes + 8, false);
    const auto fourth = word(bytes + 12, false);
    if (high == directory_tag) {
        record.entry.kind = Rpf7EntryKind::directory;
        record.name_offset = low;
        record.first_child = third;
        record.child_count = fourth;
        return true;
    }

    auto& entry = record.entry;
    record.name_offset = low & 0xffff;
    entry.offset = std::uint64_t((high >> 8) & 0x7fffff) * 512;
    entry.stored_size = (low >> 16) | ((high & 0xff) << 16);
    if (high & resource_bit) {
        entry.kind = Rpf7EntryKind::resource;
        if (entry.stored_size == 0xffffff) {
            std::array<unsigned char, 16> extended{};
            if (entry.offset < index_end || entry.offset > file_size ||
                file_size - entry.offset < extended.size() || !stream.seekg(static_cast<std::streamoff>(entry.offset)) ||
                !stream.read(reinterpret_cast<char*>(extended.data()), extended.size())) {
                return fail(error, "invalid extended RPF7 resource size");
            }
            entry.stored_size = std::uint32_t(extended[7]) | (std::uint32_t(extended[14]) << 8) |
                                (std::uint32_t(extended[5]) << 16) | (std::uint32_t(extended[2]) << 24);
            if (entry.stored_size < 0xffffff) {
                return fail(error, "invalid extended RPF7 resource size");
            }
        }
    } else {
        if (fourth > 1) return fail(error, "invalid RPF7 binary encryption flag");
        entry.kind = Rpf7EntryKind::binary;
        entry.decoded_size = third;
        entry.encrypted = fourth == 1;
        entry.compressed = entry.stored_size != 0;
        if (!entry.compressed) entry.stored_size = entry.decoded_size;
    }
    if (entry.offset > file_size || entry.stored_size > file_size - entry.offset ||
        (entry.stored_size != 0 && entry.offset < index_end)) {
        return fail(error, "RPF7 file extent lies outside the data region");
    }
    return true;
}

bool read_rpf7_index(std::ifstream& stream, const ArchiveHeader& header,
                     std::vector<Rpf7Entry>& result, std::string& error) {
    if (header.version != ArchiveVersion::rpf7) return fail(error, "expected RPF7 archive");
    if (header.big_endian) return fail(error, "RPF7 catalog requires little-endian PC format");
    if (header.third != 0 && header.third != open_index) {
        return fail(error, "unsupported RPF7 index mode (expected OPEN or NONE)");
    }
    const auto records_size = std::uint64_t(header.first) * 16;
    const auto index_size = records_size + header.second;
    if (header.first == 0 || header.second == 0) return fail(error, "RPF7 index has no root or names");
    if (index_size > max_index_size) return fail(error, "RPF7 index exceeds the 64 MiB reader limit");

    std::vector<unsigned char> index(static_cast<std::size_t>(index_size));
    if (!stream.read(reinterpret_cast<char*>(index.data()), index.size())) {
        return fail(error, "cannot read RPF7 index");
    }
    const auto* names = index.data() + records_size;
    std::vector<Rpf7Record> records(header.first);
    for (std::size_t i = 0; i < records.size(); ++i) {
        auto& record = records[i];
        if (!decode_record(index.data() + i * 16, stream, 16 + index_size, header.file_size, record, error)) {
            return false;
        }
        if (record.name_offset >= header.second) return fail(error, "invalid RPF7 name offset");
        if (record.entry.kind == Rpf7EntryKind::directory && record.child_count != 0 &&
            (record.first_child >= header.first || record.child_count > header.first - record.first_child)) {
            return fail(error, "invalid RPF7 directory child range");
        }
    }
    if (records[0].entry.kind != Rpf7EntryKind::directory) return fail(error, "RPF7 root must be a directory");
    const auto* names_end = names + header.second;
    if (std::find(names + records[0].name_offset, names_end, 0) == names_end) {
        return fail(error, "unterminated RPF7 root name");
    }

    std::vector<std::uint32_t> order{0};
    std::vector<bool> seen(header.first);
    seen[0] = true;
    std::uint64_t path_bytes = 0;
    for (std::size_t i = 0; i < order.size(); ++i) {
        const auto& parent = records[order[i]];
        for (std::uint32_t j = 0; j < parent.child_count; ++j) {
            const auto child = parent.first_child + j;
            if (seen[child]) return fail(error, "RPF7 entry has multiple parents or a cycle");
            seen[child] = true;
            order.push_back(child);
            auto& record = records[child];
            const auto* begin = names + record.name_offset;
            const auto* end = std::find(begin, names_end, 0);
            if (end == names_end) return fail(error, "unterminated RPF7 name");
            const auto& prefix = parent.entry.path;
            path_bytes += prefix.size() + (end - begin) + (prefix.empty() ? 0 : 1);
            if (path_bytes > max_path_bytes) return fail(error, "RPF7 paths exceed the 64 MiB reader limit");
            std::string name(begin, end);
            if (name.empty() || name == "." || name == ".." || name.find_first_of("/\\\r\n\t") != std::string::npos) {
                return fail(error, "invalid RPF7 path component");
            }
            record.entry.path = prefix.empty() ? std::move(name) : prefix + '/' + name;
        }
    }
    if (order.size() != records.size()) return fail(error, "RPF7 index contains unreachable entries");

    result.reserve(records.size() - 1);
    for (std::size_t i = 1; i < records.size(); ++i) result.push_back(std::move(records[i].entry));
    return true;
}

bool copy_deflate(std::ifstream& stream, const Rpf7Entry& entry, std::ostream& output,
                  std::string& error) {
#ifdef RAGE62_HAS_ZLIB
    z_stream decoder{};
    if (inflateInit2(&decoder, -MAX_WBITS) != Z_OK) return fail(error, "cannot initialize RPF7 inflater");
    struct InflateEnd {
        z_stream& decoder;
        ~InflateEnd() { inflateEnd(&decoder); }
    } cleanup{decoder};

    std::array<unsigned char, 32 * 1024> input{};
    std::array<unsigned char, 32 * 1024> decoded{};
    auto remaining = entry.stored_size;
    auto decoded_remaining = entry.decoded_size;
    while (true) {
        if (decoder.avail_in == 0 && remaining != 0) {
            const auto length = std::min<std::size_t>(remaining, input.size());
            if (!stream.read(reinterpret_cast<char*>(input.data()), length)) {
                return fail(error, "cannot read compressed RPF7 file data");
            }
            decoder.next_in = input.data();
            decoder.avail_in = static_cast<uInt>(length);
            remaining -= static_cast<std::uint32_t>(length);
        }
        decoder.next_out = decoded.data();
        decoder.avail_out = static_cast<uInt>(decoded.size());
        const auto status = inflate(&decoder, Z_NO_FLUSH);
        if (status != Z_OK && status != Z_STREAM_END) return fail(error, "invalid RPF7 DEFLATE stream");
        const auto length = decoded.size() - decoder.avail_out;
        if (length > decoded_remaining) return fail(error, "RPF7 decoded size exceeds the catalog size");
        decoded_remaining -= static_cast<std::uint32_t>(length);
        if (status == Z_STREAM_END && (decoded_remaining != 0 || remaining != 0 || decoder.avail_in != 0)) {
            return fail(error, "RPF7 compressed or decoded size does not match the catalog");
        }
        if (!output.write(reinterpret_cast<const char*>(decoded.data()), length)) {
            return fail(error, "cannot write RPF7 file data");
        }
        if (status == Z_STREAM_END) return true;
    }
#else
    (void)stream;
    (void)entry;
    (void)output;
    return fail(error, "reading compressed RPF7 entries requires a build with zlib");
#endif
}
}

bool read_archive_header(const std::filesystem::path& path, ArchiveHeader& header, std::string& error) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) return fail(error, "cannot open archive");
    return read_header(stream, header, error);
}

bool Rpf7Archive::open(const std::filesystem::path& path, std::string& error) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) return fail(error, "cannot open archive");
    ArchiveHeader header{};
    std::vector<Rpf7Entry> entries;
    if (!read_header(stream, header, error) || !read_rpf7_index(stream, header, entries, error)) return false;
    stream_ = std::move(stream);
    entries_ = std::move(entries);
    error.clear();
    return true;
}

bool Rpf7Archive::copy_file(const std::string& path, std::ostream& output, std::string& error) {
    if (!stream_.is_open()) return fail(error, "no RPF7 archive is open");
    const auto same_path = [&](const Rpf7Entry& item) {
        return item.path == path;
    };
    const auto entry = std::find_if(entries_.begin(), entries_.end(), same_path);
    if (entry == entries_.end()) return fail(error, "RPF7 entry not found");
    if (std::find_if(entry + 1, entries_.end(), same_path) != entries_.end()) {
        return fail(error, "RPF7 path is ambiguous");
    }
    if (entry->kind != Rpf7EntryKind::binary) return fail(error, "RPF7 entry is not a binary file");
    if (entry->encrypted) return fail(error, "RPF7 entry is encrypted");

    stream_.clear();
    if (!stream_.seekg(static_cast<std::streamoff>(entry->offset))) return fail(error, "cannot seek to RPF7 file");
    if (entry->compressed) {
        if (!copy_deflate(stream_, *entry, output, error)) return false;
        error.clear();
        return true;
    }
    std::array<char, 64 * 1024> buffer{};
    auto remaining = entry->stored_size;
    while (remaining != 0) {
        const auto length = std::min<std::size_t>(remaining, buffer.size());
        if (!stream_.read(buffer.data(), length)) return fail(error, "cannot read RPF7 file data");
        if (!output.write(buffer.data(), length)) return fail(error, "cannot write RPF7 file data");
        remaining -= static_cast<std::uint32_t>(length);
    }
    error.clear();
    return true;
}
}
