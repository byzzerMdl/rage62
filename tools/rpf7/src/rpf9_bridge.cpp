#include "rage62/rpf9_bridge.h"
#include "rage62/rpf9.h"
#include "bridge_error.h"

#include <exception>
#include <memory>

struct rage62_rpf9 {
    rage62::Rpf9Archive archive;
};

using rage62::detail::set_error;

rage62_rpf9* rage62_rpf9_open(const char* archive_path, const char* plain_catalog_path,
                             char* error, size_t error_capacity) {
    if (!archive_path || !*archive_path || !plain_catalog_path || !*plain_catalog_path) {
        set_error(error, error_capacity, "missing RPF9 archive or plain catalog path");
        return nullptr;
    }
    try {
        auto handle = std::make_unique<rage62_rpf9>();
        std::string message;
        if (!handle->archive.open(std::filesystem::u8path(archive_path), message) ||
            !handle->archive.load_catalog(std::filesystem::u8path(plain_catalog_path), message)) {
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

void rage62_rpf9_close(rage62_rpf9* archive) {
    delete archive;
}

size_t rage62_rpf9_count(const rage62_rpf9* archive) {
    return archive ? archive->archive.entries().size() : 0;
}

int rage62_rpf9_get_entry(const rage62_rpf9* archive, size_t index, rage62_rpf9_entry* entry) {
    if (!entry || index >= rage62_rpf9_count(archive)) return 0;
    const auto& item = archive->archive.entries()[index];
    *entry = {item.offset, item.span, item.size_hint, item.flags, item.identifier};
    return 1;
}

int rage62_rpf9_read(rage62_rpf9* archive, size_t index, uint64_t offset,
                    void* buffer, size_t capacity, size_t* received, char* error, size_t error_capacity) {
    if (received) *received = 0;
    if (!archive || !received) {
        set_error(error, error_capacity, "missing RPF9 archive or received count");
        return 0;
    }
    try {
        std::string message;
        const auto ok = archive->archive.read_span(index, offset, static_cast<char*>(buffer), capacity,
                                                   *received, message);
        set_error(error, error_capacity, message.c_str());
        return ok;
    } catch (const std::exception& ex) {
        set_error(error, error_capacity, ex.what());
        return 0;
    }
}
