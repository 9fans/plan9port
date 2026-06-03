package main

import "strings"

// moveto.c: dot movement and (terminal-only) double-click selection.

func (s *Sam) moveto(f *File, r Range) {
	f.dot.r.p1 = r.p1
	f.dot.r.p2 = r.p2
	if f.rasp.init {
		s.telldot(f)
		s.outTsl(Hmoveto, f.tag, f.dot.r.p1)
	}
}

func (s *Sam) telldot(f *File) {
	if !f.rasp.init {
		// unreachable
		s.panic("telldot")
	}
	if f.dot.r.p1 == f.tdot.p1 && f.dot.r.p2 == f.tdot.p2 {
		return
	}
	s.outTsll(Hsetdot, f.tag, f.dot.r.p1, f.dot.r.p2)
	f.tdot = f.dot.r
}

func (s *Sam) tellpat() {
	s.outTS(Hsetpat, s.lastpat)
	s.patset = false
}

const CHARSHIFT = 128

func (s *Sam) lookorigin(f *File, p0, ls Posn) {
	if p0 > f.b.nc() {
		p0 = f.b.nc()
	}
	oldp0 := p0
	p := p0
	nl, nc, c := 0, 0, rune(0)
	for ; c != -1 && nl < int(ls) && nc < int(ls)*CHARSHIFT; nc++ {
		p--
		if c = filereadc(f, p); c == '\n' {
			nl++
			oldp0 = p0 - Posn(nc)
		}
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

func alnum(c rune) bool {
	// Hard to get absolutely right. Use what we know about ASCII
	// and assume anything above the Latin control characters is
	// potentially an alphanumeric.
	if c <= ' ' {
		return false
	}
	if 0x7F <= c && c <= 0xA0 {
		return false
	}
	if strings.ContainsRune("!\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~", c) {
		return false
	}
	return true
}

func clickmatch(f *File, cl, cr rune, dir int, p *Posn) bool {
	nest := 1
	for {
		var c rune
		if dir > 0 {
			if *p >= f.b.nc() {
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
			if nest--; nest == 0 {
				return true
			}
		} else if c == cl {
			nest++
		}
	}
	return cl == '\n' && nest == 1
}

// strrune returns the index of c in s, or -1.
func strrune(s []rune, c rune) int {
	if c == 0 {
		// unreachable: filereadc never returns 0 (NULs are elided on load) and p1==0 yields '\n', so c is never 0
		return len(s) // points at terminating 0
	}
	for i, c1 := range s {
		if c1 == c {
			return i
		}
	}
	return -1
}

var left = [][]rune{
	{'{', '[', '(', '<', '«'},
	{'\n'},
	{'\'', '"', '`'},
}
var right = [][]rune{
	{'}', ']', ')', '>', '»'},
	{'\n'},
	{'\'', '"', '`'},
}

func (s *Sam) doubleclick(f *File, p1 Posn) {
	if p1 > f.b.nc() {
		return
	}
	f.dot.r.p1 = p1
	f.dot.r.p2 = p1
	for i := range left {
		l := left[i]
		r := right[i]
		// try left match
		p := p1
		var c rune
		if p1 == 0 {
			c = '\n'
		} else {
			c = filereadc(f, p-1)
		}
		if k := strrune(l, c); k >= 0 {
			if clickmatch(f, c, r[k], 1, &p) {
				f.dot.r.p1 = p1
				f.dot.r.p2 = p
				if c != '\n' {
					f.dot.r.p2--
				}
			}
			return
		}
		// try right match
		p = p1
		if p1 == f.b.nc() {
			c = '\n'
		} else {
			c = filereadc(f, p)
		}
		if k := strrune(r, c); k >= 0 {
			if clickmatch(f, c, l[k], -1, &p) {
				f.dot.r.p1 = p
				if c != '\n' || p != 0 || filereadc(f, 0) == '\n' {
					f.dot.r.p1++
				}
				f.dot.r.p2 = p1
				if p1 < f.b.nc() && c == '\n' {
					f.dot.r.p2++
				}
			}
			return
		}
	}
	// try filling out word to right
	p := p1
	for p < f.b.nc() && alnum(filereadc(f, p)) {
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
