



#include <math.h>

#include "nnet.h"

#include <string.h>

Result arena_alloc_matrix(Arena* arena, const uint32_t rows, const uint32_t cols, Matrix* m) {
    m->rows = rows;
    m->cols = cols;
    const size_t size = rows * cols;
    try(arena_alloc(arena, size * sizeof(float), (void**)&m->data));
    memset(m->data, 0, size * sizeof(float));
    return OK;
}

Result forward_params_init(Arena* arena, const Parameters* params, const Matrix* X, ForwardParams* forward_params) {
    
    try(arena_alloc_matrix(arena, params->W1.rows, X->cols, &forward_params->w1x));   
    try(arena_alloc_matrix(arena, params->W1.rows, X->cols, &forward_params->W1));    
    try(arena_alloc_matrix(arena, params->W1.rows, X->cols, &forward_params->b1));    

    
    try(arena_alloc_matrix(arena, params->W2.rows, X->cols, &forward_params->w2a1));  
    try(arena_alloc_matrix(arena, params->W2.rows, X->cols, &forward_params->W2));    
    try(arena_alloc_matrix(arena, params->W2.rows, X->cols, &forward_params->b2));    

    return OK;
}

void matrix_print(char* label, const Matrix* m) {
    printf("%s\n", label);
    for (int i = 0; i < m->rows; i++) {
        for (int j = 0; j < m->cols; j++) {
            printf("%f ", m->data[i * m->cols + j]);
        }
        printf("\n");
    }
    printf("--------------\n");
}

static Result forward(const Parameters* params, const Matrix* X, ForwardParams* z) {
    try(matrix_dot(&params->W1, X, &z->w1x));
    DEBUG(matrix_print("w1x", &z->w1x));

    try(matrix_add(&z->w1x, &params->b1, &z->W1));
    DEBUG(matrix_print("z->W1", &z->W1));

    try(matrix_relu(&z->W1, &z->b1));
    DEBUG(matrix_print("z->b1", &z->b1));

    try(matrix_dot(&params->W2, &z->b1, &z->w2a1));
    DEBUG(matrix_print("w2a1", &z->w2a1));

    try(matrix_add(&z->w2a1, &params->b2, &z->W2));
    DEBUG(matrix_print("z->W2", &z->W2));

    try(matrix_softmax(&z->W2, &z->b2));
    DEBUG(matrix_print("z->b2", &z->b2));

    return OK;
}

static int prediction(const Matrix* a2) {
    float max = -INFINITY;
    int idx = 0;
    for (int y = 0; y < a2->rows; y++) {
        constexpr int x = 0;
        const float v = a2->data[y * a2->cols + x];
        if (v > max) {
            max = v;
            idx = y;
        }
    }
    return idx;
}

int eval(const Matrix* X, const Parameters* input, ForwardParams* params) {
    panic(forward(input, X, params));
    return prediction(&params->b2);
}
