package main

import "fmt"

const INFINITY = 0x7FFFFFFF

type Posn int // file position or address

// Address is a range within a particular file.
type Address struct {
	r Range
	f *File
}

// Range is a span of text (sam.h struct Range).
type Range struct {
	p1, p2 Posn
}

func (s *Sam) address(ap *Addr, a Address, sign int) Address {
	f := a.f
	var a1, a2 Address

	for ; ap != nil; ap = ap.next {
		switch ap.typ {
		case 'l', '#':
			if ap.typ == '#' {
				a = s.charaddr(ap.num, a, sign)
			} else {
				a = s.lineaddr(ap.num, a, sign)
			}

		case '.':
			a = f.dot

		case '$':
			a.r.p1 = f.b.nc()
			a.r.p2 = f.b.nc()

		case '\'':
			a.r = f.mark

		case '?':
			sign = -sign
			if sign == 0 {
				sign = -1
			}
			fallthrough
		case '/':
			var from Posn
			if sign >= 0 {
				from = a.r.p2
			} else {
				from = a.r.p1
			}
			s.nextmatch(f, ap.are, from, sign)
			a.r = s.sel.p[0]

		case '"':
			a = s.matchfile(ap.are).dot
			f = a.f
			if f.unread {
				s.load(f)
			}

		case '*':
			a.r.p1 = 0
			a.r.p2 = f.b.nc()
			return a

		case ',', ';':
			if ap.left != nil {
				a1 = s.address(ap.left, a, 0)
			} else {
				a1.f = a.f
				a1.r.p1 = 0
				a1.r.p2 = 0
			}
			if ap.typ == ';' {
				f = a1.f
				a = a1
				f.dot = a1
			}
			if ap.next != nil {
				a2 = s.address(ap.next, a, 0)
			} else {
				a2.f = a.f
				a2.r.p1 = f.b.nc()
				a2.r.p2 = f.b.nc()
			}
			if a1.f != a2.f {
				// untested
				s.error(Eorder)
			}
			a.f = a1.f
			a.r.p1 = a1.r.p1
			a.r.p2 = a2.r.p2
			if a.r.p2 < a.r.p1 {
				s.error(Eorder)
			}
			return a

		case '+', '-':
			sign = 1
			if ap.typ == '-' {
				sign = -1
			}
			if ap.next == nil || ap.next.typ == '+' || ap.next.typ == '-' {
				a = s.lineaddr(1, a, sign)
			}

		default:
			// unreachable
			s.panic("address")
			return a
		}
	}
	return a
}

func (s *Sam) nextmatch(f *File, r []rune, p Posn, sign int) {
	s.compile(r)
	if sign >= 0 {
		if !s.execute(f, p, INFINITY) {
			s.error(Esearch)
		}
		if s.sel.p[0].p1 == s.sel.p[0].p2 && s.sel.p[0].p1 == p {
			if p++; p > f.b.nc() {
				// untested: zero-width search that matches at the search position then wraps
				p = 0
			}
			if !s.execute(f, p, INFINITY) {
				// unreachable
				s.panic("address")
			}
		}
	} else {
		if !s.bexecute(f, p) {
			s.error(Esearch)
		}
		if s.sel.p[0].p1 == s.sel.p[0].p2 && s.sel.p[0].p2 == p {
			if p--; p < 0 {
				// untested: zero-width search that matches at the search position then wraps
				p = f.b.nc()
			}
			if !s.bexecute(f, p) {
				// unreachable
				s.panic("address")
			}
		}
	}
}

func (s *Sam) matchfile(r []rune) *File {
	var match *File
	for _, f := range s.file {
		if f == s.cmd {
			continue
		}
		if s.filematch(f, r) {
			if match != nil {
				s.error(Emanyfiles)
			}
			match = f
		}
	}
	if match == nil {
		s.error(Efsearch)
	}
	return match
}

func (s *Sam) filematch(f *File, r []rune) bool {
	c := string(f.name)
	mc := byte(' ')
	if f.mod {
		mc = '\''
	}
	rc := byte('-')
	if f.rasp.init {
		rc = '+'
	}
	cc := byte(' ')
	if f == s.curfile {
		cc = '.'
	}
	buf := fmt.Sprintf("%c%c%c %s\n", mc, rc, cc, c)
	s.genstr = []rune(buf)
	// A little dirty...
	if s.menu == nil {
		s.menu = s.fileopen()
	}
	bufreset(&s.menu.b)
	s.bufinsert(&s.menu.b, 0, s.genstr)
	s.compile(r)
	return s.execute(s.menu, 0, s.menu.b.nc())
}

func (s *Sam) charaddr(l Posn, addr Address, sign int) Address {
	if sign == 0 {
		addr.r.p1 = l
		addr.r.p2 = l
	} else if sign < 0 {
		addr.r.p1 -= l
		addr.r.p2 = addr.r.p1
	} else {
		addr.r.p2 += l
		addr.r.p1 = addr.r.p2
	}
	if addr.r.p1 < 0 || addr.r.p2 > addr.f.b.nc() {
		s.error(Erange)
	}
	return addr
}

func (s *Sam) lineaddr(l Posn, addr Address, sign int) Address {
	var n Posn
	var c rune
	f := addr.f
	var a Address
	var p Posn

	a.f = f
	if sign >= 0 {
		if l == 0 {
			if sign == 0 || addr.r.p2 == 0 {
				a.r.p1 = 0
				a.r.p2 = 0
				return a
			}
			a.r.p1 = addr.r.p2
			p = addr.r.p2 - 1
		} else {
			if sign == 0 || addr.r.p2 == 0 {
				p = 0
				n = 1
			} else {
				p = addr.r.p2 - 1
				if filereadc(f, p) == '\n' {
					n = 1
				} else {
					n = 0
				}
				p++
			}
			for n < l {
				if p >= f.b.nc() {
					s.error(Erange)
				}
				if filereadc(f, p) == '\n' {
					n++
				}
				p++
			}
			a.r.p1 = p
		}
		for p < f.b.nc() {
			ch := filereadc(f, p)
			p++
			if ch == '\n' {
				break
			}
		}
		a.r.p2 = p
	} else {
		p = addr.r.p1
		if l == 0 {
			a.r.p2 = addr.r.p1
		} else {
			for n = 0; n < l; {
				if p == 0 {
					if n++; n != l {
						s.error(Erange)
					}
				} else {
					c = filereadc(f, p-1)
					if c != '\n' {
						p--
					} else if n++; n != l {
						p--
					}
				}
			}
			a.r.p2 = p
			if p > 0 {
				p--
			}
		}
		for p > 0 && filereadc(f, p-1) != '\n' { // lines start after a newline
			p--
		}
		a.r.p1 = p
	}
	return a
}
