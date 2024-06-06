#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void printRecord(struct index_s record) {
    printf("%lf -> %ld\n", record.time_mark, record.recno);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Use viewer [file name].\n");
        return 0xDEAD;
    }

    FILE *file = fopen(argv[1], "r");

    if (file == NULL) {
        printf("Couldn't open file.\n");
        return 0xF11E;
    }

    srand(time(NULL));

    struct index_hdr_s header;

    fread(&header, sizeof(header), 1, file);

    for(uint64_t i = 0; i < header.records; i++) {
        struct index_s index;
        fread(&index, sizeof(index), 1, file);
        printRecord(index);
    }
    
    fclose(file);
    
    return 0;
}