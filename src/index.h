#pragma once

#include <stdint.h>

#define JULIAN_YESTERDAY ((double)2460457)
#define JULIAN_EPOCH ((double)15020)

struct index_s {
    double time_mark;
    uint64_t recno;
};

struct index_hdr_s {
    uint64_t records;
    struct index_s idx[];
};