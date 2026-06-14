package main

import "unsafe"

func boolToInt(b bool) int {
	if b {
		return 1
	}
	return 0
}

// cond returns a if t is true, else b. A small ternary helper; both a and b
// are always evaluated.
func cond[T any](t bool, a, b T) T {
	if t {
		return a
	}
	return b
}

// windex returns the index of layer l within tx.l. l must point into tx.l
// (mirroring C's l - &t->l[0]).
func (t *Term) windex(tx *Text, l *Flayer) int {
	return int((uintptr(unsafe.Pointer(l)) - uintptr(unsafe.Pointer(&tx.l[0]))) / unsafe.Sizeof(tx.l[0]))
}
