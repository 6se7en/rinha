



#ifndef RINHA2026_ITER_H
#define RINHA2026_ITER_H

#include <stddef.h>
#include <stdint.h>

#include "types.h"

typedef struct {
    size_t size;
    size_t index;
    const u8* data;
} Iterator;

void iter_init(Iterator* iter, const u8* data, size_t size);

uint32_t iter_uint32(Iterator* iter);

uint64_t iter_varint(Iterator* iter);

float iter_float(Iterator* iter);

void iter_skip(Iterator* iter, size_t n);

const u8* iter_current(const Iterator* iter);

const u8* iter_list(Iterator* iter, size_t s, size_t n);

#endif 
