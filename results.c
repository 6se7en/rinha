



#include <string.h>

#include "results.h"

#include <stdio.h>

void results_init(Results* results) {
    results->n = 0;
    results->max = 0;
    for (int i = 0; i < TOP_K; i++) {
        results->distances[i] = 0;
    }
}



static void heap_swap(Results* r, int i, int j) {
    uint32_t tmp_d  = r->distances[i];
    u8       tmp_f  = r->is_fraud[i];
    r->distances[i] = r->distances[j];
    r->is_fraud[i]  = r->is_fraud[j];
    r->distances[j] = tmp_d;
    r->is_fraud[j]  = tmp_f;
}

static void sift_up(Results* r, int i) {
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (r->distances[parent] > r->distances[i]) {
            heap_swap(r, parent, i);
            i = parent;
        } else {
            break;
        }
    }
}

static int find_max_idx(const Results* r) {
    int idx = 0;
    for (int i = 1; i < (int)r->n; i++) {
        if (r->distances[i] > r->distances[idx]) idx = i;
    }
    return idx;
}



void results_push(Results* results, const uint32_t dist, const u8 is_fraud) {
    if (results->n < TOP_K) {
        
        results->distances[results->n] = dist;
        results->is_fraud[results->n]  = is_fraud;
        results->n += 1;
        sift_up(results, (int)results->n - 1);
        if (dist > results->max) results->max = dist;
    } else {
        
        if (dist >= results->max) {
            return;
        }
        const int idx = find_max_idx(results);
        results->distances[idx] = dist;
        results->is_fraud[idx]  = is_fraud;
        sift_up(results, idx);
        
        results->max = results->distances[find_max_idx(results)];
    }
}

void results_print(const Results *r) {
    for (int i = 0; i < r->n; i++) {
        printf("distance: %u - is fraud %hhu\n", r->distances[i], r->is_fraud[i]);
    }
}
