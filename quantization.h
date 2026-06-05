



#ifndef RINHA2026_QUANTIZATION_H
#define RINHA2026_QUANTIZATION_H
#include <stddef.h>

#include "types.h"

typedef struct {
    float min_value;
    float scale;
} QuantizationParams;

void quantize_vec(const QuantizationParams* params, const float* in, size_t n, u8* out);

#endif 
