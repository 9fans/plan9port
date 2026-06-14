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
	"encoding/binary"
	"fmt"
	"os"
	"unicode/utf8"

	"9fans.net/plan9port/internal/sam"
	"9fans.net/plan9port/plumb"
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
			t.h.htype = sam.Hmesg(c)
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
					t.dumperrmsg(t.rcvcount, int(t.h.htype), t.h.count0, c)
					t.rcvstate = 0
					continue
				}
				// unreachable
				panic("count>DATASIZE")
			}
			t.indata = t.indata[:t.rcvcount]
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
		t.inmesg(t.h.htype)
		t.rcvstate = 0
		t.rcvcount = 0
	}
}

func (t *Term) inmesg(typ sam.Hmesg) {
	m := t.inshort(0)
	l := t.inlong(2)
	switch typ {
	default:
		// unreachable
		panic("rcv unknown")

	case sam.Hversion:
		t.hversion = m

	case sam.Hbindname:
		l := t.invlong(2) // for 64-bit pointers / our token
		i := t.whichmenu(m)
		if i < 0 {
			break
		}
		// in case of a race, a bindname may already have occurred.
		tx := t.text[i]
		if tx == nil {
			tx = t.texttoken[l]
		} else {
			// let the old one win; clean up the new one.
			nw := t.texttoken[l]
			for nw != nil && nw.nwin > 0 {
				// untested: the duplicate text whose token is re-bound has live
				// windows to close. The bindtwice proto script reaches this branch
				// with an empty token (no duplicate windows); a duplicate that
				// actually has windows needs a second windowed text, which the
				// cmd-only proto host cannot create.
				t.close(&nw.l[nw.front])
			}
		}
		t.text[i] = tx
		t.text[i].tag = m

	case sam.Hcurrent:
		if t.whichmenu(m) < 0 {
			break
		}
		tx := t.whichtext(m)
		i := t.which != nil && t.which.user1 == &t.cmd && m != t.cmd.tag
		if tx == nil {
			// untested: sam only sends a file Hcurrent right after the terminal
			// opens it (Tstartfile/Tstartnewfile), so the menu entry always has a
			// text by now; an Hcurrent for a windowless file does not occur.
			tx = t.sweeptext(false, m)
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
			t.flborder(lp, false)
			t.work = lp
		} else {
			t.current(lp)
		}

	case sam.Hmovname:
		m := t.whichmenu(m)
		if m < 0 {
			break
		}
		tx := t.text[m]
		tag := t.tag[m]
		mod := t.name[m][0]
		t.text[m] = nil // suppress panic in menudel
		t.menudel(m)
		if tx == &t.cmd {
			m = 0
		} else {
			m = cond(len(t.text) > 0 && t.text[0] == &t.cmd, 1, 0)
			for ; m < len(t.text); m++ {
				if bytes.Compare(t.indata[2:], t.name[m][1:]) < 0 {
					break
				}
			}
		}
		t.menuins(m, t.indata[2:], tx, mod, tag)

	case sam.Hgrow:
		if t.whichmenu(m) >= 0 {
			t.hgrow(m, l, t.inlong(6), 1)
		}

	case sam.Hnewname:
		t.menuins(0, []byte(""), nil, ' ', m)

	case sam.Hcheck0:
		i := t.whichmenu(m)
		if i >= 0 {
			tx := t.text[i]
			if tx != nil {
				tx.lock++
			}
			t.outTs(sam.Tcheck, m)
		}

	case sam.Hcheck:
		i := t.whichmenu(m)
		if i >= 0 {
			tx := t.text[i]
			if tx != nil && tx.lock != 0 {
				tx.lock--
			}
			t.hcheck(m)
		}

	case sam.Hunlock:
		t.clrlock()

	case sam.Hdata:
		if t.whichmenu(m) >= 0 {
			l += t.hdata(m, l, t.indata[6:])
		}
		t.checkscroll(m, l)

	case sam.Horigin:
		if t.whichmenu(m) >= 0 {
			t.horigin(m, l)
		}

	case sam.Hunlockfile:
		if t.whichmenu(m) >= 0 {
			if tx := t.whichtext(m); tx.lock != 0 {
				tx.lock--
				t.checkscroll(m, -1)
			}
		}

	case sam.Hsetdot:
		if t.whichmenu(m) >= 0 {
			t.hsetdot(m, l, t.inlong(6))
		}

	case sam.Hgrowdata:
		if t.whichmenu(m) < 0 {
			break
		}
		t.hgrow(m, l, t.inlong(6), 0)
		t.whichtext(m).lock++ // fake the request
		l += t.hdata(m, l, t.indata[10:])
		t.checkscroll(m, l)

	case sam.Hmoveto:
		if t.whichmenu(m) >= 0 {
			t.hmoveto(m, l)
		}

	case sam.Hclean:
		if mm := t.whichmenu(m); mm >= 0 {
			t.name[mm][0] = ' '
		}

	case sam.Hdirty:
		if mm := t.whichmenu(m); mm >= 0 {
			t.name[mm][0] = '\''
		}

	case sam.Hdelname:
		if mm := t.whichmenu(m); mm >= 0 {
			t.menudel(mm)
		}

	case sam.Hcut:
		if t.whichmenu(m) >= 0 {
			t.hcut(m, l, t.inlong(6))
		}

	case sam.Hclose:
		if t.whichmenu(m) < 0 {
			break
		}
		tx := t.whichtext(m)
		nw := tx.nwin
		for i := 0; nw > 0 && i < NL; i++ {
			if tx.l[i].textfn != nil {
				t.close(&tx.l[i])
				nw--
			}
		}

	case sam.Hsetpat:
		t.setpat(string(t.indata))

	case sam.Hsetsnarf:
		t.hsetsnarf(m)

	case sam.Hsnarflen:
		t.snarflen = t.inlong(0)

	case sam.Hack:
		t.outT0(sam.Tack)

	case sam.Hexit:
		t.exiting = true
		t.outT0(sam.Texit)

	case sam.Hplumb:
		t.hplumb(m)
	}
}

func (t *Term) checkscroll(m, l int) {
	if m == t.cmd.tag {
		for i := range t.cmd.l {
			lp := &t.cmd.l[i]
			if lp.textfn != nil {
				t.center(lp, cond(l >= 0, l, lp.p1))
			}
		}
	}
}

func (t *Term) setlock() {
	t.hostlock++
	t.cursor = &lockarrow
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
	t.outTsv(sam.Tstartfile, tx.tag, t.token(tx))
	t.setlock()
}

func (t *Term) startnewfile(typ sam.Tmesg, tx *Text) {
	tx.tag = Untagged
	t.outTv(typ, t.token(tx))
}

// The explicit high bounds reach past len(indata) into its capacity: like C,
// inmesg reads the m and l header fields before switching on the type, even
// for messages too short to carry them (the values, stale bytes, go unused).
func (t *Term) inshort(n int) int {
	return int(binary.LittleEndian.Uint16(t.indata[n : n+2]))
}

func (t *Term) inlong(n int) int {
	return int(binary.LittleEndian.Uint32(t.indata[n : n+4]))
}

func (t *Term) invlong(n int) int64 {
	return int64(binary.LittleEndian.Uint64(t.indata[n : n+8]))
}

func (t *Term) outT0(typ sam.Tmesg) {
	t.outstart(typ)
	t.outsend()
}

// outTl is unused -- no T message carries a lone long -- but is kept for parity
// with C samterm's mesg.c, which also defines it and never calls it.
func (t *Term) outTl(typ sam.Tmesg, l int) {
	// untested: see outTl doc (dead in C too)
	t.outstart(typ)
	t.outlong(l)
	t.outsend()
}

func (t *Term) outTs(typ sam.Tmesg, s int) {
	t.outstart(typ)
	t.outshort(s)
	t.outsend()
}

// outTss is unused -- no T message carries two shorts -- but is kept for parity
// with C samterm's mesg.c, which also defines it and never calls it.
func (t *Term) outTss(typ sam.Tmesg, s1, s2 int) {
	// untested: see outTss doc (dead in C too)
	t.outstart(typ)
	t.outshort(s1)
	t.outshort(s2)
	t.outsend()
}

func (t *Term) outTsll(typ sam.Tmesg, s1, l1, l2 int) {
	t.outstart(typ)
	t.outshort(s1)
	t.outlong(l1)
	t.outlong(l2)
	t.outsend()
}

func (t *Term) outTsl(typ sam.Tmesg, s1, l1 int) {
	t.outstart(typ)
	t.outshort(s1)
	t.outlong(l1)
	t.outsend()
}

func (t *Term) outTsv(typ sam.Tmesg, s1 int, v1 int64) {
	t.outstart(typ)
	t.outshort(s1)
	t.outvlong(v1)
	t.outsend()
}

func (t *Term) outTv(typ sam.Tmesg, v1 int64) {
	t.outstart(typ)
	t.outvlong(v1)
	t.outsend()
}

func (t *Term) outTslS(typ sam.Tmesg, s1, l1 int, s []rune) {
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

func (t *Term) outTsls(typ sam.Tmesg, s1, l1, s2 int) {
	t.outstart(typ)
	t.outshort(s1)
	t.outlong(l1)
	t.outshort(s2)
	t.outsend()
}

func (t *Term) outstart(typ sam.Tmesg) {
	t.outdata = append(t.outdata[:0], byte(typ), 0, 0)
}

func (t *Term) outcopy(data []byte) {
	t.outdata = append(t.outdata, data...)
}

func (t *Term) outshort(s int) {
	t.outcopy(binary.LittleEndian.AppendUint16(nil, uint16(s)))
}

func (t *Term) outlong(l int) {
	t.outcopy(binary.LittleEndian.AppendUint32(nil, uint32(l)))
}

func (t *Term) outvlong(v int64) {
	t.outcopy(binary.LittleEndian.AppendUint64(nil, uint64(v)))
}

func (t *Term) outsend() {
	outcount := len(t.outdata) - HSIZE
	if outcount > DATASIZE-HSIZE {
		// unreachable
		panic("outcount>sizeof outdata")
	}
	t.outdata[1] = byte(outcount)
	t.outdata[2] = byte(outcount >> 8)
	if _, err := t.hostw.Write(t.outdata); err != nil {
		// untested: requires sam's read end to close while samterm is mid-write;
		// the harness shuts sam down only after settlement.
		panic(hostError{fmt.Sprintf("write error: %v", err)})
	}
}

func (t *Term) hsetdot(m, p0, p1 int) {
	tx := t.whichtext(m)
	l := &tx.l[tx.front]
	t.flushtyping(true)
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
		r := t.load(&tx.rasp, p0, l.origin)
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
		t.outTsll(sam.Torigin, m, p0, 2)
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
	for i := range tx.l {
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
				t.load(&tx.rasp, a, a+n)
				nl := l.f.NumChars
				r := t.scratch[:n]
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
			t.outTsls(sam.Trequest, m, a, n)
			t.outTs(sam.Tcheck, m)
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
	t.setcursor(&deadmouse)
	s2 := make([]byte, nc)
	for i := 0; i < nc; i++ {
		s2[i] = byte(t.getch())
	}
	s1 := t.snarfswap(s2)
	if len(s1) > SNARFSIZE {
		// untested: Go's snarfswap returns at most SNARFSIZE bytes (it reads into a
		// SNARFSIZE buffer), so this never fires -- the guard mirrors C, whose plan9
		// read could in principle return more.
		s1 = []byte("<snarf too long>")
	}
	t.snarflen = len(s1)
	t.outTs(sam.Tsetsnarf, len(s1))
	if len(s1) > 0 {
		if _, err := t.hostw.Write(s1); err != nil {
			// unreachable
			panic("snarf write error")
		}
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
	for i := range tx.l {
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
		t.outTsls(sam.Trequest, m, a, new)
		tx.lock++
		req = 0
	}
}

func (t *Term) hdata1(tx *Text, a int, r []rune) int {
	for i := range tx.l {
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

func (t *Term) hdata(m, a int, s []byte) int {
	tx := t.whichtext(m)
	if tx.lock != 0 {
		tx.lock--
	}
	if len(s) == 0 {
		return 0
	}
	r := make([]rune, 0, len(s))
	for i := 0; i < len(s); {
		ru, w := utf8.DecodeRune(s[i:])
		r = append(r, ru)
		i += w
	}
	return t.hdata1(tx, a, r)
}

func (t *Term) hdatarune(m, a int, r []rune) int {
	tx := t.whichtext(m)
	if tx.lock != 0 {
		tx.lock--
	}
	if len(r) == 0 {
		// untested: hdatarune's only caller (typing) inserts at least one rune
		return 0
	}
	return t.hdata1(tx, a, r)
}

func (t *Term) hcut(m, a, old int) {
	tx := t.whichtext(m)
	if tx.lock != 0 {
		tx.lock--
	}
	for i := range tx.l {
		l := &tx.l[i]
		if l.textfn == nil {
			continue
		}
		o := l.origin
		b := a - o - tx.rasp.missing(o, a)
		// must prevent b temporarily becoming unsigned
		if (b < 0 || b < l.f.NumChars) && a+old >= o {
			t.fldelete(l, cond(b < 0, o, o+b), a+old-tx.rasp.missing(o, a+old))
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
