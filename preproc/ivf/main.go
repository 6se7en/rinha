package main

import (
	"bufio"
	"fmt"
	"math/rand"
	"os"
	"preproc/dataset"
	"runtime"
	"sync"
	"sync/atomic"
	"time"
)

const (
	MaxLoops  = 1000
	TopK      = 5
	Threshold = 0.6
	Dim       = 16
	Seed      = 0x1234567890abcdef
)

// Index shape. Higher K with fewer LEVELS gives a higher accuracy ceiling at
// more calcs; deeper trees (e.g. K=8 LEVELS=6) are calc-cheaper but cap around
// 99.4%. See the frontier notes below.
const (
	K      = 100
	LEVELS = 1
)

// Operating point: BEAM navigation paths kept per level, NPROBES finest-level
// subclusters brute-forced. This is the result reported as the headline metric.
const (
	BEAM    = 8
	NPROBES = 8
)

// When set, write the trained index (centroids + leaf vectors + fraud flags)
// to OutPath after building it, for the downstream runtime to load.
const (
	OutPath = "ivf.bin"
)

// Measured accuracy / total-dist-calcs frontier (deterministic Seed), for
// picking an operating point:
//
//	K=8  L=6  beam=1  np=8   -> 96.93% @   4.1M  (cheap, greedy)
//	K=8  L=6  beam=8  np=8   -> 99.37% @  22.0M  (best accuracy/calc knee)
//	K=64 L=2  beam=4  np=32  -> 99.88% @  53.9M
//	K=64 L=2  beam=8  np=64  -> 99.98% @ 103.8M  (default below)
//	K=64 L=2  beam=16 np=128 -> 99.996% @ 203.7M (~2 queries short of 100%)
//	K=32 L=2  beam=32 np=256 -> 100.00% @ 1.71B  (full accuracy, very costly)

func main() {
	test, err := dataset.LoadTestDataset("test.json")
	if err != nil {
		panic(err)
	}
	fmt.Println("test vectors:", test.N)

	refs, err := dataset.LoadReferences("rinha/resources/references.json")
	if err != nil {
		panic(err)
	}
	fmt.Println("references:", len(refs))

	vecs := toVecs(refs)
	flags := make([]bool, dataset.NumRefs)
	for i, r := range refs {
		flags[i] = r.Label == "fraud"
	}

	t0 := time.Now()
	rng := rand.New(rand.NewSource(Seed))

	for k := 64; k <= 128; k++ {
		ivf := newIVF(vecs, WithRand(rng))
		ivf.train(k)
		ivf.subdivide(k, LEVELS)
		fmt.Printf("trained k=%d levels=%d in %s\n\n", k, LEVELS, time.Since(t0))

		outPath := fmt.Sprintf("ivf-%d.bin", k)
		if err := dumpIndex(outPath, ivf, flags); err != nil {
			panic(err)
		}
	}

	// Headline operating point.
	//ap, calcs := evaluate(test, flags, ivf, []int{BEAM}, NPROBES)
	//fmt.Printf("Operating point: beam=%d nprobes=%d\n", BEAM, NPROBES)
	//fmt.Printf("\tApproval accuracy: %f%%\n", ap*100)
	//fmt.Printf("\tN dist calcs: %d\n\n", calcs)

	//return

	// Within-config frontier (no retraining) for context.
	//fmt.Printf("%-6s %-8s %-12s %-12s\n", "beam", "nprobes", "approval%", "Ndistcalcs")
	//for _, beam := range []int{1, 2, 4, 8, 16} {
	//	for _, np := range []int{16, 32, 64, 128} {
	//		ap, calcs := evaluate(test, flags, ivf, []int{beam}, np)
	//		fmt.Printf("%-6d %-8d %-12.4f %-12d\n", beam, np, ap*100, calcs)
	//	}
	//}
}

// dumpIndex writes the trained index to path: tree of centroids plus, at each
// leaf, the member vectors and their fraud flags (everything the downstream
// runtime needs to reproduce the beam/nprobes search without references.json).
func dumpIndex(path string, ivf *IVF, flags []bool) error {
	t0 := time.Now()
	f, err := os.Create(path)
	if err != nil {
		return err
	}
	defer f.Close()

	w := bufio.NewWriterSize(f, 1<<20)
	if err := Serialize(w, ivf, flags); err != nil {
		return err
	}
	if err := w.Flush(); err != nil {
		return err
	}
	fi, _ := f.Stat()
	fmt.Printf("wrote %s (%d bytes) in %s\n\n", path, fi.Size(), time.Since(t0))
	return nil
}

func findNumLevels(k, n int) int {
	z := k
	for i := 1; i < n; i++ {
		if n/z < 8 {
			return i - 1
		}
		z *= k
	}
	return -1
}

// evaluate runs all test queries (parallel across queries, single-threaded per
// query) and returns approval accuracy and the total number of distance calcs.
func evaluate(test dataset.Test, flags []bool, ivf *IVF, schedule []int, nprobes int) (float64, uint64) {
	var correctApprovals int64
	var totalCalcs uint64

	ch := make(chan int, test.N)
	var wg sync.WaitGroup
	for range runtime.NumCPU() {
		wg.Go(func() {
			st := newSearchState(K)
			query := make([]float32, Dim)
			var localCalcs uint64
			var localCorrect int64
			for i := range ch {
				copy(query, test.Vectors[i][:])
				neighbors, c := ivf.search(st, query, schedule, nprobes, TopK)
				localCalcs += uint64(c)

				var nfraudes int
				for _, id := range neighbors {
					if flags[id] {
						nfraudes++
					}
				}
				score := float32(nfraudes) / TopK
				approved := score < Threshold
				if approved == test.Flags[i] {
					localCorrect++
				}
			}
			atomic.AddUint64(&totalCalcs, localCalcs)
			atomic.AddInt64(&correctApprovals, localCorrect)
		})
	}

	for i := range test.N {
		ch <- i
	}
	close(ch)
	wg.Wait()

	approvalAccuracy := float64(correctApprovals) / float64(test.N)
	return approvalAccuracy, totalCalcs
}

func toVecs(refs [dataset.NumRefs]dataset.Ref) FlatVecs {
	vecs := NewFlatVecs(len(refs))
	for i, r := range refs {
		vecs.set(i, r.Vector[:])
	}
	return vecs
}
