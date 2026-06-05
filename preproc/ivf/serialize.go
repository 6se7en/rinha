package main

import (
	"bufio"
	"iter"
	"preproc/quantization"
	"preproc/serde"
)

func Serialize(w *bufio.Writer, root *IVF, flags []bool) error {
	pc := quantization.ComputeGlobalParams(iterLevelCentroids(root))
	if err := writeQuantizationParams(w, pc); err != nil {
		return err
	}
	pv := quantization.ComputeGlobalParams(iterVectors(root))
	if err := writeQuantizationParams(w, pv); err != nil {
		return err
	}
	if err := serde.Varint(w, root.numLevels); err != nil {
		return err
	}
	if err := writeIVF(w, root, flags, pc, pv); err != nil {
		return err
	}
	return nil
}

func writeQuantizationParams(w *bufio.Writer, pc quantization.Params) error {
	if err := serde.Float32(w, pc.MinVal); err != nil {
		return err
	}
	if err := serde.Float32(w, pc.Scale); err != nil {
		return err
	}
	return nil
}

func iterVectors(root *IVF) iter.Seq[[]float32] {
	return func(yield func([]float32) bool) {
		yield(root.vecs.data)
	}
}

func iterLevelCentroids(root *IVF) iter.Seq[[]float32] {
	return func(yield func([]float32) bool) {
		stack := []*IVF{root}
		var cur *IVF
		for len(stack) > 0 {
			cur, stack = stack[len(stack)-1], stack[:len(stack)-1]
			yield(cur.centroids.data)
			for i := range cur.subs {
				stack = append(stack, cur.subs[i])
			}
		}
	}
}

func writeIVF(w *bufio.Writer, ivf *IVF, flags []bool, pc, pv quantization.Params) error {
	nsubs := len(ivf.subs)
	if err := serde.Varint(w, nsubs); err != nil {
		return err
	}
	if err := writeCentroids(w, ivf.centroids, pc); err != nil {
		return err
	}
	if nsubs == 0 {
		return writeClusters(w, ivf, flags, pv)
	}
	for i := range ivf.subs {
		if err := writeIVF(w, ivf.subs[i], flags, pc, pv); err != nil {
			return err
		}
	}
	return nil
}

func writeCentroids(w *bufio.Writer, vecs FlatVecs, pc quantization.Params) error {
	if err := serde.Varint(w, vecs.n); err != nil {
		return err
	}
	return writeVecQuantized(w, vecs.data, pc)
}

func writeClusters(w *bufio.Writer, ivf *IVF, flags []bool, pv quantization.Params) error {
	nclusters := len(ivf.clusters)
	if err := serde.Varint(w, nclusters); err != nil {
		return err
	}
	for i := range nclusters {
		ids := ivf.clusters[i]
		if err := serde.Varint(w, len(ids)); err != nil {
			return err
		}
		for _, id := range ids {
			if err := writeVecQuantized(w, ivf.vecs.at(id), pv); err != nil {
				return err
			}
		}
		fb := make([]byte, len(ids))
		for i, id := range ids {
			if flags[id] {
				fb[i] = 1
			}
		}
		if _, err := w.Write(fb); err != nil {
			return err
		}
	}
	return nil
}

func writeVecQuantized(w *bufio.Writer, v []float32, p quantization.Params) error {
	for i := range v {
		if err := w.WriteByte(quantization.Quantize(v[i], p)); err != nil {
			return err
		}
	}
	return nil
}
