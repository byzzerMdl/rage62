#ifndef RAGE62_RPF7_H
#define RAGE62_RPF7_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(RAGE62_RPF7_EXPORTS)
#define RAGE62_RPF7_API __declspec(dllexport)
#elif defined(_WIN32)
#define RAGE62_RPF7_API __declspec(dllimport)
#else
#define RAGE62_RPF7_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rage62_rpf7 rage62_rpf7;

enum {
    RAGE62_RPF7_DIRECTORY = 0,
    RAGE62_RPF7_BINARY = 1,
    RAGE62_RPF7_RESOURCE = 2
};

typedef struct {
    const char* path; /* Owned by the archive until rage62_rpf7_close(). */
    uint32_t kind;
    uint32_t stored_size;
    uint32_t decoded_size; /* Binary entries only. */
    uint32_t compressed; /* Binary entries only. */
    uint32_t encrypted; /* Binary entries only. */
} rage62_rpf7_entry;

/* Return nonzero to accept a chunk. The bytes are valid only during the call. */
typedef int (*rage62_rpf7_sink)(void* context, const void* bytes, size_t size);

/* Use one thread at a time per handle. An optional error buffer is NUL-terminated. */
RAGE62_RPF7_API rage62_rpf7* rage62_rpf7_open(const char* path_utf8, char* error, size_t error_capacity);
RAGE62_RPF7_API void rage62_rpf7_close(rage62_rpf7* archive);
RAGE62_RPF7_API size_t rage62_rpf7_count(const rage62_rpf7* archive);
RAGE62_RPF7_API int rage62_rpf7_get_entry(const rage62_rpf7* archive, size_t index, rage62_rpf7_entry* entry);

/* Returns nonzero on success. A failure can occur after a chunk has been sent to the sink. */
RAGE62_RPF7_API int rage62_rpf7_copy_file(rage62_rpf7* archive, const char* entry_path,
                                        rage62_rpf7_sink sink, void* context, char* error, size_t error_capacity);

#ifdef __cplusplus
}
#endif

#endif
