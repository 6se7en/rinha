package main

import (
	"math"
	"preproc/dataset"
)

// dist computes the Euclidean distance between two vectors.
func dist(a, b []float32) float32 {
	var sum float64
	for i := 0; i < dataset.Dim; i++ {
		diff := float64(a[i]) - float64(b[i])
		sum += diff * diff
	}
	return float32(math.Sqrt(sum))
}
