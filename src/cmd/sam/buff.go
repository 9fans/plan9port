package main

import "io"

// Buffer holds the runes of a file or transcript (sam.h struct Buffer).
// The C version pages blocks to a temp file; the Go port keeps a flat
// in-memory slice, which is observably identical.
type Buffer struct {
	c []Rune
}

// nc returns the number of runes in b.
func (b *Buffer) nc() int { return len(b.c) }

// bufinsert inserts the first n runes of s into b before position q0.
func bufinsert(b *Buffer, q0 Posn, s []Rune, n int) {
	if q0 > b.nc() {
		// unreachable
		bug("bufinsert")
	}
	tail := append([]Rune(nil), b.c[q0:]...)
	b.c = append(b.c[:q0], s[:n]...)
	b.c = append(b.c, tail...)
}

// bufdelete removes the runes in [q0, q1) from b.
func bufdelete(b *Buffer, q0, q1 Posn) {
	if !(q0 <= q1 && q0 <= b.nc() && q1 <= b.nc()) {
		// unreachable
		bug("bufdelete")
	}
	b.c = append(b.c[:q0], b.c[q1:]...)
}

// bufread copies n runes starting at q0 from b into s.
func bufread(b *Buffer, q0 Posn, s []Rune, n int) {
	if !(q0 <= b.nc() && q0+n <= b.nc()) {
		// unreachable
		bug("bufread")
	}
	copy(s[:n], b.c[q0:q0+n])
}

// bufreset empties b, keeping its backing storage for reuse.
func bufreset(b *Buffer) { b.c = b.c[:0] }

// bufclose releases b's storage.
func bufclose(b *Buffer) { b.c = nil }

// bufload reads all of r, converts it to runes (eliding NULs), inserts the
// result at q0, and returns the number of runes inserted (C bufload).
func bufload(b *Buffer, q0 Posn, r io.Reader, nulls *bool) int {
	if q0 > b.nc() {
		// unreachable
		bug("bufload")
	}
	data, err := io.ReadAll(r)
	if err != nil {
		// untested
		fail(Ebufload)
	}
	runes := make([]Rune, 0, len(data))
	cvttorunes(data, len(data), &runes, nulls)
	bufinsert(b, q0, runes, len(runes))
	return len(runes)
}

// cvttorunes converts the first n bytes of p to runes, appending to *r and
// setting *nulls if any NUL bytes were elided (util.c cvttorunes, adapted to
// decode the whole input at once).
func cvttorunes(p []byte, n int, r *[]Rune, nulls *bool) {
	for i := 0; i < n; {
		c := p[i]
		if c < Runeself {
			i++
			if c != 0 {
				*r = append(*r, Rune(c))
			} else if nulls != nil {
				*nulls = true
			}
			continue
		}
		ch, w := decodeRune(p[i:])
		i += w
		*r = append(*r, ch)
	}
}
