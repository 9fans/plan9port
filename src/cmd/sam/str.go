package main

// String is a counted array of runes (sam.h struct String). The C version
// tracks n and size separately over a Rune*; the Go version uses a slice whose
// length is n. Trailing-NUL conventions from the C code are preserved: some
// operations deliberately keep a 0 rune at the end (e.g. Strinit0), and code
// that inspects s.s[i] or s.s[0]==0 relies on it.
type String struct {
	s []Rune
}

func (p *String) n() int { return len(p.s) }

// empty is the C `Rune empty[] = {0}`.
var empty = []Rune{0}

func Strinit(p *String)  { p.s = []Rune{} }
func Strinit0(p *String) { p.s = []Rune{0} }
func Strclose(p *String) { p.s = nil }

func Strzero(p *String) { p.s = p.s[:0] }

// Strlen returns the number of runes before the first 0 (C Strlen of Rune*).
func Strlen(r []Rune) int {
	for i, c := range r {
		if c == 0 {
			return i
		}
	}
	// untested
	return len(r)
}

// Strdupl copies s including its terminating NUL (C Strdupl, "copies the null").
func Strdupl(p *String, s []Rune) {
	n := Strlen(s) + 1
	p.s = append(p.s[:0], s[:n]...)
}

// Strduplstr copies q, including a trailing NUL if q has one (C Strduplstr,
// "will copy the null if there's one there").
func Strduplstr(p, q *String) {
	p.s = append(p.s[:0], q.s...)
}

func Strinsure(n int) {
	if n > STRSIZE {
		fail(Etoolong)
	}
}

func Straddc(p *String, c int) {
	Strinsure(p.n() + 1)
	p.s = append(p.s, Rune(c))
}

func Strinsert(p, q *String, p0 Posn) {
	Strinsure(p.n() + q.n())
	tail := append([]Rune(nil), p.s[p0:]...)
	p.s = append(p.s[:p0], q.s...)
	p.s = append(p.s, tail...)
}

func Strdelete(p *String, p1, p2 Posn) {
	p.s = append(p.s[:p1], p.s[p2:]...)
}

// Strcmp mirrors the C comparison, including the special handling of a
// trailing NUL distinguishing otherwise-equal strings.
func Strcmp(a, b *String) int {
	na, nb := a.n(), b.n()
	for i := 0; i < na && i < nb; i++ {
		if c := int(a.s[i]) - int(b.s[i]); c != 0 {
			return c
		}
	}
	// damn NULs confuse everything
	i := na - nb
	if i == 1 {
		// untested
		if a.s[na-1] == 0 {
			// untested
			return 0
		}
	} else if i == -1 {
		// untested
		if b.s[nb-1] == 0 {
			// untested
			return 0
		}
	}
	return i
}

// Strispre reports whether a is a prefix of b (C Strispre).
func Strispre(a, b *String) bool {
	na, nb := a.n(), b.n()
	i := 0
	for ; i < na && i < nb; i++ {
		if a.s[i] != b.s[i] {
			return a.s[i] == 0
		}
	}
	return i == na
}

// Strtoc returns the UTF-8 encoding of s up to (not including) the first NUL
// rune. This matches every C use, where the result is consumed with strlen()
// or printed with %s.
func Strtoc(s *String) string {
	for i, c := range s.s {
		if c == 0 {
			return string(s.s[:i])
		}
	}
	return string(s.s)
}

// tmprstr builds a very temporary String aliasing the first n runes of r
// (C tmprstr).
func tmprstr(r []Rune, n int) *String {
	return &String{s: r[:n]}
}

// tmpcstr converts a Go string into a String (no trailing NUL; C tmpcstr,
// whose utflen deliberately does not include the NUL).
func tmpcstr(s string) *String {
	return &String{s: []rune(s)}
}

func freetmpstr(*String) {}
