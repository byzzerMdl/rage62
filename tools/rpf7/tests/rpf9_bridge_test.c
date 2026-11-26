#include "rage62/rpf9_bridge.h"

#include <stdio.h>
#include <string.h>

static int write_file(const char* path, const void* bytes, size_t size) {
    FILE* file = fopen(path, "wb");
    int wrote;
    if (!file) return 0;
    wrote = fwrite(bytes, 1, size, file) == size;
    return fclose(file) == 0 && wrote;
}

static int write_fixtures(const char* archive, const char* catalog) {
    unsigned char image[176] = {'9', 'F', 'P', 'R', 80, 0, 0, 0, 64, 0, 0, 0, 16};
    unsigned char locations[48] = {0};
    image[36] = 2;
    memset(image + 96, 'A', 32);
    memset(image + 128, 'B', 32);
    memset(image + 160, 'T', 16);
    locations[8] = 6;
    locations[16] = 9;
    locations[32] = 8;
    return write_file(archive, image, sizeof(image)) && write_file(catalog, locations, sizeof(locations));
}

static int verify_reads(rage62_rpf9* archive, char* error, size_t error_capacity) {
    unsigned char output[4] = {0};
    size_t received;
    rage62_rpf9_entry entry;

    if (rage62_rpf9_count(archive) != 2 || !rage62_rpf9_get_entry(archive, 0, &entry) ||
        entry.offset != 96 || entry.span != 32 || entry.identifier != 9 ||
        rage62_rpf9_get_entry(archive, 2, &entry)) return 0;

    if (!rage62_rpf9_read(archive, 1, 31, output, sizeof(output), &received, error, error_capacity) ||
        received != 1 || output[0] != 'B' || output[1] != 0) return 0;
    if (!rage62_rpf9_read(archive, 0, 30, output, sizeof(output), &received, error, error_capacity) ||
        received != 2 || memcmp(output, "AA\0\0", sizeof(output)) != 0) return 0;
    if (!rage62_rpf9_read(archive, 0, 32, output, sizeof(output), &received, error, error_capacity) ||
        received != 0 || error[0] != '\0') return 0;

    if (rage62_rpf9_read(archive, 0, 33, output, sizeof(output), &received, error, error_capacity) ||
        received != 0 || !strstr(error, "offset")) return 0;
    if (rage62_rpf9_read(archive, 0, 0, NULL, 1, &received, error, error_capacity) ||
        received != 0 || !strstr(error, "buffer")) return 0;
    return rage62_rpf9_read(archive, 0, 0, output, sizeof(output), &received, error, error_capacity) &&
           received == sizeof(output) && memcmp(output, "AAAA", sizeof(output)) == 0 && error[0] == '\0';
}

int main(void) {
    const char* archive_path = "rage62_rpf9_bridge_test.rpf";
    const char* catalog_path = "rage62_rpf9_bridge_test.catalog";
    char error[128] = "";
    int passed = 0;
    if (write_fixtures(archive_path, catalog_path)) {
        rage62_rpf9* archive = rage62_rpf9_open(archive_path, catalog_path, error, sizeof(error));
        passed = archive && verify_reads(archive, error, sizeof(error));
        rage62_rpf9_close(archive);
    }
    remove(archive_path);
    remove(catalog_path);
    if (!passed) fprintf(stderr, "RPF9 C host check failed: %s\n", error);
    return passed ? 0 : 1;
}
