package main

import "slices"

// A Rasp models a file's text as a list of present/absent sections, the
// terminal's lazy mirror of the text held by the sam command engine.
type Rasp struct {
	nrunes int
	sect   *Section
}

// A Section is one span of a Rasp, representing nrunes runes,
// possibly not yet loaded from the host (text == nil).
type Section struct {
	nrunes int
	text   []rune
	next   *Section
}

// init resets the rasp to empty.
func (r *Rasp) init() {
	r.nrunes = 0
	r.sect = nil
}

// clear empties the rasp's local sections but preserves the known text length.
// (It discards the local cache but not what it knows about the file.)
func (r *Rasp) clear() {
	r.sect = nil
}

// insert inserts and returns a new empty section before s.
func (r *Rasp) insert(s *Section) *Section {
	t := &Section{}
	if r.sect == s { // includes empty list case: r.sect==s==nil
		r.sect = t
		t.next = s
		return t
	}
	u := r.sect
	if u == nil {
		// unreachable
		panic("rsinsert 1")
	}
	for u != nil {
		if u.next == s {
			t.next = s
			u.next = t
			return t
		}
		u = u.next
	}
	// unreachable
	panic("rsinsert 2")
}

// delete unlinks the section s from the rasp.
func (r *Rasp) delete(s *Section) {
	if s == nil {
		// unreachable
		panic("rsdelete")
	}
	if r.sect == s {
		r.sect = s.next
		return
	}
	for t := r.sect; t != nil; t = t.next {
		if t.next == s {
			t.next = s.next
			return
		}
	}
	// unreachable
	panic("rsdelete 2")
}

// split splits section s into two at offset n0.
func (r *Rasp) split(s *Section, n0 int) {
	if s == nil {
		// unreachable
		panic("split")
	}
	r.insert(s.next) // new section lands between s and s.next
	if s.text != nil {
		s.next.text = slices.Clone(s.text[n0:])
		s.text = s.text[:n0:n0]
	}
	s.next.nrunes = s.nrunes - n0
	s.nrunes = n0
}

// find returns a section starting at offset q,
// splitting an existing section if necessary.
// The search begins at section s, assumed to start at offset p.
func (r *Rasp) find(s *Section, p, q int) *Section {
	if s == nil && p != q {
		// unreachable
		panic("find")
	}
	for ; s != nil && p+s.nrunes <= q; s = s.next {
		p += s.nrunes
	}
	if p != q {
		r.split(s, q-p)
		s = s.next
	}
	return s
}

// resize replaces the old runes at offset q with a hole of new runes.
// The new span is expected to be filled in by [Rasp.data].
func (r *Rasp) resize(q, old, new int) {
	s := r.find(r.sect, 0, q)
	t := r.find(s, q, q+old)
	for s != t {
		ns := s.next
		r.delete(s)
		s = ns
	}
	// now insert the new piece before t
	if new > 0 {
		ns := r.insert(t)
		ns.nrunes = new
		ns.text = nil
	}
	r.nrunes += new - old
}

// data fills [p0, p1) with the runes in cp.
// The replaced sections must all be holes.
func (r *Rasp) data(p0, p1 int, cp []rune) {
	s := r.find(r.sect, 0, p0)
	t := r.find(s, p0, p1)
	for s != t {
		ns := s.next
		if s.text != nil {
			// unreachable
			panic("rdata")
		}
		r.delete(s)
		s = ns
	}
	p1 -= p0
	s = r.insert(t)
	s.text = slices.Clone(cp[:p1])
	s.nrunes = p1
}

// clean coalesces adjacent sections of the same kind,
// keeping present sections no larger than TBLOCKSIZE.
func (r *Rasp) clean() {
	for s := r.sect; s != nil; s = s.next {
		for s.next != nil && (s.text != nil) == (s.next.text != nil) {
			if s.text != nil {
				if s.nrunes+s.next.nrunes > TBLOCKSIZE {
					break
				}
				s.text = append(s.text, s.next.text...)
			}
			s.nrunes += s.next.nrunes
			r.delete(s.next)
		}
	}
}

// load returns the present runes in [p0,p1), skipping holes.
// The result aliases the Term's scratch buffer, valid until the next load.
func (t *Term) load(r *Rasp, p0, p1 int) []rune {
	t.scratch = slices.Grow(t.scratch[:0], p1-p0+1)
	p := 0
	s := r.sect
	for ; s != nil && p+s.nrunes <= p0; s = s.next {
		p += s.nrunes
	}
	for p < p1 && s != nil {
		// Subtle and important: If we are being called from [Rasp.data],
		// it's because we have a [Rasp.resize]-generated hole here,
		// so the screen doesn't have data for that space anyway
		// (it got cut first). So pretend it isn't there.
		if s.text != nil {
			n := s.nrunes - (p0 - p)
			if n > p1-p0 { // all in this section
				n = p1 - p0
			}
			t.scratch = append(t.scratch, s.text[p0-p:p0-p+n]...)
		}
		p += s.nrunes
		p0 = p
		s = s.next
	}
	return t.scratch
}

// missing returns the number of not-yet-loaded runes in [p0, p1).
func (r *Rasp) missing(p0, p1 int) int {
	nm := 0
	p := 0
	s := r.sect
	for ; s != nil && p+s.nrunes <= p0; s = s.next {
		p += s.nrunes
	}
	for p < p1 && s != nil {
		if s.text == nil {
			n := s.nrunes - (p0 - p)
			if n > p1-p0 { // all in this section
				n = p1 - p0
			}
			nm += n
		}
		p += s.nrunes
		p0 = p
		s = s.next
	}
	return nm
}

// contig returns the number of runes from p0 that are contiguously present
// (text=true) or absent (text=false), up to p1.
func (r *Rasp) contig(p0, p1 int, text bool) int {
	np := 0
	p := 0
	s := r.sect
	for ; s != nil && p+s.nrunes <= p0; s = s.next {
		p += s.nrunes
	}
	for p < p1 && s != nil && text == (s.text != nil) {
		n := s.nrunes - (p0 - p)
		if n > p1-p0 { // all in this section
			n = p1 - p0
		}
		np += n
		p += s.nrunes
		p0 = p
		s = s.next
	}
	return np
}
