package main

import (
	"math"
	"math/rand"
	"testing"
)

func TestTree(t *testing.T) {
	const N = 1000
	values := make([]float32, N)
	for i := range values {
		values[i] = rand.Float32()
	}

	tree := NewTree()
	t.Run("Add", func(t *testing.T) {
		for i := range N {
			tree.Add(i, values[i])
		}
	})

	t.Run("Size", func(t *testing.T) {
		size := tree.Size()
		if size != N {
			t.Fatalf("tree.Size() = %d, want %d", size, N)
		}
	})

	t.Run("Iterate", func(t *testing.T) {
		count := 0
		var prev float32 = -math.MaxFloat32
		ids := make(map[int]float32, N)

		for i, v := range tree.Iter() {
			if v < prev {
				t.Fatalf("tree.Size() = %f, want >= %f", v, prev)
			}
			if _, ok := ids[i]; ok {
				t.Fatalf("duplicate id: %d", i)
			}
			ids[i] = v
			count++
		}

		if count != N {
			t.Fatalf("tree.Size() = %d, want %d", count, N)
		}
	})

	t.Run("PopMax", func(t *testing.T) {
		for range N {
			tree.PopMax()
		}
		if tree.Size() != 0 {
			t.Fatalf("tree.Size() = %d, want 0", tree.Size())
		}
	})
}
