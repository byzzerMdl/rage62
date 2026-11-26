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

typedef struct {
    uint64_t offset;
    uint64_t span;
    uint32_t size_hint;
    uint32_t flags;
    uint32_t identifier;
} rage62_rpf9_entry;

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

#ifdef __cplusplus
}
#endif

#endif
