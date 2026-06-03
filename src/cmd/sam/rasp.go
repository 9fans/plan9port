package main

// Faithful port of rasp.c.
//
// The rasp routines inform the terminal of changes to the file. A rasp (f.rasp)
// is a list of spans within the file with a high bit (M) marking spans the
// terminal already knows about. It is optimized by coalescing multiple updates
// to the same span when the terminal does not know it. (Other possible
// optimizations: flush the terminal's rasp by cutting everything and inserting
// everything if the rasp gets too large.)
//
// raspinsert/raspdelete also shift dot and mark to track edits, which matters
// even in -d mode (where f.rasp is nil), so that adjustment runs before the
// nil check.

// GROWDATASIZE must be big enough that all errors go out as Hgrowdata's, so
// they will be scrolled into visibility in the ~~sam~~ window (yuck!).
const GROWDATASIZE = 50 // if size is <= this, send data with grow

const M = 0x80000000

func rT(p Posn) bool { return p&M != 0 } // in terminal
func rL(p Posn) Posn { return p &^ M }   // length of this piece

// raspload is only called for the initial load of a file.
func (s *Sam) raspload(f *File) {
	if !f.hasrasp {
		return
	}
	s.grown = Posn(f.b.nc())
	s.growpos = 0
	if f.b.nc() != 0 {
		rgrow(&f.rasp, 0, Posn(f.b.nc()))
	}
	s.raspdone(f, true)
}

func (s *Sam) raspstart(f *File) {
	if !f.hasrasp {
		return
	}
	s.grown = 0
	s.shrunk = 0
	s.outbuffered = true
}

func (s *Sam) raspdone(f *File, toterm bool) {
	if f.dot.r.p1 > Posn(f.b.nc()) {
		f.dot.r.p1 = Posn(f.b.nc())
	}
	if f.dot.r.p2 > Posn(f.b.nc()) {
		f.dot.r.p2 = Posn(f.b.nc())
	}
	if f.mark.p1 > Posn(f.b.nc()) {
		f.mark.p1 = Posn(f.b.nc())
	}
	if f.mark.p2 > Posn(f.b.nc()) {
		f.mark.p2 = Posn(f.b.nc())
	}
	if !f.hasrasp {
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

	if !f.hasrasp {
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
	rcut(&f.rasp, p1, p2)
}

func (s *Sam) raspinsert(f *File, p1 Posn, buf []Rune, n int, toterm bool) {
	if n == 0 {
		// untested: fileundo only replays records with n>0 (loginsert/logdelete drop zero-length edits)
		return
	}

	if p1 < f.dot.r.p1 {
		f.dot.r.p1 += Posn(n)
		f.dot.r.p2 += Posn(n)
	}
	if p1 < f.mark.p1 {
		f.mark.p1 += Posn(n)
		f.mark.p2 += Posn(n)
	}

	if !f.hasrasp {
		return
	}
	if f == s.cmd && p1 < s.cmdpt {
		s.cmdpt += Posn(n)
	}
	if toterm {
		if s.shrunk != 0 {
			s.outTsll(Hcut, f.tag, s.shrinkpos, s.shrunk)
			s.shrunk = 0
		}
		if n > GROWDATASIZE || !rterm(&f.rasp, p1) {
			rgrow(&f.rasp, p1, Posn(n))
			if s.grown != 0 && s.growpos+s.grown != p1 && s.growpos != p1 {
				s.outTsll(Hgrow, f.tag, s.growpos, s.grown)
				s.grown = 0
			}
			if s.grown == 0 {
				s.growpos = p1
			}
			s.grown += Posn(n)
		} else {
			if s.grown != 0 {
				s.outTsll(Hgrow, f.tag, s.growpos, s.grown)
				s.grown = 0
			}
			rgrow(&f.rasp, p1, Posn(n))
			r := rdata(&f.rasp, p1, Posn(n))
			if r.p1 != p1 || r.p2 != p1+Posn(n) {
				// unreachable
				s.panicf("rdata in toterminal")
			}
			s.outTsllS(Hgrowdata, f.tag, p1, Posn(n), tmprstr(buf, n))
		}
	} else {
		rgrow(&f.rasp, p1, Posn(n))
		r := rdata(&f.rasp, p1, Posn(n))
		if r.p1 != p1 || r.p2 != p1+Posn(n) {
			// unreachable
			s.panicf("rdata in toterminal")
		}
	}
}

// rasp list primitives. inslist/dellist operate on the []Posn slice.

func raspInsAt(r *[]Posn, i int, v Posn) {
	*r = append(*r, 0)
	copy((*r)[i+1:], (*r)[i:])
	(*r)[i] = v
}

func raspDelAt(r *[]Posn, i int) {
	*r = append((*r)[:i], (*r)[i+1:]...)
}

func rcut(r *[]Posn, p1, p2 Posn) {
	if p1 == p2 {
		// unreachable
		bug("rcut 0")
	}
	var p Posn
	i := 0
	for ; i < len(*r) && p+rL((*r)[i]) <= p1; i++ {
		p += rL((*r)[i])
	}
	if i == len(*r) {
		// unreachable
		bug("rcut 1")
	}
	if p < p1 { // chop this piece
		var x Posn
		if p+rL((*r)[i]) < p2 {
			x = p1 - p
			p += rL((*r)[i])
		} else {
			x = rL((*r)[i]) - (p2 - p1)
			p = p2
		}
		if rT((*r)[i]) {
			(*r)[i] = x | M
		} else {
			(*r)[i] = x
		}
		i++
	}
	for i < len(*r) && p+rL((*r)[i]) <= p2 {
		p += rL((*r)[i])
		raspDelAt(r, i)
	}
	if p < p2 {
		if i == len(*r) {
			// unreachable
			bug("rcut 2")
		}
		x := rL((*r)[i]) - (p2 - p)
		if rT((*r)[i]) {
			(*r)[i] = x | M
		} else {
			(*r)[i] = x
		}
	}
	// can we merge i and i-1 ?
	if i > 0 && i < len(*r) && rT((*r)[i-1]) == rT((*r)[i]) {
		x := rL((*r)[i-1]) + rL((*r)[i])
		raspDelAt(r, i)
		i--
		if rT((*r)[i]) {
			(*r)[i] = x | M
		} else {
			(*r)[i] = x
		}
	}
}

func rgrow(r *[]Posn, p1, n Posn) {
	if n == 0 {
		// unreachable
		bug("rgrow 0")
	}
	var p Posn
	i := 0
	for ; i < len(*r) && p+rL((*r)[i]) <= p1; i++ {
		p += rL((*r)[i])
	}
	if i == len(*r) { // stick on end of file
		if p != p1 {
			// unreachable
			bug("rgrow 1")
		}
		if i > 0 && !rT((*r)[i-1]) {
			(*r)[i-1] += n
		} else {
			raspInsAt(r, i, n)
		}
	} else if !rT((*r)[i]) { // goes in this empty piece
		(*r)[i] += n
	} else if p == p1 && i > 0 && !rT((*r)[i-1]) { // special case; simplifies life
		(*r)[i-1] += n
	} else if p == p1 {
		raspInsAt(r, i, n)
	} else { // must break piece in terminal
		raspInsAt(r, i+1, (rL((*r)[i])-(p1-p))|M)
		raspInsAt(r, i+1, n)
		(*r)[i] = (p1 - p) | M
	}
}

func rterm(r *[]Posn, p1 Posn) bool {
	var p Posn
	i := 0
	for ; i < len(*r) && p+rL((*r)[i]) <= p1; i++ {
		p += rL((*r)[i])
	}
	if i == len(*r) {
		return i > 0 && rT((*r)[i-1])
	}
	return rT((*r)[i])
}

func rdata(r *[]Posn, p1, n Posn) Range {
	if n == 0 {
		// unreachable
		bug("rdata 0")
	}
	var p Posn
	i := 0
	for ; i < len(*r) && p+rL((*r)[i]) <= p1; i++ {
		p += rL((*r)[i])
	}
	if i == len(*r) {
		// unreachable
		bug("rdata 1")
	}
	if rT((*r)[i]) {
		n -= rL((*r)[i]) - (p1 - p)
		if n <= 0 {
			return Range{p1, p1}
		}
		p += rL((*r)[i])
		i++
		p1 = p
	}
	if (i < len(*r) && rT((*r)[i])) || i == len(*r) {
		// unreachable
		bug("rdata 2")
	}
	if p+rL((*r)[i]) < p1+n {
		n = rL((*r)[i]) - (p1 - p)
	}
	rg := Range{p1, p1 + n}
	if p != p1 {
		raspInsAt(r, i+1, rL((*r)[i])-(p1-p))
		(*r)[i] = p1 - p
		i++
	}
	if rL((*r)[i]) != n {
		raspInsAt(r, i+1, rL((*r)[i])-n)
		(*r)[i] = n
	}
	(*r)[i] |= M
	// now i is set; can we merge?
	if i < len(*r)-1 && rT((*r)[i+1]) {
		n += rL((*r)[i+1])
		(*r)[i] = n | M
		raspDelAt(r, i+1)
	}
	if i > 0 && rT((*r)[i-1]) {
		(*r)[i] = (n + rL((*r)[i-1])) | M
		raspDelAt(r, i-1)
	}
	return rg
}
