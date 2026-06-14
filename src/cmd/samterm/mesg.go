// Port of ../../../src/cmd/samterm/mesg.c and ../sam/mesg.h.
//
// The wire protocol between sam (host) and samterm (terminal). The terminal
// sends Tmesg messages and receives Hmesg messages, each framed as
// type[1] count[2] data[count].
//
// Where the C protocol carries a Text* pointer as an opaque vlong (the
// terminal sends its own pointer in Tstartfile/Tstartnewfile and the host
// echoes it back in Hbindname), the Go port assigns each Text an integer
// token and looks it up in t.texttoken — pointers can't cross the pipe.

package main

import (
	"bufio"
	"bytes"
	"fmt"
	"os"
	"unicode/utf8"

	"9fans.net/plan9port/plumb"
)

//FIX: import internal/sam instead

// Tmesg: messages originating at the terminal.
const (
	Tversion = iota
	Tstartcmdfile
	Tcheck
	Trequest
	Torigin
	Tstartfile
	Tworkfile
	Ttype
	Tcut
	Tpaste
	Tsnarf
	Tstartnewfile
	Twrite
	Tclose
	Tlook
	Tsearch
	Tsend
	Tdclick
	Tstartsnarf
	Tsetsnarf
	Tack
	Texit
	Tplumb
	TMAX
)

// Hmesg: messages originating at the host.
const (
	Hversion = iota
	Hbindname
	Hcurrent
	Hnewname
	Hmovname
	Hgrow
	Hcheck0
	Hcheck
	Hunlock
	Hdata
	Horigin
	Hunlockfile
	Hsetdot
	Hgrowdata
	Hmoveto
	Hclean
	Hdirty
	Hcut
	Hsetpat
	Hdelname
	Hclose
	Hsetsnarf
	Hsnarflen
	Hack
	Hexit
	Hplumb
	HMAX
)

const HSIZE = 3 // type + short count

// token assigns (if needed) and returns tx's opaque protocol id.
func (t *Term) token(tx *Text) int64 {
	if t.texttoken == nil {
		t.texttoken = make(map[int64]*Text)
	}
	if tx.token == 0 {
		t.nexttoken++
		tx.token = t.nexttoken
		t.texttoken[tx.token] = tx
	}
	return tx.token
}

func (t *Term) rcv() {
	for c := t.rcvchar(); c >= 0; c = t.rcvchar() {
		switch t.rcvstate {
		case 0:
			t.h.htype = c
			t.rcvstate++

		case 1:
			t.h.count0 = c
			t.rcvstate++

		case 2:
			t.h.count1 = c
			t.rcvcount = t.h.count0 | (t.h.count1 << 8)
			t.rcvi = 0
			if t.rcvcount > DATASIZE {
				t.rcverrs++
				if t.rcverrs < 5 {
					t.dumperrmsg(t.rcvcount, t.h.htype, t.h.count0, c)
					t.rcvstate = 0
					continue
				}
				// unreachable
				panic("count>DATASIZE")
			}
			if t.rcvcount == 0 {
				goto Complete
			}
			t.rcvstate++

		case 3:
			t.indata[t.rcvi] = byte(c)
			if t.rcvi++; t.rcvi == t.rcvcount {
				goto Complete
			}
		}
		continue

	Complete:
		t.indata[t.rcvcount] = 0
		t.inmesg(t.h.htype, t.rcvcount)
		t.rcvstate = 0
		t.rcvcount = 0
	}
}

func (t *Term) inmesg(typ, count int) {
	m := t.inshort(0)
	l := t.inlong(2)
	switch typ {
	default:
		// unreachable
		panic("rcv unknown")

	case Hversion:
		t.hversion = m

	case Hbindname:
		l := t.invlong(2) // for 64-bit pointers / our token
		i := t.whichmenu(m)
		if i < 0 {
			break
		}
		// in case of a race, a bindname may already have occurred
		//FIX: delete whichtextOrNil and use whichmenu to identify whether m is okay
		tx := t.whichtextOrNil(m)
		if tx == nil {
			tx = t.texttoken[l]
		} else {
			// let the old one win; clean up the new one.
			// untested: a second Hbindname for an already-bound tag -- sam binds
			// each file once, so the harness never produces the double bind.
			nw := t.texttoken[l]
			// untested: see above
			for nw != nil && nw.nwin > 0 {
				// untested: see above
				t.closeup(&nw.l[nw.front])
			}
		}
		t.text[i] = tx
		t.text[i].tag = m

	case Hcurrent:
		if t.whichmenu(m) < 0 {
			break
		}
		tx := t.whichtext(m)
		i := t.which != nil && t.which.user1 == &t.cmd && m != t.cmd.tag
		if tx == nil {
			// untested: sam only sends a file Hcurrent right after the terminal
			// opens it (Tstartfile/Tstartnewfile), so the menu entry always has a
			// text by now; an Hcurrent for a windowless file does not occur.
			tx = t.sweeptext(0, m)
			if tx == nil {
				// untested: see above
				break
			}
		}
		if tx.l[tx.front].textfn == nil {
			// unreachable
			panic("Hcurrent")
		}
		lp := &tx.l[tx.front]
		if i {
			// untested: requires the command window to be current when a *file*
			// Hcurrent arrives, but sam sends a file Hcurrent only at open time,
			// when sweeptext has just reset `which` to nil -- so i is never true.
			t.flupfront(lp)
			t.flborder(lp, 0)
			t.work = lp
		} else {
			t.current(lp)
		}

	case Hmovname:
		//FIX: rename mm to m, ll to tag, ii to mod, pos to m
		mm := t.whichmenu(m)
		if mm < 0 {
			break
		}
		tx := t.text[mm]
		ll := t.tag[mm]
		ii := t.name[mm][0]
		t.text[mm] = nil // suppress panic in menudel
		t.menudel(mm)
		var pos int
		if tx == &t.cmd {
			pos = 0
		} else {
			//FIX: use cond
			pos = 0
			if t.nname > 0 && t.text[0] == &t.cmd {
				pos = 1
			}
			for ; pos < t.nname; pos++ {
				//FIX: use cstr and bytes.Compare
				if strcmpz(t.indata[2:], t.name[pos][1:]) < 0 {
					break
				}
			}
		}
		t.menuins(pos, cstr(t.indata[2:]), tx, int(ii), ll)

	case Hgrow:
		if t.whichmenu(m) >= 0 {
			t.hgrow(m, l, t.inlong(6), 1)
		}

	case Hnewname:
		t.menuins(0, []byte(""), nil, ' ', m)

	case Hcheck0:
		i := t.whichmenu(m)
		if i >= 0 {
			tx := t.text[i]
			if tx != nil {
				tx.lock++
			}
			t.outTs(Tcheck, m)
		}

	case Hcheck:
		i := t.whichmenu(m)
		if i >= 0 {
			tx := t.text[i]
			if tx != nil && tx.lock != 0 {
				tx.lock--
			}
			t.hcheck(m)
		}

	case Hunlock:
		t.clrlock()

	case Hdata:
		if t.whichmenu(m) >= 0 {
			l += t.hdata(m, l, t.indata[6:count], count-6)
		}
		t.checkscroll(m, l)

	case Horigin:
		if t.whichmenu(m) >= 0 {
			t.horigin(m, l)
		}

	case Hunlockfile:
		if t.whichmenu(m) >= 0 {
			if tx := t.whichtext(m); tx.lock != 0 {
				tx.lock--
				t.checkscroll(m, -1)
			}
		}

	case Hsetdot:
		if t.whichmenu(m) >= 0 {
			t.hsetdot(m, l, t.inlong(6))
		}

	case Hgrowdata:
		if t.whichmenu(m) < 0 {
			break
		}
		t.hgrow(m, l, t.inlong(6), 0)
		t.whichtext(m).lock++ // fake the request
		l += t.hdata(m, l, t.indata[10:count], count-10)
		t.checkscroll(m, l)

	case Hmoveto:
		if t.whichmenu(m) >= 0 {
			t.hmoveto(m, l)
		}

	case Hclean:
		if mm := t.whichmenu(m); mm >= 0 {
			t.name[mm][0] = ' '
		}

	case Hdirty:
		if mm := t.whichmenu(m); mm >= 0 {
			t.name[mm][0] = '\''
		}

	case Hdelname:
		if mm := t.whichmenu(m); mm >= 0 {
			t.menudel(mm)
		}

	case Hcut:
		if t.whichmenu(m) >= 0 {
			t.hcut(m, l, t.inlong(6))
		}

	case Hclose:
		if t.whichmenu(m) < 0 {
			break
		}
		tx := t.whichtext(m)
		nw := tx.nwin
		for i := 0; nw > 0 && i < NL; i++ {
			if tx.l[i].textfn != nil {
				//FIX: rename closeup to close
				t.closeup(&tx.l[i])
				nw--
			}
		}

	case Hsetpat:
		t.setpat(string(cstr(t.indata[:])))

	case Hsetsnarf:
		t.hsetsnarf(m)

	case Hsnarflen:
		t.snarflen = t.inlong(0)

	case Hack:
		t.outT0(Tack)

	case Hexit:
		t.exiting = true
		t.outT0(Texit)
		t.exit()

	case Hplumb:
		t.hplumb(m)
	}
}

func (t *Term) checkscroll(m, l int) {
	if m == t.cmd.tag {
		//FIX: use range
		for i := 0; i < NL; i++ {
			lp := &t.cmd.l[i]
			if lp.textfn != nil {
				//FIX: use cond(l >= 0, ...)
				a := l
				if l < 0 {
					a = lp.p1
				}
				t.center(lp, a)
			}
		}
	}
}

// FIX: delete
// whichtextOrNil is whichtext without the panic (for the Hbindname race).
func (t *Term) whichtextOrNil(tg int) *Text {
	for i := 0; i < t.nname; i++ {
		if t.tag[i] == tg {
			return t.text[i]
		}
	}
	// untested: the only caller (Hbindname) first checks whichmenu(tg) >= 0, so
	// tg is always present and the loop above always returns.
	return nil
}

func (t *Term) setlock() {
	t.hostlock++
	t.cursor = &t.lockarrow
	t.setcursor(t.cursor)
}

func (t *Term) clrlock() {
	t.hasunlocked = 1
	if t.hostlock > 0 {
		t.hostlock--
	}
	if t.hostlock == 0 {
		t.cursor = nil
		t.setcursor(nil)
	}
}

func (t *Term) startfile(tx *Text) {
	t.outTsv(Tstartfile, tx.tag, t.token(tx))
	t.setlock()
}

func (t *Term) startnewfile(typ int, tx *Text) {
	tx.tag = Untagged
	t.outTv(typ, t.token(tx))
}

func (t *Term) inshort(n int) int {
	//FIX: use binary.LittleEndian
	return int(t.indata[n]) | int(t.indata[n+1])<<8
}

func (t *Term) inlong(n int) int {
	//FIX: use binary.LittleEndian
	return int(t.indata[n]) | int(t.indata[n+1])<<8 |
		int(t.indata[n+2])<<16 | int(t.indata[n+3])<<24
}

func (t *Term) invlong(n int) int64 {
	//FIX: use binary.LittleEndian
	v := int64(t.indata[n+7])<<24 | int64(t.indata[n+6])<<16 | int64(t.indata[n+5])<<8 | int64(t.indata[n+4])
	v = v<<16 | int64(t.indata[n+3])<<8 | int64(t.indata[n+2])
	v = v<<16 | int64(t.indata[n+1])<<8 | int64(t.indata[n])
	return v
}

func (t *Term) outT0(typ int) {
	t.outstart(typ)
	t.outsend()
}

// outTl is unused -- no T message carries a lone long -- but is kept for parity
// with C samterm's mesg.c, which also defines it and never calls it.
func (t *Term) outTl(typ int, l int) {
	// untested: see outTl doc (dead in C too)
	t.outstart(typ)
	t.outlong(l)
	t.outsend()
}

func (t *Term) outTs(typ, s int) {
	t.outstart(typ)
	t.outshort(s)
	t.outsend()
}

// outTss is unused -- no T message carries two shorts -- but is kept for parity
// with C samterm's mesg.c, which also defines it and never calls it.
func (t *Term) outTss(typ, s1, s2 int) {
	// untested: see outTss doc (dead in C too)
	t.outstart(typ)
	t.outshort(s1)
	t.outshort(s2)
	t.outsend()
}

func (t *Term) outTsll(typ, s1, l1, l2 int) {
	t.outstart(typ)
	t.outshort(s1)
	t.outlong(l1)
	t.outlong(l2)
	t.outsend()
}

func (t *Term) outTsl(typ, s1, l1 int) {
	t.outstart(typ)
	t.outshort(s1)
	t.outlong(l1)
	t.outsend()
}

func (t *Term) outTsv(typ, s1 int, v1 int64) {
	t.outstart(typ)
	t.outshort(s1)
	t.outvlong(v1)
	t.outsend()
}

func (t *Term) outTv(typ int, v1 int64) {
	t.outstart(typ)
	t.outvlong(v1)
	t.outsend()
}

func (t *Term) outTslS(typ, s1, l1 int, s []rune) {
	t.outstart(typ)
	t.outshort(s1)
	t.outlong(l1)
	var buf []byte
	for _, r := range s {
		buf = utf8.AppendRune(buf, r)
	}
	buf = append(buf, 0)
	t.outcopy(buf)
	t.outsend()
}

func (t *Term) outTsls(typ, s1, l1, s2 int) {
	t.outstart(typ)
	t.outshort(s1)
	t.outlong(l1)
	t.outshort(s2)
	t.outsend()
}

// FIX: give the message types type Tmesg and use here as type of typ
func (t *Term) outstart(typ int) {
	t.outdata[0] = byte(typ)
	t.outcount = 0
}

func (t *Term) outcopy(data []byte) {
	//FIX: make outdata []byte and make outcount len(outdata)
	for _, b := range data {
		t.outdata[HSIZE+t.outcount] = b
		t.outcount++
	}
}

func (t *Term) outshort(s int) {
	//FIX: use binary.LittleEndian.Append
	t.outcopy([]byte{byte(s), byte(s >> 8)})
}

func (t *Term) outlong(l int) {
	//FIX: use binary.LittleEndian.Append
	t.outcopy([]byte{byte(l), byte(l >> 8), byte(l >> 16), byte(l >> 24)})
}

func (t *Term) outvlong(v int64) {
	//FIX: use binary.LittleEndian.Append
	var buf [8]byte
	for i := 0; i < 8; i++ {
		buf[i] = byte(v)
		v >>= 8
	}
	t.outcopy(buf[:])
}

func (t *Term) outsend() {
	if t.outcount > DATASIZE-HSIZE {
		// unreachable
		panic("outcount>sizeof outdata")
	}
	t.outdata[1] = byte(t.outcount)
	t.outdata[2] = byte(t.outcount >> 8)
	n := t.outcount + HSIZE
	if _, err := t.hostw.Write(t.outdata[:n]); err != nil {
		// The host (sam) is gone: its read end closed under us. C samterm
		// panics here (abort), but in-process that would crash the test binary,
		// so treat it as a disconnect and exit the event loop instead -- the
		// same outcome the C process reaches, just without taking the harness
		// down with it.
		// untested: requires sam's read end to close while samterm is mid-write;
		// the harness shuts sam down only after settlement.
		t.exiting = true
	}
}

func (t *Term) hsetdot(m, p0, p1 int) {
	tx := t.whichtext(m)
	l := &tx.l[tx.front]
	t.flushtyping(1)
	t.flsetselect(l, p0, p1)
}

func (t *Term) horigin(m, p0 int) {
	tx := t.whichtext(m)
	l := &tx.l[tx.front]
	if !t.flprepare(l) {
		// untested: a Horigin for a front window that cannot prepare (fully
		// hidden) -- the front window is normally visible, and producing a hidden
		// front window that sam then scrolls is not reproducible deterministically.
		l.origin = p0
		return
	}
	a := p0 - l.origin
	if a >= 0 && a < l.f.NumChars {
		l.f.Delete(0, a)
	} else if a < 0 && -a < l.f.NumChars {
		r := t.rload(&tx.rasp, p0, l.origin)
		l.f.Insert(r, 0)
	} else {
		l.f.Delete(0, l.f.NumChars)
	}
	l.origin = p0
	t.scrdraw(l, tx.rasp.nrunes)
	if l.visible == Some {
		// untested: a Horigin for a partially-covered front window; same reason
		// as the !flprepare branch above.
		t.flrefresh(l, l.entire, 0)
	}
	t.hcheck(m)
}

func (t *Term) hmoveto(m, p0 int) {
	tx := t.whichtext(m)
	l := &tx.l[tx.front]
	if p0 < l.origin || p0-l.origin > l.f.NumChars*9/10 {
		t.outTsll(Torigin, m, p0, 2)
	}
}

func (t *Term) hcheck(m int) {
	if m == Untagged {
		// untested: hcheck is only reached for a tagged file (its callers pass a
		// bound tag); flnewlyvisible's Untagged path needs an unbound window,
		// which exists only transiently during build.
		return
	}
	tx := t.whichtext(m)
	if tx == nil {
		// untested: a half-built window whose text is not yet wired -- transient.
		return
	}
	reqd := false
	//FIX: use range
	for i := 0; i < NL; i++ {
		l := &tx.l[i]
		if l.textfn == nil || !t.flprepare(l) {
			continue
		}
		a := tx.l[i].origin
		n := tx.rasp.contig(a, a+l.f.NumChars, true)
		if n < l.f.NumChars {
			// text missing in middle of screen.
			// untested: a partial fill (some rows present, then a hole) at the
			// instant hcheck runs -- a transient that needs sam's data to be
			// mid-flight, which the frame-synced harness never catches.
			a += n
		} else {
			// text missing at end of screen?
		Again:
			if l.f.LastLineFull {
				goto Checksel // all's well
			}
			a = tx.l[i].origin + l.f.NumChars
			n = tx.rasp.nrunes - a
			if n == 0 {
				goto Checksel
			}
			if n > TBLOCKSIZE {
				n = TBLOCKSIZE
			}
			n = tx.rasp.contig(a, a+n, true)
			if n > 0 {
				t.rload(&tx.rasp, a, a+n)
				nl := l.f.NumChars
				r := t.scratch[:n]
				//FIX: lost r+n arg to flinsert
				t.flinsert(l, r, l.origin+nl)
				if nl == l.f.NumChars { // made no progress
					goto Checksel
				}
				goto Again
			}
		}
		if !reqd {
			n = tx.rasp.contig(a, a+TBLOCKSIZE, false)
			if n <= 0 {
				// unreachable
				panic("hcheck request==0")
			}
			t.outTsls(Trequest, m, a, n)
			t.outTs(Tcheck, m)
			tx.lock++ // for the Trequest
			tx.lock++ // for the Tcheck
			reqd = true
		}
	Checksel:
		t.flsetselect(l, l.p0, l.p1)
	}
}

func (t *Term) flnewlyvisible(l *Flayer) {
	t.hcheck(l.user1.tag)
}

func (t *Term) hsetsnarf(nc int) {
	t.setcursor(&t.deadmouse)
	s2 := make([]byte, nc)
	for i := 0; i < nc; i++ {
		s2[i] = byte(t.getch())
	}
	// FIX: make snarfswap return a slice of length n instead of returning n
	s1, n := t.snarfswap(s2, nc)
	if n >= 0 {
		if s1 == nil {
			n = 0
		}
		if n > SNARFSIZE {
			// untested: Go's snarfswap returns n = len(s1) <= SNARFSIZE (it reads
			// into a SNARFSIZE buffer), so n never exceeds it -- the guard mirrors
			// C, whose plan9 read could in principle return more.
			s1 = []byte("<snarf too long>")
			n = len(s1)
		} else {
			if len(s1) > n {
				// untested: snarfswap returns n == len(s1), so this never trims.
				s1 = s1[:n]
			}
		}
		t.snarflen = n
		t.outTs(Tsetsnarf, n)
		if n > 0 {
			if _, err := t.hostw.Write(s1[:n]); err != nil {
				// unreachable
				panic("snarf write error")
			}
		}
	} else {
		// untested: snarfswap returns n = len(...) >= 0, never negative.
		t.outTs(Tsetsnarf, 0)
	}
	t.setcursor(t.cursor)
}

func (t *Term) hplumb(nc int) {
	// sam forwards a serialized plumb message (built in its Tplumb handler); read
	// the nc bytes and, if the send port is open, re-send them to the plumber.
	// With no plumber (plumbsendfid == nil) the bytes are simply dropped, keeping
	// the protocol in sync -- as C does when plumbfd <= 0.
	s := make([]byte, nc)
	for i := 0; i < nc; i++ {
		s[i] = byte(t.getch())
	}
	if t.plumbsendfid == nil {
		return
	}
	var m plumb.Message
	if err := m.Recv(bufio.NewReader(bytes.NewReader(s))); err != nil {
		//TODO: does C print this?
		fmt.Fprintf(os.Stderr, "samterm: plumb unpack: %v\n", err)
		return
	}
	if err := m.Send(t.plumbsendfid); err != nil {
		// untested: the write to the plumber's send port fails only if the
		// plumber dies mid-test, which the harness keeps alive.
		//TODO: does C print this?
		fmt.Fprintf(os.Stderr, "samterm: plumb send: %v\n", err)
	}
}

func (t *Term) hgrow(m, a, new, req int) {
	if new <= 0 {
		// unreachable
		panic("hgrow")
	}
	tx := t.whichtext(m)
	tx.rasp.resize(a, 0, new)
	//FIX: use range
	for i := 0; i < NL; i++ {
		l := &tx.l[i]
		if l.textfn == nil {
			continue
		}
		o := l.origin
		b := a - o - tx.rasp.missing(o, a)
		if a < o {
			l.origin += new
		}
		if a < l.p0 {
			l.p0 += new
		}
		if a < l.p1 {
			l.p1 += new
		}
		// must prevent b temporarily becoming unsigned
		if req == 0 || a < o || (b > 0 && b > l.f.NumChars) ||
			(l.f.NumChars == 0 && a-o > 0) {
			continue
		}
		if new > TBLOCKSIZE {
			new = TBLOCKSIZE
		}
		t.outTsls(Trequest, m, a, new)
		tx.lock++
		req = 0
	}
}

func (t *Term) hdata1(tx *Text, a int, r []rune) int {
	//FIX: use range
	for i := 0; i < NL; i++ {
		l := &tx.l[i]
		if l.textfn == nil {
			continue
		}
		o := l.origin
		b := a - o - tx.rasp.missing(o, a)
		if a < o || (b > 0 && b > l.f.NumChars) {
			continue
		}
		t.flinsert(l, r, o+b)
	}
	tx.rasp.data(a, a+len(r), r)
	tx.rasp.clean()
	return len(r)
}

// FIX: change caller ot pass s of correct length, drop length arg
func (t *Term) hdata(m, a int, s []byte, length int) int {
	tx := t.whichtext(m)
	if tx.lock != 0 {
		tx.lock--
	}
	if length == 0 {
		return 0
	}
	r := make([]rune, 0, length)
	for i := 0; i < length; {
		ru, w := utf8.DecodeRune(s[i:])
		r = append(r, ru)
		i += w
	}
	return t.hdata1(tx, a, r)
}

// FIX: change caller to pass r of correct length, drop length arg
func (t *Term) hdatarune(m, a int, r []rune, length int) int {
	tx := t.whichtext(m)
	if tx.lock != 0 {
		tx.lock--
	}
	if length == 0 {
		// untested: hdatarune's only caller (typing) inserts at least one rune
		return 0
	}
	return t.hdata1(tx, a, r[:length])
}

func (t *Term) hcut(m, a, old int) {
	tx := t.whichtext(m)
	if tx.lock != 0 {
		tx.lock--
	}
	//FIX: use range
	for i := 0; i < NL; i++ {
		l := &tx.l[i]
		if l.textfn == nil {
			continue
		}
		o := l.origin
		b := a - o - tx.rasp.missing(o, a)
		// must prevent b temporarily becoming unsigned
		if (b < 0 || b < l.f.NumChars) && a+old >= o {
			//FIX: use cond
			lo := o + b
			if b < 0 {
				lo = o
			}
			t.fldelete(l, lo, a+old-tx.rasp.missing(o, a+old))
		}
		if a+old < o {
			l.origin -= old
		} else if a <= o {
			l.origin = a
		}
		if a+old < l.p0 {
			l.p0 -= old
		} else if a <= l.p0 {
			l.p0 = a
		}
		if a+old < l.p1 {
			l.p1 -= old
		} else if a <= l.p1 {
			l.p1 = a
		}
	}
	tx.rasp.resize(a, old, 0)
	tx.rasp.clean()
}

// FIX: does the protocol actually send the NULs? if not, drop use of this
// cstr returns the bytes of b up to (not including) the first NUL.
func cstr(b []byte) []byte {
	//FIX: use bytes.IndexByte
	for i, c := range b {
		if c == 0 {
			return b[:i]
		}
	}
	return b
}

// strcmpz compares two byte slices as C strings (each up to its first NUL).
func strcmpz(a, b []byte) int {
	a = cstr(a)
	b = cstr(b)
	for i := 0; i < len(a) && i < len(b); i++ {
		if a[i] != b[i] {
			if a[i] < b[i] {
				return -1
			}
			return 1
		}
	}
	switch {
	case len(a) < len(b):
		return -1
	case len(a) > len(b):
		return 1
	}
	return 0
}
