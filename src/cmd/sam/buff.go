package main

import (
	"io"
	"slices"
	"unicode/utf8"
)

// Buffer holds the runes of a file or transcript (sam.h struct Buffer).
// The C version pages blocks to a temp file; the Go port keeps a flat
// in-memory slice instead, at least for now.
type Buffer struct {
	c []rune
}

// nc returns the number of runes in b.
func (b *Buffer) nc() Posn { return Posn(len(b.c)) }

// The buffer routines that guard an invariant are *Sam methods so they can
// reach s.panic (C's buff.c calls the global panic() directly); the receiver is
// otherwise unused. bufreset/bufclose stay free.

// bufinsert inserts str into b before position q0.
func (s *Sam) bufinsert(b *Buffer, q0 Posn, str []rune) {
	if q0 > b.nc() {
		// unreachable
		s.panic("internal error: bufinsert")
	}
	b.c = slices.Insert(b.c, int(q0), str...)
}

// bufdelete removes the runes in [q0, q1) from b.
func (s *Sam) bufdelete(b *Buffer, q0, q1 Posn) {
	if !(q0 <= q1 && q0 <= b.nc() && q1 <= b.nc()) {
		// unreachable
		s.panic("internal error: bufdelete")
	}
	b.c = append(b.c[:q0], b.c[q1:]...)
}

// bufread copies len(str) runes starting at q0 from b into str.
func (s *Sam) bufread(b *Buffer, q0 Posn, str []rune) {
	n := Posn(len(str))
	if !(q0 <= b.nc() && q0+n <= b.nc()) {
		// unreachable
		s.panic("bufread: internal error")
	}
	copy(str, b.c[q0:q0+n])
}

// bufreset empties b, keeping its backing storage for reuse.
func bufreset(b *Buffer) { b.c = b.c[:0] }

// bufclose releases b's storage.
func bufclose(b *Buffer) { b.c = nil }

// bufload reads all of r, converts it to runes (eliding NULs), inserts the
// result at q0, and returns the number of runes inserted (C bufload).
func (s *Sam) bufload(b *Buffer, q0 Posn, r io.Reader, nulls *bool) Posn {
	if q0 > b.nc() {
		// unreachable
		s.panic("internal error: bufload")
	}
	data, err := io.ReadAll(r)
	if err != nil {
		// untested
		s.error(Ebufload)
	}
	runes := make([]rune, 0, len(data))
	cvttorunes(data, len(data), &runes, nulls)
	s.bufinsert(b, q0, runes)
	return Posn(len(runes))
}

// cvttorunes converts the first n bytes of p to runes, appending to *r and
// setting *nulls if any NUL bytes were elided (util.c cvttorunes, adapted to
// decode the whole input at once).
func cvttorunes(p []byte, n int, r *[]rune, nulls *bool) {
	for i := 0; i < n; {
		c := p[i]
		if c < utf8.RuneSelf {
			i++
			if c != 0 {
				*r = append(*r, rune(c))
			} else if nulls != nil {
				*nulls = true
			}
			continue
		}
		ch, w := utf8.DecodeRune(p[i:])
		i += w
		*r = append(*r, ch)
	}
}
