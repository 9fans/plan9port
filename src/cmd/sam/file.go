package main

import (
	"io"
	"slices"
)

// File is a file and its undo state.
type File struct {
	b       Buffer // the data
	delta   []Undo // transcript of changes (undo)
	epsilon []Undo // inverse of delta (redo)
	name    String // name of associated file
	qidpath uint64
	mtime   uint
	dev     uint32
	unread  bool // file has not been read from disk

	seq      int  // if seq==0, File acts like Buffer
	cleanseq int  // f.seq at last read/write of file
	mod      bool // file appears modified in menu
	rescuing bool // sam exiting; this file unusable

	hiposn Posn    // highest address touched this Mod
	dot    Address // current position
	ndot   Address // new current position after update
	tdot   Range   // what terminal thinks is current range
	mark   Range   // tagged spot in text

	rasp    []Posn // map of what terminal's got (nil if no terminal)
	hasrasp bool   // rasp list has been allocated (distinguishes nil from empty)
	tag     int    // for communicating with terminal
	closeok bool   // ok to close file?
	deleted bool   // delete at completion of command

	prevdot  Range // state before start of change
	prevmark Range
	prevseq  int
	prevmod  bool
}

// Undo transcript.
// delta/epsilon are stacks of Undo.
// During a command, loginsert/logdelete batch changes through s.merge,
// and flushmerge appends them to f.epsilon.
// Fileupdate then replays epsilon onto f.b via fileundo(isundo=false),
// recording inverses into f.delta.

// Undo is one entry in a File's undo/redo transcript. The C code packs
// an Undo struct into a rune Buffer that is read backwards; we use a typed
// slice consumed LIFO instead. Behavior (sequence grouping, replay chunking)
// is preserved.
type Undo struct {
	typ int  // Delete, Insert, Filename, Dot, Mark
	mod bool // modify bit at time of change
	seq int  // sequence number
	p0  Posn
	n   Posn   // # runes in buf, or count for Delete/Dot/Mark
	buf []Rune // payload for Insert / Filename
}

// Merge batches adjacent log changes.
type Merge struct {
	f    *File
	seq  int
	p0   Posn
	n    Posn
	nbuf Posn
	buf  [RBUFSIZE]Rune
}

const (
	Maxmerge = 50
)

func (s *Sam) fileopen() *File {
	f := &File{}
	f.dot.f = f
	f.ndot.f = f
	f.seq = 0
	f.mod = false
	f.unread = true
	Strinit0(&f.name)
	return f
}

func fileisdirty(f *File) bool {
	return f.seq != f.cleanseq
}

func wrinsert(delta []Undo, seq int, mod bool, p0 Posn, s []Rune, ns int) []Undo {
	return append(delta, Undo{
		typ: Insert,
		mod: mod,
		seq: seq,
		p0:  p0,
		n:   ns,
		buf: slices.Clone(s[:ns]),
	})
}

func wrdelete(delta []Undo, seq int, mod bool, p0, p1 Posn) []Undo {
	return append(delta, Undo{
		typ: Delete,
		mod: mod,
		seq: seq,
		p0:  p0,
		n:   p1 - p0,
	})
}

func (s *Sam) flushmerge() {
	m := &s.merge
	f := m.f
	if f == nil {
		return
	}
	if m.seq != f.seq {
		// unreachable
		s.panicf("flushmerge seq mismatch")
	}
	if m.n != 0 {
		f.epsilon = wrdelete(f.epsilon, f.seq, true, m.p0, m.p0+m.n)
	}
	if m.nbuf != 0 {
		f.epsilon = wrinsert(f.epsilon, f.seq, true, m.p0+m.n, m.buf[:m.nbuf], m.nbuf)
	}
	m.f = nil
	m.n = 0
	m.nbuf = 0
}

func (s *Sam) mergeextend(f *File, p0 Posn) {
	m := &s.merge
	mp0n := m.p0 + m.n
	if mp0n != p0 {
		bufread(&f.b, mp0n, m.buf[m.nbuf:], p0-mp0n)
		m.nbuf += p0 - mp0n
		m.n = p0 - m.p0
	}
}

// loginsert is like fileundelete, but it gets the data from arguments.
func (s *Sam) loginsert(f *File, p0 Posn, str []Rune, ns int) {
	if f.rescuing {
		// unreachable: rescue() is always followed by exits(), so nothing is logged while rescuing
		return
	}
	if ns == 0 {
		return
	}
	if ns > STRSIZE {
		// unreachable
		s.panicf("loginsert")
	}
	if f.seq < s.seq {
		s.filemark(f)
	}
	if p0 < f.hiposn {
		fail(Esequence)
	}

	merge := &s.merge
	if merge.f != f ||
		p0-(merge.p0+merge.n) > Maxmerge || // too far
		merge.nbuf+((p0+ns)-(merge.p0+merge.n)) >= RBUFSIZE { // too long
		s.flushmerge()
	}

	if ns >= RBUFSIZE {
		if !(merge.n == 0 && merge.nbuf == 0 && merge.f == nil) {
			// unreachable
			s.panicf("loginsert bad merge state")
		}
		f.epsilon = wrinsert(f.epsilon, f.seq, true, p0, str, ns)
	} else {
		if merge.f != f {
			merge.f = f
			merge.p0 = p0
			merge.seq = f.seq
		}
		s.mergeextend(f, p0)

		// append string to merge
		copy(merge.buf[merge.nbuf:], str[:ns])
		merge.nbuf += ns
	}

	f.hiposn = p0
	if !f.unread && !f.mod {
		s.state(f, Dirty)
	}
}

func (s *Sam) logdelete(f *File, p0, p1 Posn) {
	if f.rescuing {
		// unreachable: rescue() is always followed by exits(), so nothing is logged while rescuing
		return
	}
	if p0 == p1 {
		return
	}
	if f.seq < s.seq {
		s.filemark(f)
	}
	if p0 < f.hiposn {
		fail(Esequence)
	}

	merge := &s.merge
	if merge.f != f ||
		p0-(merge.p0+merge.n) > Maxmerge || // too far
		merge.nbuf+(p0-(merge.p0+merge.n)) >= RBUFSIZE { // too long
		s.flushmerge()
		merge.f = f
		merge.p0 = p0
		merge.seq = f.seq
	}

	s.mergeextend(f, p0)

	// add to deletion
	merge.n = p1 - merge.p0

	f.hiposn = p1
	if !f.unread && !f.mod {
		s.state(f, Dirty)
	}
}

// logsetname is like fileunsetname, but gets the data from arguments.
func (s *Sam) logsetname(f *File, str *String) {
	if f.rescuing {
		// unreachable: rescue() is always followed by exits(), so nothing is logged while rescuing
		return
	}

	if f.unread { // setting initial file name
		s.filesetname(f, str)
		return
	}

	if f.seq < s.seq {
		s.filemark(f)
	}

	// The new name goes into epsilon (the pending-change log); fileupdate's
	// fileundo(redo) applies it and records the old name into delta.
	f.epsilon = append(f.epsilon, Undo{
		typ: Filename,
		mod: true,
		seq: f.seq,
		p0:  0,
		n:   str.n(),
		buf: append([]Rune(nil), str.s...),
	})
	if !f.unread && !f.mod {
		s.state(f, Dirty)
	}
}

func (s *Sam) fileuninsert(f *File, delta *[]Undo, p0 Posn, ns int) {
	// undo an insertion by deleting
	*delta = append(*delta, Undo{
		typ: Delete,
		mod: f.mod,
		seq: f.seq,
		p0:  p0,
		n:   ns,
	})
}

func (s *Sam) fileundelete(f *File, delta *[]Undo, p0, p1 Posn) {
	// undo a deletion by inserting
	buf := make([]Rune, p1-p0)
	bufread(&f.b, p0, buf, p1-p0)
	*delta = append(*delta, Undo{
		typ: Insert,
		mod: f.mod,
		seq: f.seq,
		p0:  p0,
		n:   p1 - p0,
		buf: buf,
	})
}

func filereadc(f *File, q Posn) int {
	if q < 0 || q >= f.b.nc() {
		return -1
	}
	return int(f.b.c[q])
}

func (s *Sam) filesetname(f *File, str *String) {
	if !f.unread {
		// unreachable: the only caller (logsetname) calls filesetname only when f.unread
		s.fileunsetname(f, &f.delta)
	}
	Strduplstr(&f.name, str)
	s.sortname(f)
	f.unread = true
}

func (s *Sam) fileunsetname(f *File, delta *[]Undo) {
	// undo a file name change by restoring the old name
	var ss String
	Strinit(&ss)
	Strduplstr(&ss, &f.name)
	s.fullname(&ss)
	*delta = append(*delta, Undo{
		typ: Filename,
		mod: f.mod,
		seq: f.seq,
		p0:  0,
		n:   ss.n(),
		buf: slices.Clone(ss.s),
	})
}

func (s *Sam) fileunsetdot(f *File, delta *[]Undo, dot Range) {
	*delta = append(*delta, Undo{
		typ: Dot,
		mod: f.mod,
		seq: f.seq,
		p0:  dot.p1,
		n:   dot.p2 - dot.p1,
	})
}

func (s *Sam) fileunsetmark(f *File, delta *[]Undo, mark Range) {
	*delta = append(*delta, Undo{
		typ: Mark,
		mod: f.mod,
		seq: f.seq,
		p0:  mark.p1,
		n:   mark.p2 - mark.p1,
	})
}

func (s *Sam) fileload(f *File, p0 Posn, r io.Reader, nulls *bool) int {
	if f.seq > 0 {
		// unreachable
		s.panicf("undo in file.load unimplemented")
	}
	return bufload(&f.b, p0, r, nulls)
}

func (s *Sam) fileupdate(f *File, notrans, toterm bool) bool {
	if f.rescuing {
		// unreachable: rescue() is always followed by exits(), so no update runs while rescuing
		return false
	}

	s.flushmerge()

	// Fix the modification bit. Subtle point: don't save it away in the log.
	//
	// If another change is made, the correct f.mod state is saved in the undo
	// log by filemark when setting the dot and mark.
	//
	// If the change is undone, the correct state is saved from f in the fileun... routines.
	mod := f.mod
	f.mod = f.prevmod
	if f == s.cmd {
		notrans = true
	} else {
		s.fileunsetdot(f, &f.delta, f.prevdot)
		s.fileunsetmark(f, &f.delta, f.prevmark)
	}
	f.dot = f.ndot
	s.fileundo(f, false, !notrans, toterm)
	f.mod = mod

	if len(f.delta) == 0 {
		f.seq = 0
	}

	if f == s.cmd {
		return false
	}

	if f.mod {
		f.closeok = false
		s.quitok = false
	} else {
		f.closeok = true
	}
	return true
}

func prevseq(b []Undo) int {
	if len(b) == 0 {
		return 0
	}
	return b[len(b)-1].seq
}

func (s *Sam) undoseq(f *File, isundo bool) int {
	if isundo {
		return f.seq
	}
	return prevseq(f.epsilon)
}

func (s *Sam) fileundo(f *File, isundo, canredo, flag bool) (q0p, q1p Posn) {
	var delta, epsilon *[]Undo
	var stop int

	if isundo {
		// undo; reverse delta onto epsilon, seq decreases
		delta = &f.delta
		epsilon = &f.epsilon
		stop = f.seq
	} else {
		// redo; reverse epsilon onto delta, seq increases
		delta = &f.epsilon
		epsilon = &f.delta
		stop = 0 // don't know yet
	}

	s.raspstart(f)
	for len(*delta) > 0 {
		// rasp and buffer are in sync; sync with wire if needed
		if s.needoutflush() {
			s.raspflush(f)
		}
		u := (*delta)[len(*delta)-1]
		if isundo {
			if u.seq < stop {
				f.seq = u.seq
				s.raspdone(f, flag)
				return
			}
		} else {
			if stop == 0 {
				stop = u.seq
			}
			if u.seq > stop {
				s.raspdone(f, flag)
				return
			}
		}
		switch u.typ {
		default:
			// unreachable
			s.panicf("undo unknown u.type")

		case Delete:
			f.seq = u.seq
			if canredo {
				s.fileundelete(f, epsilon, u.p0, u.p0+u.n)
			}
			f.mod = u.mod
			bufdelete(&f.b, u.p0, u.p0+u.n)
			s.raspdelete(f, u.p0, u.p0+u.n, flag)
			q0p = u.p0
			q1p = u.p0

		case Insert:
			f.seq = u.seq
			if canredo {
				s.fileuninsert(f, epsilon, u.p0, u.n)
			}
			f.mod = u.mod
			for i := 0; i < u.n; {
				n := u.n - i
				if n > RBUFSIZE {
					n = RBUFSIZE
				}
				bufinsert(&f.b, u.p0+i, u.buf[i:i+n], n)
				s.raspinsert(f, u.p0+i, u.buf[i:i+n], n, flag)
				i += n
			}
			q0p = u.p0
			q1p = u.p0 + u.n

		case Filename:
			f.seq = u.seq
			if canredo {
				s.fileunsetname(f, epsilon)
			}
			f.mod = u.mod
			f.name.s = append(f.name.s[:0], u.buf[:u.n]...)
			s.fixname(&f.name)
			s.sortname(f)

		case Dot:
			f.seq = u.seq
			if canredo {
				s.fileunsetdot(f, epsilon, f.dot.r)
			}
			f.mod = u.mod
			f.dot.r.p1 = u.p0
			f.dot.r.p2 = u.p0 + u.n

		case Mark:
			f.seq = u.seq
			if canredo {
				s.fileunsetmark(f, epsilon, f.mark)
			}
			f.mod = u.mod
			f.mark.p1 = u.p0
			f.mark.p2 = u.p0 + u.n
		}
		*delta = (*delta)[:len(*delta)-1]
	}
	if isundo {
		// unreachable: every edit leaves seq-0 baseline dot/mark records,
		// so an undo (stop>=1) always early-returns above before delta drains.
		f.seq = 0
	}
	s.raspdone(f, flag)
	return
}

func (s *Sam) fileclose(f *File) {
	Strclose(&f.name)
	bufclose(&f.b)
	f.delta = nil
	f.epsilon = nil
	f.rasp = nil
	f.hasrasp = false
}

func (s *Sam) filemark(f *File) {
	if f.unread {
		// unreachable: a file is always loaded (unread=false) before its first edit
		return
	}
	if len(f.epsilon) != 0 {
		f.epsilon = f.epsilon[:0]
	}

	if f != s.cmd {
		f.prevdot = f.dot.r
		f.prevmark = f.mark
		f.prevseq = f.seq
		f.prevmod = f.mod
	}

	f.ndot = f.dot
	f.seq = s.seq
	f.hiposn = 0
}
