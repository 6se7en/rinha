package main

import (
	"math"
	"math/rand"
	"testing"
)

const eps = 1e-4

const N = 1000

var query []float32
var vecs [N * Dim]float32
var results [N]float32

func init() {
	randVec(query)
	randVec(vecs[:])
}

func randVec(vec []float32) {
	for i := range vec {
		vec[i] = rand.Float32()
	}
}

func BenchmarkDistNative(b *testing.B) {
	for i := 0; i < b.N; i++ {
		//dist(query, vecs[i%N])
	}
}

func BenchmarkDistMulti(b *testing.B) {
	for i := 0; i < b.N; i++ {
		distMulti(query, vecs[:], results[:], N)
	}
}

func BenchmarkDistSIMD(b *testing.B) {
	for i := 0; i < b.N; i++ {
		distMulti(query, vecs[:], results[:], N)
	}
}

func TestDist(t *testing.T) {
	var expected [N]float32
	//for i := range N {
	//expected[i] = dist(query, vecsfixed[i])
	//}

	t.Run("multi", func(t *testing.T) {
		var results [N]float32
		distMulti(query, vecs[:], results[:], N)
		for i := range N {
			if math.Abs(float64(results[i]-expected[i])) > eps {
				t.Errorf("Dist(%d) = %f, want %f", i, results[i], expected[i])
				return
			}
		}
	})

	t.Run("simd", func(t *testing.T) {
		var results [N]float32
		//distSimd(query, vecs[:], results[:], N)
		for i := range N {
			if math.Abs(float64(results[i]-expected[i])) > eps {
				t.Errorf("Dist(%d) = %f, want %f", i, results[i], expected[i])
				return
			}
		}
	})
}
