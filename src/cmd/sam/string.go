package main

import "slices"

const STRSIZE = 2 * BLOCKSIZE

// empty is the empty rune slice.
var empty = []rune{}

// extendString/appendString/insertString are *Sam methods because they enforce
// STRSIZE and panic Etoolong via s.error, exactly as String.c's Strinsure calls
// the global error(). Keeping them on *Sam lets every error site roll back in
// place through hiccough.
func (s *Sam) extendString(n Posn) {
	if n > STRSIZE {
		s.error(Etoolong)
	}
}

// appendString appends c to r. Like append, the result must be stored back:
// r = s.appendString(r, c).
func (s *Sam) appendString(r []rune, c rune) []rune {
	s.extendString(Posn(len(r)) + 1)
	return append(r, c)
}

// insertString inserts q into r before position p0. Like append, the result must
// be stored back: r = s.insertString(r, q, p0).
func (s *Sam) insertString(r, q []rune, p0 Posn) []rune {
	s.extendString(Posn(len(r)) + Posn(len(q)))
	return slices.Insert(r, int(p0), q...)
}

// runesCompare compares a and b rune by rune; a shorter prefix sorts first.
func runesCompare(a, b []rune) int {
	na, nb := len(a), len(b)
	for i := 0; i < na && i < nb; i++ {
		if c := int(a[i]) - int(b[i]); c != 0 {
			return c
		}
	}
	return na - nb
}

// runesHasPrefix reports whether s begins with pre (C Strispre).
func runesHasPrefix(s, pre []rune) bool {
	return len(s) >= len(pre) && slices.Equal(s[:len(pre)], pre)
}
