



#include "quantization.h"

#include <immintrin.h>
#include <math.h>
#include <string.h>

void quantize_vec(const QuantizationParams* params, const float* in, const size_t n, u8* out) {
    if (n == 14) {
        const __m256 vmin   = _mm256_set1_ps(params->min_value);
        const __m256 vinv   = _mm256_set1_ps(1.0f / params->scale);
        const __m256 v0     = _mm256_setzero_ps();
        const __m256 v255   = _mm256_set1_ps(255.0f);

        __m256 a0 = _mm256_loadu_ps(in);
        __m256 a1 = _mm256_loadu_ps(in + 8);
        a0 = _mm256_mul_ps(_mm256_sub_ps(a0, vmin), vinv);
        a1 = _mm256_mul_ps(_mm256_sub_ps(a1, vmin), vinv);
        a0 = _mm256_min_ps(_mm256_max_ps(a0, v0), v255);
        a1 = _mm256_min_ps(_mm256_max_ps(a1, v0), v255);
        const __m256i i0 = _mm256_cvtps_epi32(a0);   
        const __m256i i1 = _mm256_cvtps_epi32(a1);

        const __m128i i0_lo = _mm256_castsi256_si128(i0);
        const __m128i i0_hi = _mm256_extracti128_si256(i0, 1);
        const __m128i i1_lo = _mm256_castsi256_si128(i1);
        const __m128i i1_hi = _mm256_extracti128_si256(i1, 1);
        const __m128i p16_a = _mm_packus_epi32(i0_lo, i0_hi);   
        const __m128i p16_b = _mm_packus_epi32(i1_lo, i1_hi);   
        const __m128i p8    = _mm_packus_epi16(p16_a, p16_b);   
        _mm_storeu_si128((__m128i*)out, p8);
        return;
    }

    for (size_t i = 0; i < n; i++) {
        const float q = (in[i] - params->min_value) / params->scale;
        out[i] = (u8)roundf(fmaxf(0.0f, fminf(255.0f, q)));
    }
}
