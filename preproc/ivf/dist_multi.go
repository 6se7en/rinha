//go:build darwin

package main

import "math"

func distMulti(q []float32, vecs []float32, dists []float32, N int) {
	for i := range N {
		var sum, diff float64
		for j := range Dim {
			diff = float64(q[j]) - float64(vecs[i*Dim+j])
			sum += diff * diff
		}
		dists[i] = float32(math.Sqrt(sum))
	}
}
