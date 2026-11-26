#include "rage62/rpf7.h"

#include <stdio.h>
#include <string.h>

static const unsigned char payload[] = {0, 0x0a, 0x1a, 0xff};

struct capture {
    unsigned char bytes[sizeof(payload)];
    size_t length;
};

static int append(void* context, const void* bytes, size_t size) {
    struct capture* capture = (struct capture*)context;
    if (size > sizeof(capture->bytes) - capture->length) return 0;
    memcpy(capture->bytes + capture->length, bytes, size);
    capture->length += size;
    return 1;
}

static int write_fixture(const char* path) {
    /* RPF7 header, root directory, and one stored binary at sector 1. */
    unsigned char archive[512 + sizeof(payload)] = {
        '7', 'F', 'P', 'R', 2, 0, 0, 0, 7, 0, 0, 0, 'O', 'P', 'E', 'N',
        0, 0, 0, 0, 0, 0xff, 0xff, 0x7f, 1, 0, 0, 0, 1, 0, 0, 0,
        1, 0, 0, 0, 0, 1, 0, 0, sizeof(payload), 0, 0, 0, 0, 0, 0, 0,
        0, 'a', '.', 'b', 'i', 'n', 0
    };
    FILE* file = fopen(path, "wb");
    int wrote;
    if (!file) return 0;
    memcpy(archive + 512, payload, sizeof(payload));
    wrote = fwrite(archive, 1, sizeof(archive), file) == sizeof(archive);
    return fclose(file) == 0 && wrote;
}

static int check_archive(rage62_rpf7* archive, char* error, size_t error_capacity) {
    rage62_rpf7_entry entry;
    struct capture output = {{0}, 0};

    if (rage62_rpf7_count(archive) != 1 ||
        !rage62_rpf7_get_entry(archive, 0, &entry) ||
        rage62_rpf7_get_entry(archive, 1, &entry) ||
        strcmp(entry.path, "a.bin") != 0 || entry.kind != RAGE62_RPF7_BINARY ||
        entry.stored_size != sizeof(payload) || entry.decoded_size != sizeof(payload) ||
        entry.encrypted || entry.compressed) return 0;

    if (!rage62_rpf7_copy_file(archive, entry.path, append, &output, error, error_capacity) ||
        output.length != sizeof(payload) || memcmp(output.bytes, payload, sizeof(payload)) != 0 ||
        error[0] != '\0') return 0;

    return !rage62_rpf7_copy_file(archive, entry.path, append, &output, error, error_capacity) &&
           strstr(error, "cannot write") != NULL;
}

int main(void) {
    const char* path = "rage62_rpf_bridge_test.rpf";
    char error[128] = "";
    char short_error[2] = "X";
    rage62_rpf7* archive = rage62_rpf7_open(NULL, short_error, sizeof(short_error));
    int passed;
    if (archive || short_error[0] == '\0' || short_error[1] != '\0') {
        rage62_rpf7_close(archive);
        return 1;
    }

    if (!write_fixture(path)) return 2;
    archive = rage62_rpf7_open(path, error, sizeof(error));
    passed = archive && check_archive(archive, error, sizeof(error));
    rage62_rpf7_close(archive);
    remove(path);
    if (!passed) fprintf(stderr, "RPF7 bridge check failed: %s\n", error);
    return passed ? 0 : 3;
}
