
#include "iter.h"

void iter_init(Iterator* iter, const u8* data, const size_t size) {
    iter->size = size;
    iter->index = 0;
    iter->data = data;
}

uint32_t iter_uint32(Iterator* iter) {
    const uint32_t v =
        (uint32_t)iter->data[iter->index] |
        (uint32_t)iter->data[iter->index + 1] << 8 |
        (uint32_t)iter->data[iter->index + 2] << 16 |
        (uint32_t)iter->data[iter->index + 3] << 24;
    iter->index += 4;
    return v;
}

float iter_float(Iterator* iter) {
    const float* f = (float*)(iter->data + iter->index);
    iter->index += 4;
    return *f;
}

void iter_skip(Iterator* iter, const size_t n) {
    iter->index += n;
}

const u8* iter_current(const Iterator* iter) {
    return iter->data + iter->index;
}

uint64_t iter_varint(Iterator* iter) {
    uint64_t result = 0;
    int shift = 0;
    uint8_t b;
    do {
        b = iter->data[iter->index++];
        result |= (uint64_t)(b & 0x7F) << shift;
        shift += 7;
    } while (b & 0x80);
    return result;
}

const u8* iter_list(Iterator* iter, const size_t s, const size_t n) {
    const u8* ptr = iter->data + iter->index;
    iter->index += s * n;
    return ptr;
}
