package main

// Log of changes made by editing commands (elog.c). Three reasons for this:
// 1) addresses in commands apply to the old file, not the file-in-change;
// 2) it's hard to track changes correctly as things move (e.g. ,x m$);
// 3) it lets us optimize by merging adjacent changes.
// Like the Undo/Redo log in File, but kept separate so it can use Replace as
// well as Insert and Delete. The C version packs Buflog structs into a rune
// Buffer read backwards; we use a typed []elogRec consumed LIFO, each record
// carrying its runes directly.

// Elog is the current pending change being accumulated (dat.h Elog). nr is
// len(r).
type Elog struct {
	typ int    // Delete, Insert, Filename, Null, Empty
	q0  int    // location of change
	nd  int    // number of deleted characters
	r   []rune // inserted runes or file name
}

// elogRec is one flushed change in File.elogbuf.
type elogRec struct {
	typ int
	q0  int
	nd  int
	r   []rune
}

const (
	// Minstring shouldn't be big or we do lots of I/O for small changes;
	// Maxstring bounds how much we merge into one change.
	elogMinstring = 16
	elogMaxstring = RBUFSIZE
)

func eloginit(f *File) {
	if f.elog.typ != Empty {
		return
	}
	f.elog.typ = Null
	f.elogbuf = f.elogbuf[:0]
}

func elogclose(f *File) {
	f.elogbuf = nil
}

func elogreset(f *File) {
	f.elog.typ = Null
	f.elog.nd = 0
	f.elog.r = f.elog.r[:0]
}

func (a *Acme) elogterm(f *File) {
	elogreset(f)
	f.elogbuf = f.elogbuf[:0]
	f.elog.typ = Empty
	f.elog.r = nil
	a.warned = false
}

// elogflush pushes the current pending change onto f.elogbuf and resets it.
func elogflush(f *File) {
	switch f.elog.typ {
	default:
		// unreachable
		bug("unknown elog type")
	case Null:
		// nothing pending
	case Insert, Replace, Delete:
		f.elogbuf = append(f.elogbuf, elogRec{
			typ: f.elog.typ,
			q0:  f.elog.q0,
			nd:  f.elog.nd,
			r:   append([]rune(nil), f.elog.r...),
		})
	}
	elogreset(f)
}

func (a *Acme) elogreplace(f *File, q0, q1 int, r []rune) {
	nr := len(r)
	if q0 == q1 && nr == 0 {
		return
	}
	eloginit(f)
	if f.elog.typ != Null && q0 < f.elog.q0 {
		if !a.warned {
			a.warned = true
			a.warning(nil, "warning: changes out of sequence\n")
		}
		elogflush(f)
	}
	// try to merge with previous
	gap := q0 - (f.elog.q0 + f.elog.nd) // gap between previous and this
	// C computes gap in UNSIGNED arithmetic, so an overlapping replace (q0 inside
	// the previous delete range) underflows gap to a huge value that fails the size
	// test, forcing a flush + fresh record. Reproduce that: a negative (overlapping)
	// gap never merges.
	if f.elog.typ == Replace && gap >= 0 && len(f.elog.r)+gap+nr < elogMaxstring {
		if gap < elogMinstring {
			if gap > 0 {
				buf := make([]rune, gap)
				f.b.Read(f.elog.q0+f.elog.nd, buf)
				f.elog.r = append(f.elog.r, buf...)
			}
			f.elog.nd += gap + q1 - q0
			f.elog.r = append(f.elog.r, r...)
			return
		}
	}
	elogflush(f)
	f.elog.typ = Replace
	f.elog.q0 = q0
	f.elog.nd = q1 - q0
	if nr > RBUFSIZE {
		// A single replacement never exceeds a buffer block through the -D edit
		// surface; this mirrors C's internal-error guard.
		// untested
		a.editerror("internal error: replacement string too large(%d)", nr)
	}
	f.elog.r = append(f.elog.r[:0], r...)
}

func (a *Acme) eloginsert(f *File, q0 int, r []rune) {
	nr := len(r)
	if nr == 0 {
		// untested: appendCmd and copyText never pass an empty insertion.
		return
	}
	eloginit(f)
	if f.elog.typ != Null && q0 < f.elog.q0 {
		if !a.warned {
			a.warned = true
			a.warning(nil, "warning: changes out of sequence\n")
		}
		elogflush(f)
	}
	// try to merge with previous
	if f.elog.typ == Insert && q0 == f.elog.q0 && len(f.elog.r)+nr < elogMaxstring {
		f.elog.r = append(f.elog.r, r...)
		return
	}
	for nr > 0 {
		elogflush(f)
		f.elog.typ = Insert
		f.elog.q0 = q0
		n := min(nr, RBUFSIZE)
		f.elog.r = append(f.elog.r[:0], r[:n]...)
		r = r[n:]
		nr -= n
	}
}

func (a *Acme) elogdelete(f *File, q0, q1 int) {
	if q0 == q1 {
		// untested: every caller (d_cmd, move) guards against an empty range.
		return
	}
	eloginit(f)
	if f.elog.typ != Null && q0 < f.elog.q0+f.elog.nd {
		if !a.warned {
			a.warned = true
			a.warning(nil, "warning: changes out of sequence\n")
		}
		elogflush(f)
	}
	// try to merge with previous
	if f.elog.typ == Delete && f.elog.q0+f.elog.nd == q0 {
		f.elog.nd += q1 - q0
		return
	}
	elogflush(f)
	f.elog.typ = Delete
	f.elog.q0 = q0
	f.elog.nd = q1 - q0
}

// elogapply replays the (flushed) log onto f's current text. The edit commands
// computed addresses against the unmodified buffer; textinsert/textdelete here
// shift the live selection to match (elogapply).
func (a *Acme) elogapply(f *File) {
	elogflush(f)
	log := f.elogbuf
	t := f.curtext

	mod := false
	owner := 0
	if t.w != nil {
		owner = t.w.owner
		if owner == 0 {
			t.w.owner = 'E'
		}
	}

	// We constrain addresses (textconstrain) because overlapping changes
	// generate bogus addresses; we warn about out-of-sequence changes but
	// proceed, so we must keep things in range.
	for len(log) > 0 {
		b := log[len(log)-1]
		log = log[:len(log)-1]
		switch b.typ {
		default:
			// unreachable
			bug("elogapply: bad type")
		case Replace:
			if !mod {
				mod = true
				a.filemark(f)
			}
			tq0, tq1 := a.textconstrain(t, b.q0, b.q0+b.nd)
			a.textdelete(t, tq0, tq1, true)
			a.textinsert(t, tq0, b.r, true)
			if t.q0 == b.q0 && t.q1 == b.q0 {
				t.q1 += len(b.r)
			}
		case Delete:
			if !mod {
				mod = true
				a.filemark(f)
			}
			tq0, tq1 := a.textconstrain(t, b.q0, b.q0+b.nd)
			a.textdelete(t, tq0, tq1, true)
		case Insert:
			if !mod {
				mod = true
				a.filemark(f)
			}
			tq0, _ := a.textconstrain(t, b.q0, b.q0)
			a.textinsert(t, tq0, b.r, true)
			if t.q0 == b.q0 && t.q1 == b.q0 {
				t.q1 += len(b.r)
			}
		}
	}
	f.elogbuf = log
	a.elogterm(f)

	// Bad addresses would crash bufload, so double-check; if changes were
	// out of order we expect problems and don't complain further.
	if t.q0 > f.b.nc() || t.q1 > f.b.nc() || t.q0 > t.q1 {
		// textconstrain keeps the live selection in range while the log is
		// applied, so this "can't happen" backstop is never reached via -D.
		// untested
		if !a.warned {
			// untested
			a.warning(nil, "elogapply: can't happen %d %d %d\n", t.q0, t.q1, f.b.nc())
		}
		// untested
		t.q1 = min(t.q1, f.b.nc())
		t.q0 = min(t.q0, t.q1)
	}

	if t.w != nil {
		t.w.owner = owner
	}
}
