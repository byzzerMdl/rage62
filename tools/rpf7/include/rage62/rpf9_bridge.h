#ifndef RAGE62_RPF9_BRIDGE_H
#define RAGE62_RPF9_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(RAGE62_RPF9_EXPORTS)
#define RAGE62_RPF9_API __declspec(dllexport)
#elif defined(_WIN32)
#define RAGE62_RPF9_API __declspec(dllimport)
#else
#define RAGE62_RPF9_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rage62_rpf9 rage62_rpf9;
typedef struct rage62_psin rage62_psin;
typedef struct rage62_resource9 rage62_resource9;

typedef struct {
    uint64_t offset;
    uint64_t span;
    uint32_t size_hint;
    uint32_t flags;
    uint32_t identifier;
} rage62_rpf9_entry;

typedef struct {
    const void* data;
    size_t size;
    uint32_t id;
} rage62_psin_block;

/* Paths are UTF-8. Each handle is used by one thread at a time. */
RAGE62_RPF9_API rage62_rpf9* rage62_rpf9_open(const char* archive_path, const char* plain_catalog_path,
                                           char* error, size_t error_capacity);
RAGE62_RPF9_API void rage62_rpf9_close(rage62_rpf9* archive);
RAGE62_RPF9_API size_t rage62_rpf9_count(const rage62_rpf9* archive);
RAGE62_RPF9_API int rage62_rpf9_get_entry(const rage62_rpf9* archive, size_t index, rage62_rpf9_entry* entry);

/* Reads raw bytes at an entry-relative offset. A short read at span end succeeds. */
/* On failure received is zero when supplied. The caller discards the buffer. */
RAGE62_RPF9_API int rage62_rpf9_read(rage62_rpf9* archive, size_t index, uint64_t offset,
                                   void* buffer, size_t capacity, size_t* received,
                                   char* error, size_t error_capacity);

/* Pass NULL and 0 for raw or single-zlib resources. Chunked input takes a plain table value. */
RAGE62_RPF9_API rage62_psin* rage62_rpf9_load_psin(rage62_rpf9* archive, size_t index, uint64_t stored_size,
                                                 const void* chunk_table, size_t table_size,
                                                 char* error, size_t error_capacity);
RAGE62_RPF9_API void rage62_psin_close(rage62_psin* resource);
RAGE62_RPF9_API size_t rage62_psin_block_count(const rage62_psin* resource);
/* A block view remains valid until rage62_psin_close(). */
RAGE62_RPF9_API int rage62_psin_get_block(const rage62_psin* resource, size_t index, rage62_psin_block* block);

RAGE62_RPF9_API rage62_resource9* rage62_rpf9_load_resource(rage62_rpf9* archive, size_t index,
                                                          uint64_t stored_size, const void* chunk_table,
                                                          size_t table_size, char* error, size_t error_capacity);
RAGE62_RPF9_API void rage62_resource9_close(rage62_resource9* resource);
/* Returns NULL for an invalid pointer or range. A view is owned by the resource. */
RAGE62_RPF9_API const void* rage62_resource9_resolve(const rage62_resource9* resource, uint64_t pointer, size_t length);

#ifdef __cplusplus
}
#endif

#endif
