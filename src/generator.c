#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

double randomDate() {
    return JULIAN_EPOCH + (JULIAN_YESTERDAY - JULIAN_EPOCH) * rand() / RAND_MAX;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Use generator [file name] [size].\n");
        return 0xDEAD;
    }

    uint64_t size;
    if (sscanf(argv[2], "%lu", &size) != 1) {
        printf("Size should be a non-negative decimal number.\n");
        return 0xAB0BA;
    }

    FILE *file = fopen(argv[1], "w");

    if (file == NULL) {
        printf("Couldn't open file.\n");
        return 0xF11E;
    }

    srand(time(NULL));

    struct index_hdr_s header = {
        .records = size
    };

    fwrite(&header, sizeof(header), 1, file);

    for (uint64_t i = 0; i < size; i++) {
        struct index_s index = {
            .time_mark = randomDate(),
            .recno = i + 1
        };
        fwrite(&index, sizeof(index), 1, file);
    }
    fclose(file);
    
    return 0;
}