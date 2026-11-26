#include "rage62/rpf9_bridge.h"
#include "rage62/psin.h"
#include "rage62/rpf9.h"
#include "bridge_error.h"

#include <exception>
#include <memory>

struct rage62_rpf9 {
    rage62::Rpf9Archive archive;
};

struct rage62_psin {
    rage62::PsinResource resource;
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

rage62_psin* rage62_rpf9_load_psin(rage62_rpf9* archive, size_t index, uint64_t stored_size,
                                 const void* chunk_table, size_t table_size, char* error, size_t error_capacity) {
    if (!archive || ((chunk_table == nullptr) != (table_size == 0))) {
        set_error(error, error_capacity, "missing RPF9 archive or invalid chunk table buffer");
        return nullptr;
    }
    try {
        auto result = std::make_unique<rage62_psin>();
        std::string message;
        bool loaded;
        if (chunk_table) {
            const std::string_view table(static_cast<const char*>(chunk_table), table_size);
            loaded = rage62::load_chunked_psin(archive->archive, index, stored_size, table, result->resource, message);
        } else {
            loaded = rage62::load_psin(archive->archive, index, stored_size, result->resource, message);
        }
        set_error(error, error_capacity, message.c_str());
        return loaded ? result.release() : nullptr;
    } catch (const std::exception& ex) {
        set_error(error, error_capacity, ex.what());
        return nullptr;
    }
}

void rage62_psin_close(rage62_psin* resource) {
    delete resource;
}

size_t rage62_psin_block_count(const rage62_psin* resource) {
    return resource ? resource->resource.blocks.size() : 0;
}

int rage62_psin_get_block(const rage62_psin* resource, size_t index, rage62_psin_block* block) {
    if (!block || index >= rage62_psin_block_count(resource)) return 0;
    const auto& item = resource->resource.blocks[index];
    *block = {resource->resource.data.data() + item.offset, item.size, item.id};
    return 1;
}
