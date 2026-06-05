



#ifndef RINHA2026_NNET_H
#define RINHA2026_NNET_H
#include "arena.h"
#include "matrix.h"

typedef struct {
    Matrix W1;
    Matrix b1;
    Matrix W2;
    Matrix b2;
} Parameters;

typedef struct {
    Matrix W1;
    Matrix b1;
    Matrix W2;
    Matrix b2;

    
    Matrix w1x;
    Matrix w2a1;
} ForwardParams;

Result forward_params_init(Arena* arena, const Parameters* params, const Matrix* X, ForwardParams* forward_params);

int eval(const Matrix* X, const Parameters* input, ForwardParams* params);

#endif 
