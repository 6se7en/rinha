#include "matrix.h"

#include <math.h>
#include <string.h>

#include "logs.h"

const Result errIncompatibleShape = "Incompatible shapes for matrix operation";

Result matrix_dot(const Matrix* a, const Matrix* b, Matrix* c) {
    if (a->cols != b->rows || a->rows != c->rows || b->cols != c->cols) {
        LOG(LOG_LEVEL_ERROR, "Error: Failed to get dot product shapes (%d, %d) and (%d, %d) for output shape (%d, %d)\n",
            a->rows, a->cols, b->rows, b->cols, c->rows, c->cols);
        return errIncompatibleShape;
    }

    for (int i = 0; i < a->rows; i++) {
        for (int j = 0; j < b->cols; j++) {
            float sum = 0;
            for (int k = 0; k < a->cols; k++) {
                sum += a->data[i * a->cols + k] * b->data[k * b->cols + j];
            }
            c->data[i * c->cols + j] = sum;
        }
    }

    return OK;
}

Result matrix_add(const Matrix* a, const Matrix* b, Matrix* c) {
    if (a->rows != c->rows || a->cols != c->cols) {
        LOG(LOG_LEVEL_ERROR, "Error: Output shape (%d, %d) does not match input shape (%d, %d)\n", c->rows, c->cols, a->rows, a->cols);
        return errIncompatibleShape;
    }

    
    
    const int b_broadcast_cols = (b->cols == 1 && a->cols > 1 && b->rows == a->rows);
    const int b_broadcast_rows = (b->rows == 1 && a->rows > 1 && b->cols == a->cols);

    if (!b_broadcast_cols && !b_broadcast_rows &&
        (a->rows != b->rows || a->cols != b->cols)) {
        LOG(LOG_LEVEL_ERROR, "Error: Failed to add shapes (%d, %d) and (%d, %d) for output shape (%d, %d)\n",
            a->rows, a->cols, b->rows, b->cols, c->rows, c->cols);
        return errIncompatibleShape;
    }

    for (int i = 0; i < a->rows; i++) {
        for (int j = 0; j < a->cols; j++) {
            const int bi = b_broadcast_rows ? 0 : i;
            const int bj = b_broadcast_cols ? 0 : j;
            c->data[i * c->cols + j] = a->data[i * a->cols + j] + b->data[bi * b->cols + bj];
        }
    }

    return OK;
}

Result matrix_relu(const Matrix* m, const Matrix* out) {
    if (m->rows != out->rows || m->cols != out->cols) {
        LOG(LOG_LEVEL_ERROR, "Error: Failed to apply ReLU to shapes (%d, %d) and (%d, %d)\n", m->rows, m->cols, out->rows, out->cols);
        return errIncompatibleShape;
    }
    for (uint32_t i = 0; i < m->rows; i++) {
        for (uint32_t j = 0; j < m->cols; j++) {
            const uint32_t idx = i * m->cols + j;
            const float val = m->data[idx];
            out->data[idx] = fmaxf(val, 0);
        }
    }
    return OK;
}

Result matrix_softmax(const Matrix* m, Matrix* out) {
    if (m->rows != out->rows || m->cols != out->cols) {
        LOG(LOG_LEVEL_ERROR, "Error: Failed to apply softmax to shapes (%d, %d) and (%d, %d)\n", m->rows, m->cols, out->rows, out->cols);
        return errIncompatibleShape;
    }

    float sum[m->cols];
    memset(sum, 0, sizeof(float) * m->cols);

    
    float col_max[m->cols];
    for (int j = 0; j < m->cols; j++) {
        col_max[j] = -INFINITY;
        for (int i = 0; i < m->rows; i++) {
            const float v = m->data[i * m->cols + j];
            if (v > col_max[j]) col_max[j] = v;
        }
    }

    for (int i = 0; i < m->rows; i++) {
        for (int j = 0; j < m->cols; j++) {
            const float f = expf(m->data[i * m->cols + j] - col_max[j]);
            out->data[i * m->cols + j] = f;
            sum[j] += f;
        }
    }

    for (int i = 0; i < out->rows; i++) {
        for (int j = 0; j < out->cols; j++) {
            out->data[i * out->cols + j] /= sum[j];
        }
    }

    return OK;
}
