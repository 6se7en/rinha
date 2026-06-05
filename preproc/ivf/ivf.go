package main

import (
	"iter"
	"math"
	"math/rand"
	"runtime"
	"sort"
	"sync"
	"time"
)

type IVFOption func(*IVF)

func WithRand(rng *rand.Rand) IVFOption {
	return func(ivf *IVF) {
		ivf.rng = rng
	}
}

type IVF struct {
	vecs      FlatVecs
	centroids FlatVecs
	clusters  [][]int
	rng       *rand.Rand
	subs      []*IVF
	numLevels int
	level     int
}

func newIVF(vecs FlatVecs, options ...IVFOption) *IVF {
	ivf := &IVF{
		vecs: vecs,
		subs: nil,
	}
	for _, option := range options {
		option(ivf)
	}
	if ivf.rng == nil {
		ivf.rng = rand.New(rand.NewSource(time.Now().UnixNano()))
	}
	return ivf
}

func (ivf *IVF) distToCentroids(query []float32) []float32 {
	dists := make([]float32, ivf.centroids.n)
	distMulti(query, ivf.centroids.data, dists, ivf.centroids.n)
	return dists
}

func (ivf *IVF) findBestClusters(query []float32) int {
	closest := -1
	var minDist float32 = math.MaxFloat32
	for i, d := range ivf.distToCentroids(query) {
		if d < minDist {
			minDist = d
			closest = i
		}
	}

	return closest
}

func (ivf *IVF) searchBestClusters(query []float32, nprobes int) iter.Seq[int] {
	clusters := NewTree()

	dists := make([]float32, ivf.centroids.n)

	distMulti(query, ivf.centroids.data, dists, ivf.centroids.n)

	for i, d := range dists {
		clusters.Add(i, d)
		if clusters.Size() > nprobes {
			clusters.PopMax()
		}
	}

	return func(yield func(int) bool) {
		for id, _ := range clusters.Iter() {
			yield(id)
		}
	}
}

func (ivf *IVF) train(k int) {
	ids := make([]int, ivf.vecs.n)
	for i := range ivf.vecs.n {
		ids[i] = i
	}
	foo(ivf, ids, k)
}

func (ivf *IVF) kMeanOfCluster(ids []int) []float32 {
	var v [Dim]float32
	for _, id := range ids {
		for i := 0; i < Dim; i++ {
			v[i] += ivf.vecs.at(id)[i]
		}
	}
	n := float32(len(ids))
	for i := 0; i < Dim; i++ {
		v[i] /= n
	}
	return v[:]
}

func (ivf *IVF) variance() float32 {
	var total float32
	for c, ids := range ivf.clusters {
		centroid := ivf.centroids.at(c)
		for _, id := range ids {
			d := dist(ivf.vecs.at(id), centroid)
			total += d * d // squared Euclidean distance
		}
	}
	return total
}

func (ivf *IVF) findClosesCentroid(idx int, centroids FlatVecs, distances []float32) int {
	v := ivf.vecs.at(idx)
	distMulti(v, centroids.data, distances, centroids.n)

	closest := -1
	var minDist float32 = math.MaxFloat32
	for i, d := range distances[:centroids.n] {
		if d < minDist {
			minDist = d
			closest = i
		}
	}

	return closest
}

func (ivf *IVF) subdivide(k, z int) {
	ivf.numLevels = z
	var wg sync.WaitGroup
	bar(&wg, ivf, k, z, 1)
	wg.Wait()
}

func didClustersChange(a [][]int, b [][]int) bool {
	if len(a) != len(b) {
		return true
	}
	for i := range a {
		if len(a[i]) != len(b[i]) {
			return true
		}
	}
	for i := range a {
		x, y := a[i], b[i]
		sort.Ints(x)
		sort.Ints(y)
		for j := range x {
			if x[j] != y[j] {
				return true
			}
		}
	}
	return false
}

const parallelThreshold = 512

func foo(ivf *IVF, ids []int, k int) {
	if len(ids) == 0 {
		ivf.centroids = NewFlatVecs(0)
		ivf.clusters = make([][]int, k)
		return
	}
	// If fewer points than clusters, reduce k
	if len(ids) < k {
		k = len(ids)
	}
	ivf.centroids = NewFlatVecs(k)
	unique := make(map[int]struct{}, k)
	for i := range k {
	loop:
		idx := ivf.rng.Intn(len(ids))
		if _, ok := unique[idx]; ok {
			goto loop
		}
		ivf.centroids.set(i, ivf.vecs.at(ids[idx]))
		unique[idx] = struct{}{}
	}

	n := len(ids)
	nWorkers := runtime.NumCPU()
	if n < parallelThreshold {
		nWorkers = 1
	}

	// Pre-allocate assignment array (reused across iterations)
	assignments := make([]int, n)

	for loop := 0; loop < MaxLoops; loop++ {
		if nWorkers == 1 {
			// Serial path: no goroutines, no channels
			distances := make([]float32, k)
			for i, id := range ids {
				assignments[i] = ivf.findClosesCentroid(id, ivf.centroids, distances)
			}
		} else {
			// Parallel path: split work into chunks, no channels
			var wg sync.WaitGroup
			chunkSize := (n + nWorkers - 1) / nWorkers
			for w := range nWorkers {
				lo := w * chunkSize
				hi := lo + chunkSize
				if hi > n {
					hi = n
				}
				if lo >= hi {
					break
				}
				wg.Go(func() {
					distances := make([]float32, k)
					for i := lo; i < hi; i++ {
						assignments[i] = ivf.findClosesCentroid(ids[i], ivf.centroids, distances)
					}
				})
			}
			wg.Wait()
		}

		clusters := make([][]int, k)
		for i, id := range ids {
			c := assignments[i]
			clusters[c] = append(clusters[c], id)
		}

		if !didClustersChange(clusters, ivf.clusters) {
			break
		}

		kmeans := NewFlatVecs(k)
		var wg sync.WaitGroup
		for i := range k {
			idx := i
			wg.Go(func() {
				kmeans.set(idx, ivf.kMeanOfCluster(clusters[idx]))
			})
		}
		wg.Wait()

		ivf.centroids = kmeans
		ivf.clusters = clusters
	}
}

func bar(wg *sync.WaitGroup, ivf *IVF, k int, z int, lvl int) {
	wg.Add(1)
	defer wg.Done()

	if lvl > z {
		return
	}

	nClusters := len(ivf.clusters)
	ivf.subs = make([]*IVF, nClusters)
	ivf.level = lvl
	ivf.numLevels = z

	// Generate independent seeds from parent rng before spawning goroutines
	seeds := make([]int64, nClusters)
	for i := range nClusters {
		seeds[i] = ivf.rng.Int63()
	}

	for i := range nClusters {
		idx := i
		wg.Go(func() {
			if len(ivf.clusters[idx]) == 0 {
				ivf.subs[idx] = &IVF{
					vecs: ivf.vecs,
				}
				return
			}
			sub := &IVF{
				vecs:      ivf.vecs,
				rng:       rand.New(rand.NewSource(seeds[idx])),
				numLevels: z,
				level:     lvl + 1,
			}
			ivf.subs[idx] = sub
			foo(sub, ivf.clusters[idx], k)

			bar(wg, sub, k, z, lvl+1)
		})
	}
}
