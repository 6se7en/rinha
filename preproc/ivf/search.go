package main

import (
	"sort"
)

// beamEntry is a candidate node during hierarchical navigation.
type beamEntry struct {
	node *IVF
	d    float32
}

// leafCand is a finest-level subcluster (slice of vector ids) with its
// centroid distance to the query.
type leafCand struct {
	ids []int
	d   float32
}

// searchState holds reusable scratch buffers so that a single goroutine can
// run many queries without re-allocating.
type searchState struct {
	beam  []beamEntry
	next  []beamEntry
	cands []leafCand
	dbuf  []float32
	best  []Pair[int, float32] // current topK, sorted ascending by distance
}

func newSearchState(maxCentroids int) *searchState {
	return &searchState{
		dbuf: make([]float32, maxCentroids),
		best: make([]Pair[int, float32], 0, 8),
	}
}

// search returns the topK nearest neighbor ids for query together with the
// number of distance computations performed (centroid + vector).
//
// schedule[s] is the beam width kept after descent step s (the last entry is
// reused for any deeper steps), letting the beam be wide near the top (where a
// wrong branch is catastrophic) and narrow deeper. nprobes controls how many
// finest-level subclusters are brute-forced.
func (ivf *IVF) search(st *searchState, query []float32, schedule []int, nprobes, topK int) ([]int, int) {
	calcs := 0

	st.beam = st.beam[:0]
	st.beam = append(st.beam, beamEntry{ivf, 0})

	// Descend while the current frontier consists of internal nodes.
	step := 0
	for len(st.beam) > 0 && st.beam[0].node.subs != nil {
		st.next = st.next[:0]
		for _, e := range st.beam {
			n := e.node
			cn := n.centroids.n
			if cn == 0 {
				continue
			}
			if cn > len(st.dbuf) {
				st.dbuf = make([]float32, cn)
			}
			distMulti(query, n.centroids.data, st.dbuf[:cn], cn)
			calcs += cn
			for i := 0; i < cn; i++ {
				if s := n.subs[i]; s != nil {
					st.next = append(st.next, beamEntry{s, st.dbuf[i]})
				}
			}
		}
		si := step
		if si >= len(schedule) {
			si = len(schedule) - 1
		}
		w := schedule[si]
		if len(st.next) > w {
			sort.Slice(st.next, func(a, b int) bool { return st.next[a].d < st.next[b].d })
			st.next = st.next[:w]
		}
		st.beam, st.next = st.next, st.beam
		step++
	}

	// Frontier now holds leaf nodes. Collect their subclusters as candidates.
	st.cands = st.cands[:0]
	for _, e := range st.beam {
		n := e.node
		cn := n.centroids.n
		if cn == 0 {
			continue
		}
		distMulti(query, n.centroids.data, st.dbuf[:cn], cn)
		calcs += cn
		for i := 0; i < cn; i++ {
			if len(n.clusters[i]) > 0 {
				st.cands = append(st.cands, leafCand{n.clusters[i], st.dbuf[i]})
			}
		}
	}
	if len(st.cands) > nprobes {
		sort.Slice(st.cands, func(a, b int) bool { return st.cands[a].d < st.cands[b].d })
		st.cands = st.cands[:nprobes]
	}

	// Brute force the selected subclusters, keeping the topK closest vectors.
	st.best = st.best[:0]
	for _, c := range st.cands {
		for _, id := range c.ids {
			d := dist(ivf.vecs.at(id), query)
			st.insert(id, d, topK)
		}
		calcs += len(c.ids)
	}

	ids := make([]int, len(st.best))
	for i, p := range st.best {
		ids[i] = p.left
	}
	return ids, calcs
}

// insert maintains st.best as the topK smallest-distance pairs, sorted ascending.
func (st *searchState) insert(id int, d float32, topK int) {
	n := len(st.best)
	if n >= topK && d >= st.best[n-1].right {
		return
	}
	// find insertion position
	pos := sort.Search(n, func(i int) bool { return st.best[i].right >= d })
	if n < topK {
		st.best = append(st.best, Pair[int, float32]{})
	}
	copy(st.best[pos+1:], st.best[pos:])
	if pos < len(st.best) {
		st.best[pos] = Pair[int, float32]{id, d}
	}
}
