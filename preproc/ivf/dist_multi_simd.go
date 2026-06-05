//go:build !darwin

package main

import "C"
import (
	"unsafe"
)

/*
#cgo CFLAGS: -O3 -mavx2 -mfma
#cgo LDFLAGS: -lm
#include <immintrin.h>
#include <math.h>

float dist(const float *a, const float *b, float* c, const size_t d, const size_t n) {
	for (size_t j = 0; j < n; j++) {
		__m256 sum = _mm256_setzero_ps();
		for (size_t i = 0; i < d; i += 8) {
			__m256 va = _mm256_loadu_ps(a + i);
			__m256 vb = _mm256_loadu_ps(b + (j * d + i));
			__m256 diff = _mm256_sub_ps(va, vb);
			sum = _mm256_fmadd_ps(diff, diff, sum);
		}
		// Horizontal add to get the final sum
		__m128 low  = _mm256_castps256_ps128(sum);
		__m128 high = _mm256_extractf128_ps(sum, 1);
		__m128 sum128 = _mm_add_ps(low, high);
		sum128 = _mm_hadd_ps(sum128, sum128);
		sum128 = _mm_hadd_ps(sum128, sum128);
		c[j] = sqrtf(_mm_cvtss_f32(sum128));
	}
}
*/
import "C"

func distMulti(q []float32, vecs []float32, dists []float32, N int) {
	C.dist(
		(*C.float)(unsafe.Pointer(&q[0])),
		(*C.float)(unsafe.Pointer(&vecs[0])),
		(*C.float)(unsafe.Pointer(&dists[0])),
		C.size_t(Dim),
		C.size_t(N),
	)
}
