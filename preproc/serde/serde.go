package serde

import (
	"bufio"
	"encoding/binary"
)

func Float32(w *bufio.Writer, val float32) error {
	return binary.Write(w, binary.LittleEndian, val)
}

func UInt32(w *bufio.Writer, u uint32) error {
	return binary.Write(w, binary.LittleEndian, u)
}

func Varint(w *bufio.Writer, u int) error {
	if u < 0 {
		panic("u must be >= 0")
	}
	for u >= 0x80 {
		if err := w.WriteByte(byte(u&0x7F) | 0x80); err != nil {
			return err
		}
		u >>= 7
	}
	return w.WriteByte(byte(u))
}

func Slice[T any](w *bufio.Writer, list []T) error {
	for i := range list {
		if err := binary.Write(w, binary.LittleEndian, list[i]); err != nil {
			return err
		}
	}
	return nil
}
