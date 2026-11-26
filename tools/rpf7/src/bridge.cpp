#include "rage62/rpf7.h"
#include "rage62/archive.h"
#include "bridge_error.h"

#include <exception>
#include <memory>
#include <ostream>
#include <streambuf>

struct rage62_rpf7 {
    rage62::Rpf7Archive archive;
};

namespace {
using rage62::detail::set_error;

class SinkBuffer : public std::streambuf {
public:
    SinkBuffer(rage62_rpf7_sink sink, void* context) : sink_(sink), context_(context) {}

private:
    std::streamsize xsputn(const char* bytes, std::streamsize size) override {
        if (size == 0) return 0;
        return sink_(context_, bytes, static_cast<size_t>(size)) ? size : 0;
    }

    rage62_rpf7_sink sink_;
    void* context_;
};

uint32_t entry_kind(rage62::Rpf7EntryKind kind) {
    switch (kind) {
    case rage62::Rpf7EntryKind::directory: return RAGE62_RPF7_DIRECTORY;
    case rage62::Rpf7EntryKind::binary: return RAGE62_RPF7_BINARY;
    case rage62::Rpf7EntryKind::resource: return RAGE62_RPF7_RESOURCE;
    }
    return RAGE62_RPF7_DIRECTORY;
}
}

rage62_rpf7* rage62_rpf7_open(const char* path_utf8, char* error, size_t error_capacity) {
    if (!path_utf8 || !*path_utf8) {
        set_error(error, error_capacity, "missing archive path");
        return nullptr;
    }
    try {
        auto handle = std::make_unique<rage62_rpf7>();
        std::string message;
        if (!handle->archive.open(std::filesystem::u8path(path_utf8), message)) {
            set_error(error, error_capacity, message.c_str());
            return nullptr;
        }
        set_error(error, error_capacity, "");
        return handle.release();
    } catch (const std::exception& ex) {
        set_error(error, error_capacity, ex.what());
        return nullptr;
    }
}

void rage62_rpf7_close(rage62_rpf7* archive) {
    delete archive;
}

size_t rage62_rpf7_count(const rage62_rpf7* archive) {
    return archive ? archive->archive.entries().size() : 0;
}

int rage62_rpf7_get_entry(const rage62_rpf7* archive, size_t index, rage62_rpf7_entry* entry) {
    if (!entry || index >= rage62_rpf7_count(archive)) return 0;
    const auto& item = archive->archive.entries()[index];
    *entry = {item.path.c_str(), entry_kind(item.kind), item.stored_size, item.decoded_size,
              item.compressed, item.encrypted};
    return 1;
}

int rage62_rpf7_copy_file(rage62_rpf7* archive, const char* entry_path, rage62_rpf7_sink sink,
                         void* context, char* error, size_t error_capacity) {
    if (!archive || !entry_path || !sink) {
        set_error(error, error_capacity, "missing archive, entry path or sink");
        return 0;
    }
    try {
        SinkBuffer buffer(sink, context);
        std::ostream stream(&buffer);
        std::string message;
        const bool copied = archive->archive.copy_file(entry_path, stream, message);
        set_error(error, error_capacity, message.c_str());
        return copied;
    } catch (const std::exception& ex) {
        set_error(error, error_capacity, ex.what());
        return 0;
    }
}
