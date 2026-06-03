package main

import (
	"fmt"
	"io"
)

// Command execution (xec.c).

func (s *Sam) resetxec() {
	s.glooping = 0
	s.nest = 0
}

func (s *Sam) cmdexec(f *File, cp *Cmd) bool {
	if f != nil && f.unread {
		s.load(f)
	}
	if f == nil && (cp.addr == nil || cp.addr.typ != '"') &&
		!runeIn("bBnqUXY!", cp.cmdc) &&
		cp.cmdc != ('c'|0x100) && !(cp.cmdc == 'D' && cp.ctext != nil) {
		fail(Enofile)
	}
	i := lookup(cp.cmdc)
	if i >= 0 && cmdtab[i].defaddr != aNo {
		ap := cp.addr
		if ap == nil && cp.cmdc != '\n' {
			cp.addr = newaddr()
			ap = cp.addr
			ap.typ = '.'
			if cmdtab[i].defaddr == aAll {
				ap.typ = '*'
			}
		} else if ap != nil && ap.typ == '"' && ap.next == nil && cp.cmdc != '\n' {
			ap.next = newaddr()
			ap.next.typ = '.'
			if cmdtab[i].defaddr == aAll {
				ap.next.typ = '*'
			}
		}
		if cp.addr != nil { // may be false for '\n' (only)
			var none Address
			if f != nil {
				s.addr = s.address(ap, f.dot, 0)
			} else { // a "
				s.addr = s.address(ap, none, 0)
			}
			f = s.addr.f
		}
	}
	s.current(f)
	switch cp.cmdc {
	case '{':
		var a Address
		if cp.addr != nil {
			a = s.address(cp.addr, f.dot, 0)
		} else {
			a = f.dot
		}
		for cp = cp.ccmd; cp != nil; cp = cp.next {
			a.f.dot = a
			s.cmdexec(a.f, cp)
		}
	default:
		return cmdtab[i].fn(s, f, cp)
	}
	return true
}

func (s *Sam) a_cmd(f *File, cp *Cmd) bool {
	return s.append(f, cp, s.addr.r.p2)
}

func (s *Sam) b_cmd(f *File, cp *Cmd) bool {
	if cp.cmdc == 'b' {
		f = s.tofile(cp.ctext)
	} else {
		f = s.getfile(cp.ctext)
	}
	if f.unread {
		s.load(f)
	} else if s.nest == 0 {
		s.filename(f)
	}
	return true
}

func (s *Sam) c_cmd(f *File, cp *Cmd) bool {
	s.logdelete(f, s.addr.r.p1, s.addr.r.p2)
	f.ndot.r.p1 = s.addr.r.p2
	f.ndot.r.p2 = s.addr.r.p2
	return s.append(f, cp, s.addr.r.p2)
}

func (s *Sam) d_cmd(f *File, cp *Cmd) bool {
	s.logdelete(f, s.addr.r.p1, s.addr.r.p2)
	f.ndot.r.p1 = s.addr.r.p1
	f.ndot.r.p2 = s.addr.r.p1
	return true
}

func (s *Sam) D_cmd(f *File, cp *Cmd) bool {
	s.closefiles(f, cp.ctext)
	return true
}

func (s *Sam) e_cmd(f *File, cp *Cmd) bool {
	if s.getname(f, cp.ctext, cp.cmdc == 'e') == 0 {
		fail(Enoname)
	}
	s.edit(f, cp.cmdc)
	return true
}

func (s *Sam) f_cmd(f *File, cp *Cmd) bool {
	s.getname(f, cp.ctext, true)
	s.filename(f)
	return true
}

func (s *Sam) g_cmd(f *File, cp *Cmd) bool {
	if f != s.addr.f {
		// unreachable
		s.panicf("g_cmd f!=addr.f")
	}
	s.compile(cp.re)
	if s.execute(f, s.addr.r.p1, s.addr.r.p2) != (cp.cmdc == 'v') {
		f.dot = s.addr
		return s.cmdexec(f, cp.ccmd)
	}
	return true
}

func (s *Sam) i_cmd(f *File, cp *Cmd) bool {
	return s.append(f, cp, s.addr.r.p1)
}

func (s *Sam) k_cmd(f *File, cp *Cmd) bool {
	f.mark = s.addr.r
	return true
}

func (s *Sam) m_cmd(f *File, cp *Cmd) bool {
	addr2 := s.address(cp.caddr, f.dot, 0)
	if cp.cmdc == 'm' {
		s.move(f, addr2)
	} else {
		s.copy(f, addr2)
	}
	return true
}

func (s *Sam) n_cmd(f *File, cp *Cmd) bool {
	for _, g := range s.file {
		if g == s.cmd {
			continue
		}
		Strduplstr(&s.genstr, &g.name)
		s.filename(g)
	}
	return true
}

func (s *Sam) p_cmd(f *File, cp *Cmd) bool {
	return s.display(f)
}

func (s *Sam) q_cmd(f *File, cp *Cmd) bool {
	s.trytoquit()
	if s.downloaded {
		s.outT0(Hexit)
		return true
	}
	return false
}

func (s *Sam) s_cmd(f *File, cp *Cmd) bool {
	n := cp.num
	op := Posn(-1)
	didsub := false
	var delta Posn
	s.compile(cp.re)
	for p1 := s.addr.r.p1; p1 <= s.addr.r.p2 && s.execute(f, p1, s.addr.r.p2); {
		if s.sel.p[0].p1 == s.sel.p[0].p2 { // empty match?
			if s.sel.p[0].p1 == op {
				p1++
				continue
			}
			p1 = s.sel.p[0].p2 + 1
		} else {
			p1 = s.sel.p[0].p2
		}
		op = s.sel.p[0].p2
		n--
		if n > 0 {
			continue
		}
		Strzero(&s.genstr)
		for i := 0; i < cp.ctext.n(); i++ {
			c := int(cp.ctext.s[i])
			if c == '\\' && i < cp.ctext.n()-1 {
				i++
				c = int(cp.ctext.s[i])
				if '1' <= c && c <= '9' {
					j := c - '0'
					if s.sel.p[j].p2-s.sel.p[j].p1 > BLOCKSIZE {
						fail(Elongtag)
					}
					nn := int(s.sel.p[j].p2 - s.sel.p[j].p1)
					bufread(&f.b, s.sel.p[j].p1, s.genbuf[:], nn)
					Strinsert(&s.genstr, tmprstr(s.genbuf[:], nn), Posn(s.genstr.n()))
				} else {
					Straddc(&s.genstr, c)
				}
			} else if c != '&' {
				Straddc(&s.genstr, c)
			} else {
				if s.sel.p[0].p2-s.sel.p[0].p1 > BLOCKSIZE {
					fail(Elongrhs)
				}
				nn := int(s.sel.p[0].p2 - s.sel.p[0].p1)
				bufread(&f.b, s.sel.p[0].p1, s.genbuf[:], nn)
				Strinsert(&s.genstr, tmprstr(s.genbuf[:], nn), Posn(s.genstr.n()))
			}
		}
		if s.sel.p[0].p1 != s.sel.p[0].p2 {
			s.logdelete(f, s.sel.p[0].p1, s.sel.p[0].p2)
			delta -= s.sel.p[0].p2 - s.sel.p[0].p1
		}
		if s.genstr.n() != 0 {
			s.loginsert(f, s.sel.p[0].p2, s.genstr.s, s.genstr.n())
			delta += Posn(s.genstr.n())
		}
		didsub = true
		if cp.flag == 0 {
			break
		}
	}
	if !didsub && s.nest == 0 {
		fail(Enosub)
	}
	f.ndot.r.p1 = s.addr.r.p1
	f.ndot.r.p2 = s.addr.r.p2 + delta
	return true
}

func (s *Sam) u_cmd(f *File, cp *Cmd) bool {
	n := cp.num
	if n >= 0 {
		for {
			old := n
			n--
			if old == 0 || !s.undo(true) {
				break
			}
		}
	} else {
		for {
			old := n
			n++
			if old == 0 || !s.undo(false) {
				break
			}
		}
	}
	return true
}

func (s *Sam) w_cmd(f *File, cp *Cmd) bool {
	fseq := f.seq
	if s.getname(f, cp.ctext, false) == 0 {
		fail(Enoname)
	}
	if fseq == s.seq {
		fail_s(Ewseq, s.genc)
	}
	s.writef(f)
	return true
}

func (s *Sam) x_cmd(f *File, cp *Cmd) bool {
	if cp.re != nil {
		s.looper(f, cp, cp.cmdc == 'x')
	} else {
		s.linelooper(f, cp)
	}
	return true
}

func (s *Sam) X_cmd(f *File, cp *Cmd) bool {
	s.filelooper(cp, cp.cmdc == 'X')
	return true
}

func (s *Sam) plan9_cmd(f *File, cp *Cmd) bool {
	s.plan9(f, cp.cmdc, cp.ctext, s.nest)
	return true
}

func (s *Sam) eq_cmd(f *File, cp *Cmd) bool {
	charsonly := false
	switch cp.ctext.n() {
	case 1:
		charsonly = false
	case 2:
		if cp.ctext.s[0] == '#' {
			charsonly = true
			break
		}
		fail(Enewline)
	default:
		fail(Enewline)
	}
	s.printposn(f, charsonly)
	return true
}

func (s *Sam) nl_cmd(f *File, cp *Cmd) bool {
	if cp.addr == nil {
		// First put it on newline boundaries
		s.addr = s.lineaddr(0, f.dot, -1)
		a := s.lineaddr(0, f.dot, 1)
		s.addr.r.p2 = a.r.p2
		if s.addr.r.p1 == f.dot.r.p1 && s.addr.r.p2 == f.dot.r.p2 {
			s.addr = s.lineaddr(1, f.dot, 1)
		}
		s.display(f)
	} else if s.downloaded {
		s.moveto(f, s.addr.r)
	} else {
		s.display(f)
	}
	return true
}

func (s *Sam) cd_cmd(f *File, cp *Cmd) bool {
	s.cd(cp.ctext)
	return true
}

func (s *Sam) append(f *File, cp *Cmd, p Posn) bool {
	if cp.ctext.n() > 0 && cp.ctext.s[cp.ctext.n()-1] == 0 {
		cp.ctext.s = cp.ctext.s[:cp.ctext.n()-1]
	}
	if cp.ctext.n() > 0 {
		s.loginsert(f, p, cp.ctext.s, cp.ctext.n())
	}
	f.ndot.r.p1 = p
	f.ndot.r.p2 = p + Posn(cp.ctext.n())
	return true
}

func (s *Sam) display(f *File) bool {
	p1 := s.addr.r.p1
	p2 := s.addr.r.p2
	if p2 > Posn(f.b.nc()) {
		// unreachable
		fmt.Fprintf(s.err, "bad display addr p1=%d p2=%d f->b.nc=%d\n", p1, p2, f.b.nc())
		p2 = Posn(f.b.nc())
	}
	for p1 < p2 {
		np := int(p2 - p1)
		if np > BLOCKSIZE-1 {
			np = BLOCKSIZE - 1
		}
		bufread(&f.b, p1, s.genbuf[:], np)
		s.genbuf[np] = 0
		c := Strtoc(tmprstr(s.genbuf[:], np+1))
		if s.downloaded {
			s.termwrite(c)
		} else {
			io.WriteString(s.out, c)
		}
		p1 += Posn(np)
	}
	f.dot = s.addr
	return true
}

func (s *Sam) looper(f *File, cp *Cmd, xy bool) {
	r := s.addr.r
	var op Posn
	if xy {
		op = -1
	} else {
		op = r.p1
	}
	s.nest++
	s.compile(cp.re)
	for p := r.p1; p <= r.p2; {
		if !s.execute(f, p, r.p2) { // no match, but y should still run
			if xy || op > r.p2 {
				break
			}
			f.dot.r.p1 = op
			f.dot.r.p2 = r.p2
			p = r.p2 + 1 // exit next loop
		} else {
			if s.sel.p[0].p1 == s.sel.p[0].p2 { // empty match?
				if s.sel.p[0].p1 == op {
					p++
					continue
				}
				p = s.sel.p[0].p2 + 1
			} else {
				p = s.sel.p[0].p2
			}
			if xy {
				f.dot.r = s.sel.p[0]
			} else {
				f.dot.r.p1 = op
				f.dot.r.p2 = s.sel.p[0].p1
			}
		}
		op = s.sel.p[0].p2
		s.cmdexec(f, cp.ccmd)
		s.compile(cp.re)
	}
	s.nest--
}

func (s *Sam) linelooper(f *File, cp *Cmd) {
	s.nest++
	r := s.addr.r
	var a, a3 Address
	a3.f = f
	a3.r.p1 = r.p1
	a3.r.p2 = r.p1
	var linesel Range
	for p := r.p1; p < r.p2; p = a3.r.p2 {
		a3.r.p1 = a3.r.p2
		if p != r.p1 {
			a = s.lineaddr(1, a3, 1)
			linesel = a.r
		} else {
			a = s.lineaddr(0, a3, 1)
			linesel = a.r
			if linesel.p2 == p {
				a = s.lineaddr(1, a3, 1)
				linesel = a.r
			}
		}
		if linesel.p1 >= r.p2 {
			// untested: linelooper boundary not reached by the differential cases
			break
		}
		if linesel.p2 >= r.p2 {
			linesel.p2 = r.p2
		}
		if linesel.p2 > linesel.p1 {
			if linesel.p1 >= a3.r.p2 && linesel.p2 > a3.r.p2 {
				f.dot.r = linesel
				s.cmdexec(f, cp.ccmd)
				a3.r = linesel
				continue
			}
		}
		// untested: linelooper boundary not reached by the differential cases
		break
	}
	s.nest--
}

func (s *Sam) filelooper(cp *Cmd, XY bool) {
	old := s.glooping
	s.glooping++
	if old != 0 {
		fail(EnestXY)
	}
	s.nest++
	s.settempfile()
	cur := s.curfile
	for i := 0; i < len(s.tempfile); i++ {
		f := s.tempfile[i]
		if f == s.cmd {
			continue
		}
		if cp.re == nil || s.filematch(f, cp.re) == XY {
			s.cmdexec(f, cp.ccmd)
		}
	}
	if cur != nil && s.whichmenu(cur) >= 0 { // check that cur is still a file
		s.current(cur)
	}
	s.glooping--
	s.nest--
}
