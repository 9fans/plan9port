package main

import (
	"fmt"
	"os"
	"strings"
)

// Wire protocol (mesg.c, mesg.h). Tmesg = terminal->host, Hmesg = host->
// terminal. Messages are framed: type byte, count low, count high, then count
// bytes of payload (little-endian integers within).

const (
	VERSION    = 2
	TBLOCKSIZE = 512
	DATASIZE   = UTFmax*TBLOCKSIZE + 30
	SNARFSIZE  = 32000
)

// Tmesg values (mesg.h enum Tmesg).
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

// Hmesg values (mesg.h enum Hmesg).
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

func (s *Sam) meginit() {
	s.indata = make([]byte, DATASIZE+1)
	s.outdata = make([]byte, 2*DATASIZE+3)
	s.outmsg = 0
	s.outp = 0
}

// snarf copies f[p1:p2] into buf (mesg.c snarf).
func (s *Sam) snarf(f *File, p1, p2 Posn, buf *Buffer, emptyok bool) {
	if !emptyok && p1 == p2 {
		return
	}
	bufreset(buf)
	// stage through genbuf to avoid compaction problems (vestigial)
	if p2 > Posn(f.b.nc()) {
		p2 = Posn(f.b.nc())
	}
	for l := p1; l < p2; {
		i := p2 - l
		if i > BLOCKSIZE {
			i = BLOCKSIZE
		}
		bufread(&f.b, l, s.genbuf[:], int(i))
		bufinsert(buf, Posn(buf.nc()), s.genbuf[:i], int(i))
		l += i
	}
}

func (s *Sam) setgenstr(f *File, p0, p1 Posn) {
	if p0 != p1 {
		if p1-p0 >= TBLOCKSIZE {
			fail(Etoolong)
		}
		Strinsure(int(p1 - p0))
		bufread(&f.b, p0, s.genbuf[:], int(p1-p0))
		s.genstr.s = append(s.genstr.s[:0], s.genbuf[:p1-p0]...)
	} else {
		if s.snarfbuf.nc() == 0 {
			fail(Eempty)
		}
		if s.snarfbuf.nc() > TBLOCKSIZE {
			fail(Etoolong)
		}
		bufread(&s.snarfbuf, 0, s.genbuf[:], s.snarfbuf.nc())
		s.genstr.s = append(s.genstr.s[:0], s.genbuf[:s.snarfbuf.nc()]...)
	}
}

// Output encoders.

func (s *Sam) outstart(typ int) {
	s.outdata[s.outmsg] = byte(typ)
	s.outp = s.outmsg + 3
}

func (s *Sam) outcopy(data []byte) {
	copy(s.outdata[s.outp:], data)
	s.outp += len(data)
}

func (s *Sam) outshort(v int) {
	s.outdata[s.outp] = byte(v)
	s.outdata[s.outp+1] = byte(v >> 8)
	s.outp += 2
}

func (s *Sam) outlong(l int) {
	s.outdata[s.outp] = byte(l)
	s.outdata[s.outp+1] = byte(l >> 8)
	s.outdata[s.outp+2] = byte(l >> 16)
	s.outdata[s.outp+3] = byte(l >> 24)
	s.outp += 4
}

func (s *Sam) outvlong(v int64) {
	for i := 0; i < 8; i++ {
		s.outdata[s.outp] = byte(v)
		s.outp++
		v >>= 8
	}
}

func (s *Sam) outsend() {
	if s.outp >= len(s.outdata) {
		// unreachable
		s.panicf("outsend")
	}
	outcount := s.outp - s.outmsg - 3
	s.outdata[s.outmsg+1] = byte(outcount)
	s.outdata[s.outmsg+2] = byte(outcount >> 8)
	s.outmsg = s.outp
	if !s.outbuffered {
		n := s.outmsg
		if _, err := s.out.Write(s.outdata[:n]); err != nil {
			// untested
			s.rescue()
		}
		s.outmsg = 0
	}
}

func (s *Sam) outS(str *String) {
	c := Strtoc(str)
	s.outcopy([]byte(c))
}

func (s *Sam) outT0(typ int) {
	s.outstart(typ)
	s.outsend()
}

func (s *Sam) outTl(typ int, l int) {
	s.outstart(typ)
	s.outlong(l)
	s.outsend()
}

func (s *Sam) outTs(typ int, sh int) {
	s.outstart(typ)
	s.outshort(sh)
	s.outsend()
}

func (s *Sam) outTsS(typ int, s1 int, str *String) {
	s.outstart(typ)
	s.outshort(s1)
	s.outS(str)
	s.outsend()
}

func (s *Sam) outTslS(typ int, s1 int, l1 Posn, str *String) {
	s.outstart(typ)
	s.outshort(s1)
	s.outlong(int(l1))
	s.outS(str)
	s.outsend()
}

func (s *Sam) outTS(typ int, str *String) {
	s.outstart(typ)
	s.outS(str)
	s.outsend()
}

func (s *Sam) outTsllS(typ int, s1 int, l1, l2 Posn, str *String) {
	s.outstart(typ)
	s.outshort(s1)
	s.outlong(int(l1))
	s.outlong(int(l2))
	s.outS(str)
	s.outsend()
}

func (s *Sam) outTsll(typ int, sh int, l1, l2 Posn) {
	s.outstart(typ)
	s.outshort(sh)
	s.outlong(int(l1))
	s.outlong(int(l2))
	s.outsend()
}

func (s *Sam) outTsl(typ int, sh int, l Posn) {
	s.outstart(typ)
	s.outshort(sh)
	s.outlong(int(l))
	s.outsend()
}

func (s *Sam) outTsv(typ int, sh int, v int64) {
	s.outstart(typ)
	s.outshort(sh)
	s.outvlong(v)
	s.outsend()
}

func (s *Sam) needoutflush() bool {
	return s.outmsg >= DATASIZE
}

func (s *Sam) outflush() {
	if s.outmsg == 0 {
		return
	}
	s.outbuffered = false
	// flow control
	s.outT0(Hack)
	s.waitack = true
	for {
		if s.rcv() == 0 {
			// untested
			s.rescue()
			s.exits("eof")
		}
		if !s.waitack {
			break
		}
	}
	s.outmsg = 0
	s.outbuffered = true
}

// samname is the command window's file name (unix.c).
var samname = []Rune{'~', '~', 's', 'a', 'm', '~', '~', 0}

// rcvchar reads one raw byte from the protocol input, buffering like mesg.c.
func (s *Sam) rcvchar() int {
	if s.rcvn <= 0 {
		n, err := s.in.Read(s.rcvbuf[:])
		if n <= 0 || err != nil {
			return -1
		}
		s.rcvn = n
		s.rcvi = 0
	}
	s.rcvn--
	c := s.rcvbuf[s.rcvi]
	s.rcvi++
	return int(c)
}

// rcv reads bytes, assembling one framed message, then dispatches it (mesg.c).
func (s *Sam) rcv() int {
	for {
		c := s.rcvchar()
		if c == -1 {
			return 0
		}
		switch s.rcvstate {
		case 0:
			s.hdr.typ = byte(c)
			s.rcvstate++
		case 1:
			s.hdr.count0 = byte(c)
			s.rcvstate++
		case 2:
			s.hdr.count1 = byte(c)
			count := int(s.hdr.count0) | int(s.hdr.count1)<<8
			s.rcvcount = count
			s.rcvidx = 0
			if count > DATASIZE {
				s.panicf("count>DATASIZE")
			}
			if count == 0 {
				return s.deliver()
			}
			s.rcvstate++
		case 3:
			s.indata[s.rcvidx] = byte(c)
			s.rcvidx++
			if s.rcvidx == s.rcvcount {
				return s.deliver()
			}
		}
	}
}

func (s *Sam) deliver() int {
	s.indata[s.rcvidx] = 0
	s.rcvstate = 0
	s.rcvcount = 0
	return s.inmesg(int(s.hdr.typ))
}

func (s *Sam) whichfile(tag int) *File {
	for _, f := range s.file {
		if f.tag == tag {
			return f
		}
	}
	quietHiccough()
	return nil
}

func (s *Sam) inshort() int {
	n := int(s.indata[s.inp]) | int(s.indata[s.inp+1])<<8
	s.inp += 2
	return n
}

func (s *Sam) inlong() Posn {
	n := int(s.indata[s.inp]) | int(s.indata[s.inp+1])<<8 |
		int(s.indata[s.inp+2])<<16 | int(s.indata[s.inp+3])<<24
	s.inp += 4
	return Posn(n)
}

func (s *Sam) invlong() int64 {
	var v int64
	for i := 7; i >= 0; i-- {
		v = (v << 8) | int64(s.indata[s.inp+i])
	}
	s.inp += 8
	return v
}

// indataString returns the NUL-terminated text remaining at s.inp.
func (s *Sam) indataString() string {
	b := s.indata[s.inp:]
	for i, c := range b {
		if c == 0 {
			return string(b[:i])
		}
	}
	// untested: no plumber / no current file / always-NUL-terminated input
	return string(b)
}

func (s *Sam) inmesg(typ int) int {
	if typ > TMAX {
		// unreachable
		s.panicf("inmesg")
	}
	s.inp = 0
	switch typ {
	default:
		// unreachable
		s.panicf("rcv unknown")

	case Tversion:
		s.tversion = s.inshort()

	case Tstartcmdfile:
		v := s.invlong() // for 64-bit pointers
		Strdupl(&s.genstr, samname)
		s.cmd = s.newfile()
		s.cmd.unread = false
		s.outTsv(Hbindname, s.cmd.tag, v)
		s.outTs(Hcurrent, s.cmd.tag)
		s.logsetname(s.cmd, &s.genstr)
		s.cmd.rasp = []Posn{}
		s.cmd.hasrasp = true
		s.cmd.mod = false
		if s.cmdstr.n() != 0 {
			s.loginsert(s.cmd, 0, s.cmdstr.s, s.cmdstr.n())
			Strdelete(&s.cmdstr, 0, Posn(s.cmdstr.n()))
		}
		s.fileupdate(s.cmd, false, true)
		s.outT0(Hunlock)

	case Tcheck:
		// go through whichfile to check the tag
		s.outTs(Hcheck, s.whichfile(s.inshort()).tag)

	case Trequest:
		f := s.whichfile(s.inshort())
		p0 := s.inlong()
		p1 := p0 + Posn(s.inshort())
		if f.unread {
			// unreachable
			s.panicf("Trequest: unread")
		}
		if p1 > Posn(f.b.nc()) {
			p1 = Posn(f.b.nc())
		}
		// can happen e.g. scrolling during command
		if p0 > Posn(f.b.nc()) {
			p0 = Posn(f.b.nc())
		}
		var r Range
		var buf []Rune
		if p0 == p1 {
			r.p1 = p0
			r.p2 = p0
		} else {
			r = rdata(&f.rasp, p0, p1-p0)
			buf = make([]Rune, r.p2-r.p1)
			bufread(&f.b, r.p1, buf, int(r.p2-r.p1))
		}
		s.outTslS(Hdata, f.tag, r.p1, tmprstr(buf, len(buf)))

	case Torigin:
		sh := s.inshort()
		l := s.inlong()
		l1 := s.inlong()
		s.lookorigin(s.whichfile(sh), l, l1)

	case Tstartfile:
		s.termlocked++
		f := s.whichfile(s.inshort())
		if !f.hasrasp { // this might be a duplicate message
			f.rasp = []Posn{}
			f.hasrasp = true
		}
		s.current(f)
		s.outTsv(Hbindname, f.tag, s.invlong())
		s.outTs(Hcurrent, f.tag)
		if f.unread {
			s.load(f)
		} else {
			if f.b.nc() > 0 {
				rgrow(&f.rasp, 0, Posn(f.b.nc()))
				s.outTsll(Hgrow, f.tag, 0, Posn(f.b.nc()))
			}
			s.outTs(Hcheck0, f.tag)
			s.moveto(f, f.dot.r)
		}

	case Tworkfile:
		i := s.inshort()
		f := s.whichfile(i)
		s.current(f)
		f.dot.r.p1 = s.inlong()
		f.dot.r.p2 = s.inlong()
		f.tdot = f.dot.r

	case Ttype:
		f := s.whichfile(s.inshort())
		p0 := s.inlong()
		str := tmpcstr(s.indataString())
		i := str.n()
		s.loginsert(f, p0, str.s, str.n())
		if s.fileupdate(f, false, false) {
			s.seq++
		}
		if f == s.cmd && p0 == Posn(f.b.nc())-Posn(i) && i > 0 && str.s[i-1] == '\n' {
			s.termlocked++
			s.termcommand()
		}
		// terminal knows this already
		f.dot.r.p1 = p0 + Posn(i)
		f.dot.r.p2 = p0 + Posn(i)
		f.tdot = f.dot.r

	case Tcut:
		f := s.whichfile(s.inshort())
		p0 := s.inlong()
		p1 := s.inlong()
		s.logdelete(f, p0, p1)
		if s.fileupdate(f, false, false) {
			s.seq++
		}
		f.dot.r.p1 = p0
		f.dot.r.p2 = p0
		f.tdot = f.dot.r // terminal knows the value of dot already

	case Tpaste:
		f := s.whichfile(s.inshort())
		p0 := s.inlong()
		for l := Posn(0); l < Posn(s.snarfbuf.nc()); {
			m := s.snarfbuf.nc() - int(l)
			if m > BLOCKSIZE {
				m = BLOCKSIZE
			}
			bufread(&s.snarfbuf, l, s.genbuf[:], m)
			s.loginsert(f, p0, tmprstr(s.genbuf[:], m).s, m)
			l += Posn(m)
		}
		if s.fileupdate(f, false, true) {
			s.seq++
		}
		f.dot.r.p1 = p0
		f.dot.r.p2 = p0 + Posn(s.snarfbuf.nc())
		f.tdot.p1 = -1 // force telldot to tell (arguably a BUG)
		s.telldot(f)
		s.outTs(Hunlockfile, f.tag)

	case Tsnarf:
		i := s.inshort()
		p0 := s.inlong()
		p1 := s.inlong()
		s.snarf(s.whichfile(i), p0, p1, &s.snarfbuf, false)

	case Tstartnewfile:
		v := s.invlong()
		Strdupl(&s.genstr, empty)
		f := s.newfile()
		f.rasp = []Posn{}
		f.hasrasp = true
		s.outTsv(Hbindname, f.tag, v)
		s.logsetname(f, &s.genstr)
		s.outTs(Hcurrent, f.tag)
		s.current(f)
		s.load(f)

	case Twrite:
		s.termlocked++
		i := s.inshort()
		f := s.whichfile(i)
		s.addr.r.p1 = 0
		s.addr.r.p2 = Posn(f.b.nc())
		if f.name.s[0] == 0 {
			fail(Enoname)
		}
		Strduplstr(&s.genstr, &f.name)
		s.writef(f)

	case Tclose:
		s.termlocked++
		i := s.inshort()
		f := s.whichfile(i)
		s.current(f)
		s.trytoclose(f)
		// if trytoclose fails, will error out
		s.delete(f)

	case Tlook:
		f := s.whichfile(s.inshort())
		s.termlocked++
		p0 := s.inlong()
		p1 := s.inlong()
		s.setgenstr(f, p0, p1)
		for l := 0; l < s.genstr.n(); l++ {
			i := int(s.genstr.s[l])
			if runeIn(".*+?(|)\\[]^$", i) {
				str := tmpcstr("\\")
				Strinsert(&s.genstr, str, Posn(l))
				l++
				freetmpstr(str)
			}
		}
		Straddc(&s.genstr, 0)
		s.nextmatch(f, &s.genstr, p1, 1)
		s.moveto(f, s.sel.p[0])

	case Tsearch:
		s.termlocked++
		if s.curfile == nil {
			// untested: no plumber / no current file / always-NUL-terminated input
			fail(Enofile)
		}
		if s.lastpat.s[0] == 0 {
			// unreachable
			s.panicf("Tsearch")
		}
		s.nextmatch(s.curfile, &s.lastpat, s.curfile.dot.r.p2, 1)
		s.moveto(s.curfile, s.sel.p[0])

	case Tsend:
		s.termlocked++
		s.inshort() // ignored
		p0 := s.inlong()
		p1 := s.inlong()
		s.setgenstr(s.cmd, p0, p1)
		bufreset(&s.snarfbuf)
		bufinsert(&s.snarfbuf, 0, s.genstr.s, s.genstr.n())
		s.outTl(Hsnarflen, s.genstr.n())
		if s.genstr.s[s.genstr.n()-1] != '\n' {
			Straddc(&s.genstr, '\n')
		}
		s.loginsert(s.cmd, Posn(s.cmd.b.nc()), s.genstr.s, s.genstr.n())
		s.fileupdate(s.cmd, false, true)
		s.cmd.dot.r.p1 = Posn(s.cmd.b.nc())
		s.cmd.dot.r.p2 = Posn(s.cmd.b.nc())
		s.telldot(s.cmd)
		s.termcommand()

	case Tdclick:
		f := s.whichfile(s.inshort())
		p1 := s.inlong()
		s.doubleclick(f, p1)
		f.tdot.p1 = p1
		f.tdot.p2 = p1
		s.telldot(f)
		s.outTs(Hunlockfile, f.tag)

	case Tstartsnarf:
		if s.snarfbuf.nc() <= 0 { // nothing to export
			s.outTs(Hsetsnarf, 0)
			break
		}
		m := s.snarfbuf.nc()
		if m > SNARFSIZE {
			m = SNARFSIZE
			s.dprint("?warning: snarf buffer truncated\n")
		}
		rp := make([]Rune, m)
		bufread(&s.snarfbuf, 0, rp, m)
		c := Strtoc(tmprstr(rp, m))
		s.outTs(Hsetsnarf, len(c))
		s.out.Write([]byte(c))

	case Tsetsnarf:
		m := s.inshort()
		if m > SNARFSIZE {
			fail(Etoolong)
		}
		cb := make([]byte, m)
		for i := 0; i < m; i++ {
			cb[i] = byte(s.rcvchar())
		}
		str := tmpcstr(string(cb))
		bufreset(&s.snarfbuf)
		bufinsert(&s.snarfbuf, 0, str.s, str.n())
		freetmpstr(str)
		s.outT0(Hunlock)

	case Tack:
		s.waitack = false

	case Tplumb:
		// Build a plumb message for the click/selection [p0,p1) and send it to
		// the terminal as an Hplumb header followed by the packed bytes. (C
		// constructs a Plumbmsg and calls plumbpack; the wire format is the
		// libplumb one: src\ndst\nwdir\ntype\nattr\nndata\ndata.)
		f := s.whichfile(s.inshort())
		p0 := s.inlong()
		p1 := s.inlong()
		// wdir is the directory holding the file.
		name := Strtoc(&f.name)
		var wdir string
		if len(name) > 0 && name[0] == '/' {
			// untested: the test harness opens files by relative path (the temp
			// dir is dynamic), so f.name is never absolute here.
			wdir = name
		} else {
			cwd, _ := os.Getwd()
			wdir = cleanname(cwd + "/" + name)
		}
		if i := strings.LastIndexByte(wdir, '/'); i >= 0 {
			wdir = wdir[:i]
		}
		attr := ""
		if p1 <= p0 {
			// A click, not a sweep: expand to the surrounding white-space-
			// delimited word and record where in it the click fell.
			p := p0
			for p0 > 0 {
				if i := filereadc(f, p0-1); i == ' ' || i == '\t' || i == '\n' {
					break
				}
				p0--
			}
			for p1 < f.b.nc() {
				if i := filereadc(f, p1); i == ' ' || i == '\t' || i == '\n' {
					break
				}
				p1++
			}
			attr = fmt.Sprintf("click=%d", p-p0)
		}
		if p0 == p1 || p1-p0 >= BLOCKSIZE {
			break
		}
		s.setgenstr(f, p0, p1)
		data := Strtoc(&s.genstr)
		packed := "sam\n" + "\n" + wdir + "\n" + "text\n" + attr + "\n" +
			fmt.Sprintf("%d\n", len(data)) + data
		s.outTs(Hplumb, len(packed))
		s.out.Write([]byte(packed))

	case Texit:
		s.exits("0")
	}
	return 1
}
