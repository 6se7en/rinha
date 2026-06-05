



#ifndef RINHA2026_MATRIX_H
#define RINHA2026_MATRIX_H
#include <stdint.h>

#include "errors.h"

typedef struct {
    uint32_t rows;
    uint32_t cols;
    float* data;
} Matrix;

Result matrix_dot(const Matrix* a, const Matrix* b, Matrix* c);

Result matrix_add(const Matrix* a, const Matrix* b, Matrix* c);

Result matrix_relu(const Matrix* m, const Matrix* out);

Result matrix_softmax(const Matrix* m, Matrix* out);

#endif 
