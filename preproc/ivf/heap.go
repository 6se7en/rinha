package main

import "container/heap"

type Pair[A, B any] struct {
	left  A
	right B
}

type Heap []Pair[int, float32]

func (h Heap) Len() int {
	return len(h)
}

func (h Heap) Less(i, j int) bool {
	return h[i].right > h[j].right // max-heap: largest distance at root, evicted first
}

func (h Heap) Swap(i, j int) {
	h[i], h[j] = h[j], h[i]
}

func (h *Heap) Push(x any) {
	*h = append(*h, x.(Pair[int, float32]))
}

func (h *Heap) Pop() any {
	old := *h
	n := len(old)
	x := old[n-1]
	*h = old[0 : n-1]
	return x
}

var _ heap.Interface = (*Heap)(nil)
