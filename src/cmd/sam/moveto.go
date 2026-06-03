package main

// moveto.c: dot movement and (terminal-only) double-click selection.

func (s *Sam) moveto(f *File, r Range) {
	f.dot.r.p1 = r.p1
	f.dot.r.p2 = r.p2
	if f.hasrasp {
		s.telldot(f)
		s.outTsl(Hmoveto, f.tag, f.dot.r.p1)
	}
}

func (s *Sam) telldot(f *File) {
	if !f.hasrasp {
		// unreachable
		s.panicf("telldot")
	}
	if f.dot.r.p1 == f.tdot.p1 && f.dot.r.p2 == f.tdot.p2 {
		return
	}
	s.outTsll(Hsetdot, f.tag, f.dot.r.p1, f.dot.r.p2)
	f.tdot = f.dot.r
}

func (s *Sam) tellpat() {
	s.outTS(Hsetpat, &s.lastpat)
	s.patset = false
}

const CHARSHIFT = 128

func (s *Sam) lookorigin(f *File, p0, ls Posn) {
	if p0 > Posn(f.b.nc()) {
		p0 = Posn(f.b.nc())
	}
	oldp0 := p0
	p := p0
	nl, nc, c := 0, 0, 0
	for c != -1 && nl < int(ls) && nc < int(ls)*CHARSHIFT {
		p--
		c = filereadc(f, p)
		if c == '\n' {
			nl++
			oldp0 = p0 - Posn(nc)
		}
		nc++
	}
	if c == -1 {
		p0 = 0
	} else if nl == 0 {
		if p0 >= CHARSHIFT/2 {
			p0 -= CHARSHIFT / 2
		} else {
			p0 = 0
		}
	} else {
		p0 = oldp0
	}
	s.outTsl(Horigin, f.tag, p0)
}

func alnum(c int) bool {
	// Hard to get absolutely right; mirror moveto.c.
	if c <= ' ' {
		return false
	}
	if 0x7F <= c && c <= 0xA0 {
		return false
	}
	if runeIn("!\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~", c) {
		return false
	}
	return true
}

func clickmatch(f *File, cl, cr, dir int, p *Posn) bool {
	nest := 1
	for {
		var c int
		if dir > 0 {
			if *p >= Posn(f.b.nc()) {
				break
			}
			c = filereadc(f, *p)
			(*p)++
		} else {
			if *p == 0 {
				break
			}
			(*p)--
			c = filereadc(f, *p)
		}
		if c == cr {
			nest--
			if nest == 0 {
				return true
			}
		} else if c == cl {
			nest++
		}
	}
	return cl == '\n' && nest == 1
}

// strrune returns the index of c in s, or -1 (moveto.c strrune returns a
// pointer; we return an index since left/right are rune slices).
func strrune(s []Rune, c int) int {
	if c == 0 {
		// unreachable: filereadc never returns 0 (NULs are elided on load) and p1==0 yields '\n', so c is never 0
		return len(s) // points at terminating 0
	}
	for i, c1 := range s {
		if int(c1) == c {
			return i
		}
	}
	return -1
}

// left/right bracket sets (unix.c).
var leftBrackets = [][]Rune{
	{'{', '[', '(', '<', 0o253},
	{'\n'},
	{'\'', '"', '`'},
}
var rightBrackets = [][]Rune{
	{'}', ']', ')', '>', 0o273},
	{'\n'},
	{'\'', '"', '`'},
}

func (s *Sam) doubleclick(f *File, p1 Posn) {
	if p1 > Posn(f.b.nc()) {
		return
	}
	f.dot.r.p1 = p1
	f.dot.r.p2 = p1
	for i := 0; i < len(leftBrackets); i++ {
		l := leftBrackets[i]
		r := rightBrackets[i]
		// try left match
		p := p1
		var c int
		if p1 == 0 {
			c = '\n'
		} else {
			c = filereadc(f, p-1)
		}
		if k := strrune(l, c); k >= 0 && k < len(l) {
			if clickmatch(f, c, int(r[k]), 1, &p) {
				f.dot.r.p1 = p1
				if c != '\n' {
					f.dot.r.p2 = p - 1
				} else {
					f.dot.r.p2 = p
				}
			}
			return
		}
		// try right match
		p = p1
		if p1 == Posn(f.b.nc()) {
			c = '\n'
		} else {
			c = filereadc(f, p)
		}
		if k := strrune(r, c); k >= 0 && k < len(r) {
			if clickmatch(f, c, int(l[k]), -1, &p) {
				f.dot.r.p1 = p
				if c != '\n' || p != 0 || filereadc(f, 0) == '\n' {
					f.dot.r.p1++
				}
				if p1 < Posn(f.b.nc()) && c == '\n' {
					f.dot.r.p2 = p1 + 1
				} else {
					f.dot.r.p2 = p1
				}
			}
			return
		}
	}
	// try filling out word to right
	p := p1
	for p < Posn(f.b.nc()) && alnum(filereadc(f, p)) {
		p++
		f.dot.r.p2++
	}
	// try filling out word to left
	p = p1
	for {
		p--
		if p < 0 || !alnum(filereadc(f, p)) {
			break
		}
		f.dot.r.p1--
	}
}
