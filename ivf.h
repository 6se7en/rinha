



#ifndef RINHA2026_IVF_H
#define RINHA2026_IVF_H

#include <stdint.h>

#include "errors.h"
#include "types.h"
#include "iter.h"
#include "quantization.h"

typedef struct {
    uint64_t n;
    float* data;
} VecList;

typedef struct {
    VecList list;
    u8* flags;
} Cluster;

typedef struct IVF {
    uint64_t num_subs;
    struct IVF** subs;

    VecList centroids;

    uint64_t num_clusters;
    Cluster* clusters;

    uint32_t num_levels;
    uint32_t level;

    QuantizationParams *pc, *pv;
} IVF;

IVF* ivf_decode(Iterator* iter);

Result ivf_search(const IVF* root, const u8* query, int nprobes, int top_k, int* results);

void ivf_free(IVF * ivf);

#endif 
