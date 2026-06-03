package main

// The rasp routines inform the terminal of changes to the file.
//
// A rasp (f.rasp) is a list of spans within the file with a high bit (M)
// marking spans the terminal already knows about.
//
// It is optimized by coalescing multiple updates to the same span
// when the terminal does not know about the span.
//
// Other possible optimizations: flush the terminal's rasp by cutting
// everything and inserting everything if the rasp gets too large.
//
// Note that raspinsert/raspdelete also shift dot and mark to track edits,
// which matters even in -d mode, when f.rasp is nil.

import "slices"

// GROWDATASIZE must be big enough that all errors go out as Hgrowdata's,
// so they will be scrolled into visibility in the ~~sam~~ window (yuck!).
const GROWDATASIZE = 50 // if size is <= this, send data with grow

// M marks, in a Rasp span, that the terminal already knows about the span.
const M = 0x80000000

// A Rasp is a list of spans covering a file: each entry is a length with the
// high bit M set if the terminal already knows that span (rasp.c's Posn list).
// init reports whether the rasp has been allocated, i.e. the file is bound to a
// terminal (C's File.rasp != nil test).
type Rasp struct {
	p    []uint32
	init bool
}

// T reports whether span i is in the terminal.
func (r *Rasp) T(i int) bool { return r.p[i]&M != 0 }

// L returns the length of span i.
func (r *Rasp) L(i int) Posn { return Posn(r.p[i] &^ M) }

// raspload is only called for the initial load of a file.
func (s *Sam) raspload(f *File) {
	if !f.rasp.init {
		return
	}
	s.grown = f.b.nc()
	s.growpos = 0
	if f.b.nc() != 0 {
		s.rgrow(&f.rasp, 0, f.b.nc())
	}
	s.raspdone(f, true)
}

func (s *Sam) raspstart(f *File) {
	if !f.rasp.init {
		return
	}
	s.grown = 0
	s.shrunk = 0
	s.outbuffered = true
}

func (s *Sam) raspdone(f *File, toterm bool) {
	if f.dot.r.p1 > f.b.nc() {
		f.dot.r.p1 = f.b.nc()
	}
	if f.dot.r.p2 > f.b.nc() {
		f.dot.r.p2 = f.b.nc()
	}
	if f.mark.p1 > f.b.nc() {
		f.mark.p1 = f.b.nc()
	}
	if f.mark.p2 > f.b.nc() {
		f.mark.p2 = f.b.nc()
	}
	if !f.rasp.init {
		return
	}
	if s.grown != 0 {
		s.outTsll(Hgrow, f.tag, s.growpos, s.grown)
	} else if s.shrunk != 0 {
		s.outTsll(Hcut, f.tag, s.shrinkpos, s.shrunk)
	}
	if toterm {
		s.outTs(Hcheck0, f.tag)
	}
	s.outflush()
	s.outbuffered = false
	if f == s.cmd {
		s.cmdpt += s.cmdptadv
		s.cmdptadv = 0
	}
}

func (s *Sam) raspflush(f *File) {
	if s.grown != 0 {
		s.outTsll(Hgrow, f.tag, s.growpos, s.grown)
		s.grown = 0
	} else if s.shrunk != 0 {
		s.outTsll(Hcut, f.tag, s.shrinkpos, s.shrunk)
		s.shrunk = 0
	}
	s.outflush()
}

func (s *Sam) raspdelete(f *File, p1, p2 Posn, toterm bool) {
	n := p2 - p1
	if n == 0 {
		// untested: fileundo only replays records with n>0 (loginsert/logdelete drop zero-length edits)
		return
	}

	if p2 <= f.dot.r.p1 {
		f.dot.r.p1 -= n
		f.dot.r.p2 -= n
	}
	if p2 <= f.mark.p1 {
		f.mark.p1 -= n
		f.mark.p2 -= n
	}

	if !f.rasp.init {
		return
	}

	if f == s.cmd && p1 < s.cmdpt {
		if p2 <= s.cmdpt {
			s.cmdpt -= n
		} else {
			s.cmdpt = p1
		}
	}
	if toterm {
		if s.grown != 0 {
			s.outTsll(Hgrow, f.tag, s.growpos, s.grown)
			s.grown = 0
		} else if s.shrunk != 0 && s.shrinkpos != p1 && s.shrinkpos != p2 {
			s.outTsll(Hcut, f.tag, s.shrinkpos, s.shrunk)
			s.shrunk = 0
		}
		if s.shrunk == 0 || s.shrinkpos == p2 {
			s.shrinkpos = p1
		}
		s.shrunk += n
	}
	s.rcut(&f.rasp, p1, p2)
}

func (s *Sam) raspinsert(f *File, p1 Posn, buf []rune, toterm bool) {
	n := Posn(len(buf))
	if n == 0 {
		// untested: fileundo only replays records with n>0 (loginsert/logdelete drop zero-length edits)
		return
	}

	if p1 < f.dot.r.p1 {
		f.dot.r.p1 += n
		f.dot.r.p2 += n
	}
	if p1 < f.mark.p1 {
		f.mark.p1 += n
		f.mark.p2 += n
	}

	if !f.rasp.init {
		return
	}
	if f == s.cmd && p1 < s.cmdpt {
		s.cmdpt += n
	}
	if toterm {
		if s.shrunk != 0 {
			s.outTsll(Hcut, f.tag, s.shrinkpos, s.shrunk)
			s.shrunk = 0
		}
		if n > GROWDATASIZE || !rterm(&f.rasp, p1) {
			s.rgrow(&f.rasp, p1, n)
			if s.grown != 0 && s.growpos+s.grown != p1 && s.growpos != p1 {
				s.outTsll(Hgrow, f.tag, s.growpos, s.grown)
				s.grown = 0
			}
			if s.grown == 0 {
				s.growpos = p1
			}
			s.grown += n
		} else {
			if s.grown != 0 {
				s.outTsll(Hgrow, f.tag, s.growpos, s.grown)
				s.grown = 0
			}
			s.rgrow(&f.rasp, p1, n)
			r := s.rdata(&f.rasp, p1, n)
			if r.p1 != p1 || r.p2 != p1+n {
				// unreachable
				s.panic("rdata in toterminal")
			}
			s.outTsllS(Hgrowdata, f.tag, p1, n, buf)
		}
	} else {
		s.rgrow(&f.rasp, p1, n)
		r := s.rdata(&f.rasp, p1, n)
		if r.p1 != p1 || r.p2 != p1+n {
			// unreachable
			s.panic("rdata in toterminal")
		}
	}
}

func (s *Sam) rcut(r *Rasp, p1, p2 Posn) {
	if p1 == p2 {
		// unreachable
		s.panic("rcut 0")
	}
	p := Posn(0)
	i := 0
	for ; i < len(r.p) && p+r.L(i) <= p1; i++ {
		p += r.L(i)
	}
	if i == len(r.p) {
		// unreachable
		s.panic("rcut 1")
	}
	if p < p1 { // chop this piece
		var x Posn
		if p+r.L(i) < p2 {
			x = p1 - p
			p += r.L(i)
		} else {
			x = r.L(i) - (p2 - p1)
			p = p2
		}
		if r.T(i) {
			r.p[i] = uint32(x) | M
		} else {
			r.p[i] = uint32(x)
		}
		i++
	}
	for i < len(r.p) && p+r.L(i) <= p2 {
		p += r.L(i)
		r.p = slices.Delete(r.p, i, i+1)
	}
	if p < p2 {
		if i == len(r.p) {
			// unreachable
			s.panic("rcut 2")
		}
		x := r.L(i) - (p2 - p)
		if r.T(i) {
			r.p[i] = uint32(x) | M
		} else {
			r.p[i] = uint32(x)
		}
	}
	// can we merge i and i-1 ?
	if i > 0 && i < len(r.p) && r.T(i-1) == r.T(i) {
		x := r.L(i-1) + r.L(i)
		r.p = slices.Delete(r.p, i, i+1)
		i--
		if r.T(i) {
			r.p[i] = uint32(x) | M
		} else {
			r.p[i] = uint32(x)
		}
	}
}

func (s *Sam) rgrow(r *Rasp, p1, n Posn) {
	if n == 0 {
		// unreachable
		s.panic("rgrow 0")
	}
	p := Posn(0)
	i := 0
	for ; i < len(r.p) && p+r.L(i) <= p1; i++ {
		p += r.L(i)
	}
	if i == len(r.p) { // stick on end of file
		if p != p1 {
			// unreachable
			s.panic("rgrow 1")
		}
		if i > 0 && !r.T(i-1) {
			r.p[i-1] += uint32(n)
		} else {
			r.p = slices.Insert(r.p, i, uint32(n))
		}
	} else if !r.T(i) { // goes in this empty piece
		r.p[i] += uint32(n)
	} else if p == p1 && i > 0 && !r.T(i-1) { // special case; simplifies life
		r.p[i-1] += uint32(n)
	} else if p == p1 {
		r.p = slices.Insert(r.p, i, uint32(n))
	} else { // must break piece in terminal
		r.p = slices.Insert(r.p, i+1, uint32(r.L(i)-(p1-p))|M)
		r.p = slices.Insert(r.p, i+1, uint32(n))
		r.p[i] = uint32(p1-p) | M
	}
}

func rterm(r *Rasp, p1 Posn) bool {
	p := Posn(0)
	i := 0
	for ; i < len(r.p) && p+r.L(i) <= p1; i++ {
		p += r.L(i)
	}
	if i == len(r.p) {
		return i > 0 && r.T(i-1)
	}
	return r.T(i)
}

func (s *Sam) rdata(r *Rasp, p1, n Posn) Range {
	if n == 0 {
		// unreachable
		s.panic("rdata 0")
	}
	p := Posn(0)
	i := 0
	for ; i < len(r.p) && p+r.L(i) <= p1; i++ {
		p += r.L(i)
	}
	if i == len(r.p) {
		// unreachable
		s.panic("rdata 1")
	}
	if r.T(i) {
		n -= r.L(i) - (p1 - p)
		if n <= 0 {
			return Range{p1, p1}
		}
		p += r.L(i)
		i++
		p1 = p
	}
	if (i < len(r.p) && r.T(i)) || i == len(r.p) {
		// unreachable
		s.panic("rdata 2")
	}
	if p+r.L(i) < p1+n {
		n = r.L(i) - (p1 - p)
	}
	rg := Range{p1, p1 + n}
	if p != p1 {
		r.p = slices.Insert(r.p, i+1, uint32(r.L(i)-(p1-p)))
		r.p[i] = uint32(p1 - p)
		i++
	}
	if r.L(i) != n {
		r.p = slices.Insert(r.p, i+1, uint32(r.L(i)-n))
		r.p[i] = uint32(n)
	}
	r.p[i] |= M
	// now i is set; can we merge?
	if i < len(r.p)-1 && r.T(i+1) {
		n += r.L(i + 1)
		r.p[i] = uint32(n) | M
		r.p = slices.Delete(r.p, i+1, i+2)
	}
	if i > 0 && r.T(i-1) {
		r.p[i] = uint32(n+r.L(i-1)) | M
		r.p = slices.Delete(r.p, i-1, i)
	}
	return rg
}
