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
    unsigned char image[624] = {'9', 'F', 'P', 'R', 80, 0, 0, 0, 64, 0, 0, 0, 16};
    unsigned char locations[120] = {0};
    unsigned char resource[96] = {
        'P', 'S', 'I', 'N', 0, 0, 0, 48,
        0x29, 0, 0, 0, 3, 0, 0, 0, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa
    };
    image[36] = 5;
    memset(image + 96, 'A', 32);
    memset(image + 128, 'B', 32);
    memcpy(resource + 32, "one", 3);
    memcpy(resource + 48, "LSCH\0\0\0\010LENM\0\0\0\010LSNS\0\0\0\010"
                          "LGID\0\0\0\010LRSC\0\0\0\010CHKS\0\0\0\010", 48);
    memcpy(image + 160, resource, sizeof(resource));
    memcpy(image + 272, resource, sizeof(resource));
    // Fifth entry: encoding-2 prefix, system page and graphics page.
    image[392] = 0x50;
    image[395] = 0x50;
    memcpy(image + 400, "0DRP", 4);
    image[404] = 0x7a;
    image[440] = 0x80;
    image[443] = 0x50;
    image[460] = 2;
    image[472] = 1;
    image[473] = 1;
    image[512] = 0x37;
    image[516] = 5;
    image[523] = 0x60;
    image[528] = 0x38;
    image[532] = 2;
    image[536] = 0xbf;
    image[539] = 0x50;
    memcpy(image + 576, "pixel", 5);
    memset(image + 608, 'T', 16);
    locations[8] = 6;
    locations[16] = 9;
    locations[32] = 8;
    locations[56] = 10;
    locations[80] = 16;
    locations[104] = 23;
    return write_file(archive, image, sizeof(image)) && write_file(catalog, locations, sizeof(locations));
}

static int verify_reads(rage62_rpf9* archive, char* error, size_t error_capacity) {
    unsigned char output[4] = {0};
    size_t received;
    rage62_rpf9_entry entry;

    if (rage62_rpf9_count(archive) != 5 || !rage62_rpf9_get_entry(archive, 0, &entry) ||
        entry.offset != 96 || entry.span != 32 || entry.identifier != 9 ||
        rage62_rpf9_get_entry(archive, 5, &entry)) return 0;

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

static int verify_resource(rage62_psin* resource) {
    rage62_psin_block block;
    const int passed = resource && rage62_psin_block_count(resource) == 1 &&
                       rage62_psin_get_block(resource, 0, &block) && block.id == 0x29 &&
                       block.size == 3 && memcmp(block.data, "one", 3) == 0 &&
                       !rage62_psin_get_block(resource, 1, &block);
    rage62_psin_close(resource);
    return passed;
}

static int verify_psin(rage62_rpf9* archive, char* error, size_t error_capacity) {
    const unsigned char table[24] = {8, 0, 0, 0, 2, 0, 0, 2,
                                     3, 0, 0, 0, 3, 0, 0, 0,
                                     3, 0, 0, 0, 3, 0, 0, 0};
    return verify_resource(rage62_rpf9_load_psin(archive, 2, 96, NULL, 0, error, error_capacity)) &&
           verify_resource(rage62_rpf9_load_psin(archive, 3, 112, table, sizeof(table), error, error_capacity));
}

static int verify_paged(rage62_rpf9* archive, char* error, size_t error_capacity) {
    const unsigned char table[24] = {8, 0, 0, 0, 2, 0, 0, 2,
                                     12, 0, 0, 0, 12, 0, 0, 0,
                                     2, 0, 0, 0, 2, 0, 0, 0};
    rage62_resource9* resource = rage62_rpf9_load_resource(archive, 4, 240, table, sizeof(table), error, error_capacity);
    const void* system = rage62_resource9_resolve(resource, 0x50000010, 4);
    const void* graphics = rage62_resource9_resolve(resource, 0x60000000, 5);
    size_t count = 0;
    rage62_0drp_record first;
    rage62_0drp_record second;
    const int passed = system && graphics && memcmp(system, "0DRP", 4) == 0 && memcmp(graphics, "pixel", 5) == 0 &&
                       !rage62_resource9_resolve(resource, 0x500000bf, 2) &&
                       !rage62_resource9_resolve(resource, 0x60000020, 1) &&
                       !rage62_resource9_resolve(resource, UINT64_C(0x150000010), 4) &&
                       rage62_resource9_0drp_count(resource, &count) && count == 2 &&
                       rage62_resource9_0drp_record(resource, 0, &first) && first.word_0 == 0x37 &&
                       first.size_hint == 5 && first.pointer == 0x60000000 && first.data == graphics &&
                       rage62_resource9_0drp_record(resource, 1, &second) && second.word_0 == 0x38 &&
                       second.size_hint == 2 && second.pointer == 0x500000bf && second.data == NULL &&
                       !rage62_resource9_0drp_record(resource, 2, &second);
    rage62_resource9_close(resource);
    return passed;
}

int main(void) {
    const char* archive_path = "rage62_rpf9_bridge_test.rpf";
    const char* catalog_path = "rage62_rpf9_bridge_test.catalog";
    char error[128] = "";
    int passed = 0;
    if (write_fixtures(archive_path, catalog_path)) {
        rage62_rpf9* archive = rage62_rpf9_open(archive_path, catalog_path, error, sizeof(error));
        passed = archive && verify_reads(archive, error, sizeof(error)) && verify_psin(archive, error, sizeof(error)) &&
                 verify_paged(archive, error, sizeof(error));
        rage62_rpf9_close(archive);
    }
    remove(archive_path);
    remove(catalog_path);
    if (!passed) fprintf(stderr, "RPF9 C host check failed: %s\n", error);
    return passed ? 0 : 1;
}
