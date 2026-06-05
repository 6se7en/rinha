



#include "ivf.h"

#include <immintrin.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

#include "quantization.h"
#include "vector.h"





static const int BEAM_SCHEDULE[]     = {4, 2, 2, 2};
static const int BEAM_SCHEDULE_LEN   = 4;
#define          MAX_BEAM             64   



typedef struct {
    const IVF* node;
    uint32_t   d;      
} BeamEntry;

typedef struct {
    const u8*  data;   
    uint64_t   n;      
    const u8*  flags;  
    uint32_t   d;      
} LeafCand;



typedef struct {
    BeamEntry  beam[MAX_BEAM];
    int        beam_n;
    BeamEntry* next;        
    int        next_n;
    int        next_cap;
    LeafCand*  cands;       
    int        cands_n;
    int        cands_cap;
    uint32_t*  dbuf;        
    int        dbuf_cap;
} SearchState;

static SearchState* g_ss   = NULL;
static uint64_t     max_vecs = 0;


static void read_centroids(Iterator* iter, VecList* list) {
    list->n    = iter_varint(iter);
    list->data = (float*)iter_list(iter, sizeof(u8) * ALIGN_DIM, list->n);
    if (list->n > max_vecs) {
        max_vecs = list->n;
    }
}


static void read_cluster(Iterator* iter, Cluster* cluster) {
    cluster->list.n = iter_varint(iter);
    cluster->list.data = (float*)iter_list(iter, sizeof(u8) * ALIGN_DIM, cluster->list.n);
    cluster->flags = (u8*)iter_list(iter, sizeof(u8), cluster->list.n);
    if (cluster->list.n > max_vecs) {
        max_vecs = cluster->list.n;
    }
}

IVF* ivf_decode_at_level(Iterator* iter, const uint32_t level) {
    IVF* ivf = malloc(sizeof(IVF));
    ivf->level = level;
    ivf->num_subs = iter_varint(iter);
    read_centroids(iter, &ivf->centroids);
    if (ivf->num_subs == 0) {
        ivf->num_clusters = iter_varint(iter);
        if (ivf->num_clusters > 0) {
            ivf->clusters = (Cluster*)malloc(sizeof(Cluster) * ivf->num_clusters);
        } else {
            ivf->clusters = nullptr;
        }
        for (uint64_t i = 0; i < ivf->num_clusters; i++) {
            read_cluster(iter, &ivf->clusters[i]);
        }
        return ivf;
    }
    ivf->subs = malloc(sizeof(IVF*) * ivf->num_subs);
    for (uint64_t i = 0; i < ivf->num_subs; i++) {
        ivf->subs[i] = ivf_decode_at_level(iter, level + 1);
    }
    return ivf;
}

QuantizationParams* read_quant_params(Iterator* iter) {
    QuantizationParams* p = malloc(sizeof(QuantizationParams));
    p->min_value = iter_float(iter);
    p->scale = iter_float(iter);
    return p;
}

IVF* ivf_decode(Iterator* iter) {
    QuantizationParams* pc = read_quant_params(iter);
    QuantizationParams* pv = read_quant_params(iter);

    const uint32_t num_levels = iter_varint(iter);
    IVF* root = ivf_decode_at_level(iter, 1);
    root->num_levels = num_levels;
    root->pc = pc;
    root->pv = pv;

    
    const int cap = (int)(max_vecs < 64 ? 64 : max_vecs);
    g_ss = calloc(1, sizeof(SearchState));
    g_ss->next      = malloc((size_t)cap * 8 * sizeof(BeamEntry));
    g_ss->next_cap  = cap * 8;
    g_ss->cands     = malloc((size_t)cap * sizeof(LeafCand));
    g_ss->cands_cap = cap;
    g_ss->dbuf      = malloc((size_t)cap * sizeof(uint32_t));
    g_ss->dbuf_cap  = cap;

    return root;
}


static inline uint32_t dist_u8_sq(const u8* __restrict__ a, const u8* __restrict__ b) {
    const __m128i va   = _mm_loadu_si128((const __m128i*)a);
    const __m128i vb   = _mm_loadu_si128((const __m128i*)b);
    const __m256i a16  = _mm256_cvtepu8_epi16(va);
    const __m256i b16  = _mm256_cvtepu8_epi16(vb);
    const __m256i diff = _mm256_sub_epi16(a16, b16);
    const __m256i sq   = _mm256_madd_epi16(diff, diff);  
    const __m128i lo   = _mm256_castsi256_si128(sq);
    const __m128i hi   = _mm256_extracti128_si256(sq, 1);
    const __m128i sum  = _mm_add_epi32(lo, hi);
    const __m128i s2   = _mm_hadd_epi32(sum, sum);
    const __m128i s1   = _mm_hadd_epi32(s2, s2);
    return (uint32_t)_mm_cvtsi128_si32(s1);
}







static inline void select_top_beam(BeamEntry* a, int n, int k) {
    if (k >= n) return;
    for (int i = 0; i < k; i++) {
        int min_idx = i;
        uint32_t min_d = a[i].d;
        for (int j = i + 1; j < n; j++) {
            if (a[j].d < min_d) {
                min_d = a[j].d;
                min_idx = j;
            }
        }
        if (min_idx != i) {
            BeamEntry t = a[i];
            a[i] = a[min_idx];
            a[min_idx] = t;
        }
    }
}

static inline void select_top_cand(LeafCand* a, int n, int k) {
    if (k >= n) return;
    for (int i = 0; i < k; i++) {
        int min_idx = i;
        uint32_t min_d = a[i].d;
        for (int j = i + 1; j < n; j++) {
            if (a[j].d < min_d) {
                min_d = a[j].d;
                min_idx = j;
            }
        }
        if (min_idx != i) {
            LeafCand t = a[i];
            a[i] = a[min_idx];
            a[min_idx] = t;
        }
    }
}





typedef struct {
    int      n;           
    int      m;           
    uint32_t max;         
    u8*      keys;        
    uint32_t* values;     
} TopK;

static void topk_init(TopK* h, const int k, u8* keys, uint32_t* values) {
    h->n      = 0;
    h->m      = k;
    h->max    = UINT32_MAX;
    h->keys   = keys;
    h->values = values;
}

static void topk_pop_max(TopK* h) {
    if (h->n == 0) return;
    const int last = h->n - 1;
    h->keys[0]   = h->keys[last];
    h->values[0] = h->values[last];
    h->n--;

    int i = 0;
    while (1) {
        int lg = i;
        const int l = 2*i+1, r = 2*i+2;
        if (l < h->n && h->values[l] > h->values[lg]) lg = l;
        if (r < h->n && h->values[r] > h->values[lg]) lg = r;
        if (lg == i) break;
        u8       tk = h->keys[i];   h->keys[i]   = h->keys[lg];   h->keys[lg]   = tk;
        uint32_t tv = h->values[i]; h->values[i] = h->values[lg]; h->values[lg] = tv;
        i = lg;
    }
    h->max = h->n > 0 ? h->values[0] : UINT32_MAX;
}

static inline void topk_insert(TopK* h, const u8 key, const uint32_t value) {
    if (h->n >= h->m) {
        if (value >= h->max) return;
        topk_pop_max(h);
    }
    int i = h->n;
    h->keys[i]   = key;
    h->values[i] = value;
    h->n++;
    while (i > 0) {
        const int p = (i-1)/2;
        if (h->values[i] <= h->values[p]) break;
        u8       tk = h->keys[i];   h->keys[i]   = h->keys[p];   h->keys[p]   = tk;
        uint32_t tv = h->values[i]; h->values[i] = h->values[p]; h->values[p] = tv;
        i = p;
    }
    h->max = h->values[0];
}





Result ivf_search(const IVF* root, const u8* query, const int nprobes, const int top_k, int* results) {
    SearchState* st = g_ss;

    
    st->beam[0] = (BeamEntry){root, 0u};
    st->beam_n  = 1;

    
    int step = 0;
    while (st->beam_n > 0 && st->beam[0].node->num_subs > 0) {
        st->next_n = 0;

        for (int b = 0; b < st->beam_n; b++) {
            const IVF*    n  = st->beam[b].node;
            const uint64_t cn = n->centroids.n;
            if (cn == 0) continue;

            
            if ((int)cn > st->dbuf_cap) {
                free(st->dbuf);
                st->dbuf_cap = (int)cn * 2;
                st->dbuf     = malloc((size_t)st->dbuf_cap * sizeof(uint32_t));
            }

            
            const u8* cent = (const u8*)n->centroids.data;
            for (uint64_t i = 0; i < cn; i++)
                st->dbuf[i] = dist_u8_sq(query, cent + i * ALIGN_DIM);

            
            const uint64_t ns = n->num_subs < cn ? n->num_subs : cn;
            for (uint64_t i = 0; i < ns; i++) {
                if (!n->subs[i]) continue;
                if (st->next_n >= st->next_cap) {
                    st->next_cap *= 2;
                    st->next      = realloc(st->next, (size_t)st->next_cap * sizeof(BeamEntry));
                }
                st->next[st->next_n++] = (BeamEntry){n->subs[i], st->dbuf[i]};
            }
        }

        
        const int si = step < BEAM_SCHEDULE_LEN ? step : BEAM_SCHEDULE_LEN - 1;
        const int w  = BEAM_SCHEDULE[si];
        if (st->next_n > w) {
            select_top_beam(st->next, st->next_n, w);
            st->next_n = w;
        }

        
        memcpy(st->beam, st->next, (size_t)st->next_n * sizeof(BeamEntry));
        st->beam_n = st->next_n;
        step++;
    }

    
    st->cands_n = 0;
    for (int b = 0; b < st->beam_n; b++) {
        const IVF*    n  = st->beam[b].node;
        const uint64_t cn = n->num_clusters;
        if (cn == 0) continue;

        if ((int)cn > st->dbuf_cap) {
            free(st->dbuf);
            st->dbuf_cap = (int)cn * 2;
            st->dbuf     = malloc((size_t)st->dbuf_cap * sizeof(uint32_t));
        }

        const u8* cent = (const u8*)n->centroids.data;
        for (uint64_t i = 0; i < cn; i++)
            st->dbuf[i] = dist_u8_sq(query, cent + i * ALIGN_DIM);

        for (uint64_t i = 0; i < cn; i++) {
            if (n->clusters[i].list.n == 0) continue;
            if (st->cands_n >= st->cands_cap) {
                st->cands_cap *= 2;
                st->cands      = realloc(st->cands, (size_t)st->cands_cap * sizeof(LeafCand));
            }
            st->cands[st->cands_n++] = (LeafCand){
                (const u8*)n->clusters[i].list.data,
                n->clusters[i].list.n,
                n->clusters[i].flags,
                st->dbuf[i]
            };
        }
    }

    
    if (st->cands_n > nprobes) {
        select_top_cand(st->cands, st->cands_n, nprobes);
        st->cands_n = nprobes;
    }

    
    u8       tk_keys[top_k];
    uint32_t tk_vals[top_k];
    TopK     ranking;
    topk_init(&ranking, top_k, tk_keys, tk_vals);

    for (int c = 0; c < st->cands_n; c++) {
        if (c + 1 < st->cands_n) {
            const LeafCand* next = &st->cands[c + 1];
            __builtin_prefetch(next->data, 0, 0);
            __builtin_prefetch(next->flags, 0, 0);
        }
        const LeafCand* cand  = &st->cands[c];
        const u8*       vdata = cand->data;
        for (uint64_t j = 0; j < cand->n; j++) {
            const uint32_t d = dist_u8_sq(query, vdata + j * ALIGN_DIM);
            topk_insert(&ranking, cand->flags[j], d);
        }
    }

    for (int i = 0; i < ranking.n; i++)
        results[i] = ranking.keys[i];

    return OK;
}

void ivf_free(IVF * ivf) {
    if (ivf == NULL) return;
    if (ivf->subs) {
        for (uint64_t i = 0; i < ivf->num_subs; i++) {
            ivf_free(ivf->subs[i]);
        }
        free(ivf->subs);
    }
    if (ivf->num_clusters > 0) {
        free(ivf->clusters);
    }
    free(ivf);
    
    if (g_ss) {
        free(g_ss->next);
        free(g_ss->cands);
        free(g_ss->dbuf);
        free(g_ss);
        g_ss = NULL;
    }
}
