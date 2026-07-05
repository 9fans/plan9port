package main

import "io"

// The Go Buffer is a flat []rune. The C version (buff.c) pages fixed-size
// blocks through a temp file (disk.c) purely to bound memory; that paging has
// no observable effect, so it is dropped, as in the sam port. The
// insert/delete/read/load API is preserved as methods.
type Buffer struct {
	c []rune // the data
}

func (b *Buffer) nc() int { return len(b.c) }

// Insert inserts s at rune position q0 (bufinsert).
func (b *Buffer) Insert(q0 int, s []rune) {
	if q0 > b.nc() {
		// unreachable
		bug("bufinsert")
	}
	tail := append([]rune(nil), b.c[q0:]...)
	b.c = append(b.c[:q0], s...)
	b.c = append(b.c, tail...)
}

// Delete removes runes [q0,q1) (bufdelete).
func (b *Buffer) Delete(q0, q1 int) {
	if !(q0 <= q1 && q0 <= b.nc() && q1 <= b.nc()) {
		// unreachable
		bug("bufdelete")
	}
	b.c = append(b.c[:q0], b.c[q1:]...)
}

// Read copies len(s) runes starting at q0 into s (bufread).
func (b *Buffer) Read(q0 int, s []rune) {
	n := len(s)
	if !(q0 <= b.nc() && q0+n <= b.nc()) {
		// unreachable
		bug("bufread")
	}
	copy(s, b.c[q0:q0+n])
}

// Reset empties the buffer (bufreset).
func (b *Buffer) Reset() { b.c = b.c[:0] }

// Close releases the buffer (bufclose).
func (b *Buffer) Close() { b.c = nil }

// Load reads all of fd, converts it to runes (eliding NULs and noting them in
// *nulls), inserts the result at q0, and returns the number of runes inserted
// (bufload + loadfile).
func (b *Buffer) Load(q0 int, fd io.Reader, nulls *bool) int {
	if q0 > b.nc() {
		// unreachable
		bug("bufload")
	}
	data, err := io.ReadAll(fd)
	if err != nil {
		// The -D load command only opens regular files, which read without error.
		// untested
		return 0
	}
	var runes []rune
	cvttorunes(data, len(data), &runes, nulls)
	b.Insert(q0, runes)
	return len(runes)
}
