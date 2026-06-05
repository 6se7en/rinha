package quantization

import (
	"iter"
	"math"
)

// Params holds a single min/scale pair computed over an entire dataset.
type Params struct {
	MinVal float32
	Scale  float32
}

// ComputeGlobalParams scans all vectors and returns shared quantization params.
func ComputeGlobalParams(vecs iter.Seq[[]float32]) Params {
	var mn float32 = math.MaxFloat32
	var mx float32 = -math.MaxFloat32
	for v := range vecs {
		for _, x := range v {
			if x < mn {
				mn = x
			}
			if x > mx {
				mx = x
			}
		}
	}
	scale := (mx - mn) / 255.0
	if scale == 0 {
		scale = 1
	}
	return Params{MinVal: mn, Scale: scale}
}

// QuantizeVector quantizes a vector using a pre-computed global param set.
func QuantizeVector(src []float32, p Params) []uint8 {
	out := make([]uint8, len(src))
	for i, v := range src {
		out[i] = Quantize(v, p)
	}
	return out
}

func Quantize(f float32, p Params) uint8 {
	q := (f - p.MinVal) / p.Scale
	if q < 0 {
		q = 0
	} else if q > 255 {
		q = 255
	}
	return uint8(math.Round(float64(q)))
}
