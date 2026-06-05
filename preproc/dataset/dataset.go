package dataset

import (
	"encoding/json"
	"fmt"
	"os"
)

const (
	Dim     = 14
	NumRefs = 3_000_000
)

type Vector [Dim]float32

type Ref struct {
	Vector Vector `json:"vector"`
	Label  string `json:"label"`
}

func LoadReferences(path string) ([NumRefs]Ref, error) {
	var refs [NumRefs]Ref
	bytes, err := os.ReadFile(path)
	if err != nil {
		return refs, err
	}

	if err := json.Unmarshal(bytes, &refs); err != nil {
		return refs, err
	}

	return refs, nil
}

type Test struct {
	N       int
	Vectors []Vector  `json:"vectors"`
	Flags   []bool    `json:"approved"`
	Scores  []float32 `json:"scores"`
}

func LoadTestDataset(path string) (Test, error) {
	bytes, err := os.ReadFile(path)
	if err != nil {
		return Test{}, err
	}

	var obj Test
	if err := json.Unmarshal(bytes, &obj); err != nil {
		return Test{}, err
	}

	obj.N = len(obj.Vectors)
	if len(obj.Flags) != obj.N || len(obj.Scores) != obj.N {
		return Test{}, fmt.Errorf("number of flags and scores do not match")
	}

	return obj, nil
}
