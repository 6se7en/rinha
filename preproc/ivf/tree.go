package main

import (
	"iter"
	"sync"
)

var nodePool = sync.Pool{
	New: func() interface{} {
		return &Node{
			ids: make([]int, 1),
		}
	},
}

type Node struct {
	ids         []int
	value       float32
	parent      *Node
	left, right *Node
}

func (n *Node) removeChildren(children *Node) {
	if n.left == children {
		n.left = nil
	} else if n.right == children {
		n.right = nil
	} else {
		panic("family ties")
	}
}

func (n *Node) addChildren(children *Node) {
	if n.left == nil {
		n.left = children
	} else if n.right == nil {
		n.right = children
	} else {
		panic("family ties")
	}
}

type Tree struct {
	root     *Node
	min, max *Node
	size     int
}

func NewTree() *Tree {
	return &Tree{}
}

func (t *Tree) Add(id int, value float32) {
	t.size++

	if t.root == nil {
		t.root = t.newNode(id, value)
		return
	}

	current := t.root
	parent := current
	for current != nil {
		parent = current
		if value > current.value {
			current = current.right
		} else if value < current.value {
			current = current.left
		} else {
			current.ids = append(current.ids, id)
			return
		}
	}

	node := t.newNode(id, value)
	if value > parent.value {
		parent.right = node
	} else if value < parent.value {
		parent.left = node
	}
	node.parent = parent
}

func (t *Tree) Iter() iter.Seq2[int, float32] {
	return func(yield func(int, float32) bool) {
		current := t.min
		for current != nil {
			for _, id := range current.ids {
				if !yield(id, current.value) {
					return
				}
			}
			// In-order successor: go right then left as far as possible,
			// or climb up until we arrive from a left branch.
			if current.right != nil {
				current = current.right
				for current.left != nil {
					current = current.left
				}
			} else {
				for current.parent != nil && current == current.parent.right {
					current = current.parent
				}
				current = current.parent
			}
		}
	}
}

func (t *Tree) Size() int {
	return t.size
}

func (t *Tree) PopMax() {
	if t.max == nil {
		return
	}

	old := t.max
	t.size -= len(old.ids)

	if old == t.min {
		t.min = nil
	}

	// Lift old.left (if any) into old's position.
	// t.max is always the rightmost node, so it is always its parent's right child (or the root).
	if old.left != nil {
		old.left.parent = old.parent
	}
	if old.parent == nil {
		t.root = old.left
	} else {
		old.parent.right = old.left // old is always a right child since it's the max
	}

	// New max: rightmost of old.left's subtree, or old.parent if no left child.
	if old.left != nil {
		newMax := old.left
		for newMax.right != nil {
			newMax = newMax.right
		}
		t.max = newMax
	} else {
		t.max = old.parent
	}

	t.recycle(old)
}

func (t *Tree) newNode(id int, value float32) *Node {
	n := nodePool.Get().(*Node)
	n.ids[0] = id
	n.value = value

	if t.min == nil || value < t.min.value {
		t.min = n
	}
	if t.max == nil || value > t.max.value {
		t.max = n
	}

	return n
}

func (t *Tree) recycle(node *Node) {
	node.left, node.right, node.parent = nil, nil, nil
	if len(node.ids) > 1 {
		node.ids = node.ids[:1]
	}
	nodePool.Put(node)
}
