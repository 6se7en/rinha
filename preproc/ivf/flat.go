package main

type FlatVecs struct {
	data []float32
	n    int
}

func (v *FlatVecs) at(idx int) []float32 {
	off := idx * Dim
	return v.data[off : off+Dim]
}

func (v *FlatVecs) set(i int, vec []float32) {
	copy(v.data[i*Dim:], vec)
}

func (v *FlatVecs) from(i int) []float32 {
	return v.data[i*Dim:]
}

func NewFlatVecs(n int) FlatVecs {
	return FlatVecs{
		data: make([]float32, Dim*n),
		n:    n,
	}
}
