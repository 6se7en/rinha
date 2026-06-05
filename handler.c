



#include <stdint.h>
#include <string.h>

#include "handler.h"
#include "arena.h"
#include "errors.h"
#include "iter.h"
#include "ivf.h"
#include "logs.h"
#include "mmap.h"
#include "nnet.h"
#include "normalize.h"
#include "req_parse.h"
#include "results.h"
#include "vector.h"
#include "weights.h"

Request req;
IVF* ivf = nullptr;
Parameters params;
ForwardParams forward_params;

static size_t load_matrix(const u8 * weights, Matrix* matrix) {
    size_t read = 0;
    matrix->rows = *(uint32_t*)(weights);
    read += sizeof(uint32_t);

    matrix->cols = *(uint32_t*)(weights + read);
    read += sizeof(uint32_t);

    matrix->data = (float*)(weights + read);
    read += matrix->rows * matrix->cols * sizeof(float);

    return read;
}

static void load_params_from_weights(const u8* weights, Parameters* p) {
    size_t offset = 0;
    offset += load_matrix(weights, &p->W1);
    offset += load_matrix(weights + offset, &p->b1);
    offset += load_matrix(weights + offset, &p->W2);
    load_matrix(weights + offset, &p->b2);
}

Result handler_init(const bool use_ivf) {
    if (use_ivf) {
        u8* data = nullptr;
        size_t size  = 0;
        try(mopen("ivf.bin", &data, &size));

        Iterator iter;
        iter_init(&iter, data, size);

        ivf = ivf_decode(&iter);
    } else {
        u8* weights = nullptr;
        size_t wsize = 0;
        get_weights(&weights, &wsize);

        load_params_from_weights(weights, &params);

        Arena arena;
        arena_init(&arena, 1024);

        const Matrix X = {
            .rows = DIM,
            .cols = 1,
            .data = nullptr
        };

        try(forward_params_init(&arena, &params, &X, &forward_params));
    }
    return OK;
}

static uint32_t nnet_fraud_check(vec_t vec) {
    const Matrix X = {
        .rows = DIM,
        .cols = 1,
        .data = vec
    };
    return eval(&X, &params, &forward_params);
}

static uint32_t ivf_fraud_check(vec_t vec) {
    u8 qvec[ALIGN_DIM] = {0};
    quantize_vec(ivf->pv, vec, DIM, qvec);

    constexpr int top_k = 5;
    constexpr int nprobes = 8; 
    int r[top_k];

    ivf_search(ivf, qvec, nprobes, top_k, r);

    return r[0] + r[1] + r[2] + r[3] + r[4];
}

Result handle_fraud_check(const char* body, const size_t len, uint32_t* num_fraudes) {
    try(request_parse(body, len, &req));

    vec_t vec;
    normalize(&req, vec);

    const uint32_t n = ivf
        ? ivf_fraud_check(vec)
        : nnet_fraud_check(vec);

    if (UNLIKELY(n < 0 || n > TOP_K)) {
        LOG(LOG_LEVEL_WARNING, "Invalid score computed from model: %d\n", n);
        return "Invalid score computed from model";
    }
    *num_fraudes = n;

    return OK;
}
