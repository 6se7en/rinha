



#ifndef RINHA2026_RESULTS_H
#define RINHA2026_RESULTS_H
#include <stdint.h>

#include "types.h"

#define TOP_K 5

typedef struct Results {
    size_t n;
    size_t max;
    uint32_t distances[TOP_K];
    u8 is_fraud[TOP_K];
} Results;

void results_init(Results* results);

void results_push(Results* results, uint32_t dist, u8 is_fraud);

void results_print(const Results *r);

#endif 
