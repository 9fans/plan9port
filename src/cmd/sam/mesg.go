package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"os"
	"slices"
	"strings"
	"unicode/utf8"

	"9fans.net/plan9port/internal/sam"
	"9fans.net/plan9port/plumb"
)

// Sam terminal message protocol.
// Tmesg is terminal -> host.
// Hmesg is host -> terminal.
// Messages are framed: type byte, count low, count high,
// then count bytes of payload (little-endian integers within).

const (
	VERSION    = 2
	TBLOCKSIZE = 512
	DATASIZE   = utf8.UTFMax*TBLOCKSIZE + 30
	SNARFSIZE  = 32000
)

// Tmesg is a message type originating at the terminal (mesg.h enum Tmesg).
type Tmesg = sam.Tmesg

// Tmesg values (mesg.h enum Tmesg).
const (
	Tversion Tmesg = iota
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

// Hmesg is a message type originating at the host (mesg.h enum Hmesg).
type Hmesg = sam.Hmesg

// Hmesg values (mesg.h enum Hmesg).
const (
	Hversion Hmesg = iota
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

func (s *Sam) mesginit() {
	s.indata = make([]byte, DATASIZE+1)
	s.outdata = make([]byte, 2*DATASIZE+3)
	s.outmsg = 0
	s.outp = 0
}

// samname is the command window's file name.
var samname = []rune("~~sam~~")

// rcvchar reads one raw byte from the protocol input, buffering like mesg.c.
func (s *Sam) rcvchar() int {
	if s.rcvn <= 0 {
		n, _ := s.in.Read(s.rcvbuf[:])
		if n <= 0 {
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

// rcv reads one framed message (type byte, 2-byte count, count payload bytes)
// and dispatches it, returning inmesg's result, or 0 at EOF. rcvchar buffers and
// blocks for input; it returns -1 only once the input has closed, which is
// permanent, so unlike C's static state machine rcv need not resume a partial
// message on a later call.
func (s *Sam) rcv() int {
	typ := s.rcvchar()
	if typ == -1 {
		return 0
	}
	var hdr [2]byte
	for i := range hdr {
		c := s.rcvchar()
		if c == -1 {
			return 0
		}
		hdr[i] = byte(c)
	}
	count := int(binary.LittleEndian.Uint16(hdr[:]))
	if count > DATASIZE {
		s.panic("count>DATASIZE")
	}
	for i := 0; i < count; i++ {
		c := s.rcvchar()
		if c == -1 {
			return 0
		}
		s.indata[i] = byte(c)
	}
	s.indata[count] = 0
	return s.inmesg(Tmesg(typ))
}

func (s *Sam) whichfile(tag int) *File {
	for _, f := range s.file {
		if f.tag == tag {
			return f
		}
	}
	s.hiccough("") // C: hiccough((char*)0)
	return nil
}

func (s *Sam) inmesg(typ Tmesg) int {
	if typ > TMAX {
		s.panic("inmesg")
	}

	s.inp = 0
	switch typ {
	default:
		// unreachable
		s.panic("rcv unknown")

	case Tversion:
		s.tversion = s.inshort()

	case Tstartcmdfile:
		v := s.invlong() // for 64-bit pointers
		s.genstr = slices.Clone(samname)
		s.cmd = s.newfile()
		s.cmd.unread = false
		s.outTsv(Hbindname, s.cmd.tag, v)
		s.outTs(Hcurrent, s.cmd.tag)
		s.logsetname(s.cmd, s.genstr)
		s.cmd.rasp = Rasp{p: []uint32{}, init: true}
		s.cmd.mod = false
		if len(s.cmdstr) != 0 {
			s.loginsert(s.cmd, 0, s.cmdstr)
			s.cmdstr = slices.Delete(s.cmdstr, 0, int(Posn(len(s.cmdstr))))
		}
		s.fileupdate(s.cmd, false, true)
		s.outT0(Hunlock)

	case Tcheck:
		// go through whichfile to check the tag
		s.outTs(Hcheck, s.whichfile(s.inshort()).tag)

	case Trequest:
		f := s.whichfile(s.inshort())
		p0 := s.inposn()
		p1 := p0 + Posn(s.inshort())
		if f.unread {
			// unreachable
			s.panic("Trequest: unread")
		}
		if p1 > f.b.nc() {
			p1 = f.b.nc()
		}
		// can happen e.g. scrolling during command
		if p0 > f.b.nc() {
			p0 = f.b.nc()
		}
		var r Range
		var buf []rune
		if p0 == p1 {
			r.p1 = p0
			r.p2 = p0
		} else {
			r = s.rdata(&f.rasp, p0, p1-p0)
			buf = make([]rune, r.p2-r.p1)
			s.bufread(&f.b, r.p1, buf)
		}
		s.outTslS(Hdata, f.tag, r.p1, buf)

	case Torigin:
		sh := s.inshort()
		l := s.inposn()
		l1 := s.inposn()
		s.lookorigin(s.whichfile(sh), l, l1)

	case Tstartfile:
		s.termlocked++
		f := s.whichfile(s.inshort())
		if !f.rasp.init { // this might be a duplicate message
			f.rasp = Rasp{p: []uint32{}, init: true}
		}
		s.current(f)
		s.outTsv(Hbindname, f.tag, s.invlong())
		s.outTs(Hcurrent, f.tag)
		if f.unread {
			s.load(f)
		} else {
			if f.b.nc() > 0 {
				s.rgrow(&f.rasp, 0, f.b.nc())
				s.outTsll(Hgrow, f.tag, 0, f.b.nc())
			}
			s.outTs(Hcheck0, f.tag)
			s.moveto(f, f.dot.r)
		}

	case Tworkfile:
		i := s.inshort()
		f := s.whichfile(i)
		s.current(f)
		f.dot.r.p1 = s.inposn()
		f.dot.r.p2 = s.inposn()
		f.tdot = f.dot.r

	case Ttype:
		f := s.whichfile(s.inshort())
		p0 := s.inposn()
		str := []rune(s.indataString())
		i := Posn(len(str))
		s.loginsert(f, p0, str)
		if s.fileupdate(f, false, false) {
			s.seq++
		}
		if f == s.cmd && p0 == f.b.nc()-i && i > 0 && str[i-1] == '\n' {
			s.termlocked++
			s.termcommand()
		}
		// terminal knows this already
		f.dot.r.p1 = p0 + i
		f.dot.r.p2 = p0 + i
		f.tdot = f.dot.r

	case Tcut:
		f := s.whichfile(s.inshort())
		p0 := s.inposn()
		p1 := s.inposn()
		s.logdelete(f, p0, p1)
		if s.fileupdate(f, false, false) {
			s.seq++
		}
		f.dot.r.p1 = p0
		f.dot.r.p2 = p0
		f.tdot = f.dot.r // terminal knows the value of dot already

	case Tpaste:
		f := s.whichfile(s.inshort())
		p0 := s.inposn()
		for l := Posn(0); l < s.snarfbuf.nc(); {
			m := min(s.snarfbuf.nc()-l, BLOCKSIZE)
			s.bufread(&s.snarfbuf, l, s.genbuf[:m])
			s.loginsert(f, p0, s.genbuf[:m])
			l += m
		}
		if s.fileupdate(f, false, true) {
			s.seq++
		}
		f.dot.r.p1 = p0
		f.dot.r.p2 = p0 + s.snarfbuf.nc()
		f.tdot.p1 = -1 // force telldot to tell (arguably a BUG)
		s.telldot(f)
		s.outTs(Hunlockfile, f.tag)

	case Tsnarf:
		i := s.inshort()
		p0 := s.inposn()
		p1 := s.inposn()
		s.snarf(s.whichfile(i), p0, p1, &s.snarfbuf, false)

	case Tstartnewfile:
		v := s.invlong()
		s.genstr = slices.Clone(empty)
		f := s.newfile()
		f.rasp = Rasp{p: []uint32{}, init: true}
		s.outTsv(Hbindname, f.tag, v)
		s.logsetname(f, s.genstr)
		s.outTs(Hcurrent, f.tag)
		s.current(f)
		s.load(f)

	case Twrite:
		s.termlocked++
		i := s.inshort()
		f := s.whichfile(i)
		s.addr.r.p1 = 0
		s.addr.r.p2 = f.b.nc()
		if len(f.name) == 0 {
			s.error(Enoname)
		}
		s.genstr = slices.Clone(f.name)
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
		p0 := s.inposn()
		p1 := s.inposn()
		s.setgenstr(f, p0, p1)
		for l := Posn(0); l < Posn(len(s.genstr)); l++ {
			if strings.ContainsRune(".*+?(|)\\[]^$", s.genstr[l]) {
				s.genstr = s.insertString(s.genstr, []rune("\\"), l)
				l++
			}
		}
		s.nextmatch(f, s.genstr, p1, 1)
		s.moveto(f, s.sel.p[0])

	case Tsearch:
		s.termlocked++
		if s.curfile == nil {
			// untested: no plumber / no current file / always-NUL-terminated input
			s.error(Enofile)
		}
		if len(s.lastpat) == 0 {
			// unreachable
			s.panic("Tsearch")
		}
		s.nextmatch(s.curfile, s.lastpat, s.curfile.dot.r.p2, 1)
		s.moveto(s.curfile, s.sel.p[0])

	case Tsend:
		s.termlocked++
		s.inshort() // ignored
		p0 := s.inposn()
		p1 := s.inposn()
		s.setgenstr(s.cmd, p0, p1)
		bufreset(&s.snarfbuf)
		s.bufinsert(&s.snarfbuf, 0, s.genstr)
		s.outTl(Hsnarflen, Posn(len(s.genstr)))
		if s.genstr[Posn(len(s.genstr))-1] != '\n' {
			s.genstr = s.appendString(s.genstr, '\n')
		}
		s.loginsert(s.cmd, s.cmd.b.nc(), s.genstr)
		s.fileupdate(s.cmd, false, true)
		s.cmd.dot.r.p1 = s.cmd.b.nc()
		s.cmd.dot.r.p2 = s.cmd.b.nc()
		s.telldot(s.cmd)
		s.termcommand()

	case Tdclick:
		f := s.whichfile(s.inshort())
		p1 := s.inposn()
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
		rp := make([]rune, m)
		s.bufread(&s.snarfbuf, 0, rp)
		c := string(rp)
		s.outTs(Hsetsnarf, len(c))
		s.Write(s.out, []byte(c))

	case Tsetsnarf:
		m := s.inshort()
		if m > SNARFSIZE {
			s.error(Etoolong)
		}
		cb := make([]byte, m)
		for i := range m {
			cb[i] = byte(s.rcvchar())
		}
		bufreset(&s.snarfbuf)
		s.bufinsert(&s.snarfbuf, 0, []rune(string(cb)))
		s.outT0(Hunlock)

	case Tack:
		s.waitack = false

	case Tplumb:
		f := s.whichfile(s.inshort())
		p0 := s.inposn()
		p1 := s.inposn()
		// wdir is the directory holding the file.
		name := string(f.name)
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
		var attr *plumb.Attribute
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
			attr = &plumb.Attribute{Name: "click", Value: fmt.Sprintf("%d", p-p0)}
		}
		if p0 == p1 || p1-p0 >= BLOCKSIZE {
			break
		}
		s.setgenstr(f, p0, p1)
		m := &plumb.Message{
			Src:  "sam",
			Dir:  wdir,
			Type: "text",
			Attr: attr,
			Data: []byte(string(s.genstr)),
		}
		var buf bytes.Buffer
		m.Send(&buf)
		s.outTs(Hplumb, buf.Len())
		s.Write(s.out, buf.Bytes())

	case Texit:
		s.exits("0")
	}
	return 1
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

// snarf copies f[p1:p2] into buf (mesg.c snarf).
func (s *Sam) snarf(f *File, p1, p2 Posn, buf *Buffer, emptyok bool) {
	if !emptyok && p1 == p2 {
		return
	}
	bufreset(buf)
	// stage through genbuf to avoid compaction problems (vestigial).
	// Defensive clamp: callers pass an addressed range, so p2 should already
	// be within the file; C flags this as "can't happen", warns, then clamps.
	if p2 > f.b.nc() {
		fmt.Fprintf(s.err, "bad snarf addr p1=%d p2=%d f->b.nc=%d\n", p1, p2, f.b.nc())
		p2 = f.b.nc()
	}
	for l := p1; l < p2; {
		i := min(p2-l, BLOCKSIZE)
		s.bufread(&f.b, l, s.genbuf[:i])
		s.bufinsert(buf, buf.nc(), s.genbuf[:i])
		l += i
	}
}

func (s *Sam) inshort() int {
	n := binary.LittleEndian.Uint16(s.indata[s.inp:])
	s.inp += 2
	return int(n)
}

func (s *Sam) inposn() Posn {
	n := binary.LittleEndian.Uint32(s.indata[s.inp:])
	s.inp += 4
	return Posn(n)
}

func (s *Sam) invlong() int64 {
	v := binary.LittleEndian.Uint64(s.indata[s.inp:])
	s.inp += 8
	return int64(v)
}

func (s *Sam) setgenstr(f *File, p0, p1 Posn) {
	if p0 != p1 {
		if p1-p0 >= TBLOCKSIZE {
			s.error(Etoolong)
		}
		s.extendString(p1 - p0)
		s.bufread(&f.b, p0, s.genbuf[:p1-p0])
		s.genstr = append(s.genstr[:0], s.genbuf[:p1-p0]...)
	} else {
		if s.snarfbuf.nc() == 0 {
			s.error(Eempty)
		}
		if s.snarfbuf.nc() > TBLOCKSIZE {
			s.error(Etoolong)
		}
		s.bufread(&s.snarfbuf, 0, s.genbuf[:s.snarfbuf.nc()])
		s.genstr = append(s.genstr[:0], s.genbuf[:s.snarfbuf.nc()]...)
	}
}

func (s *Sam) outT0(typ Hmesg) {
	s.outstart(typ)
	s.outsend()
}

func (s *Sam) outTl(typ Hmesg, l Posn) {
	s.outstart(typ)
	s.outlong(int(l))
	s.outsend()
}

func (s *Sam) outTs(typ Hmesg, sh int) {
	s.outstart(typ)
	s.outshort(sh)
	s.outsend()
}

func (s *Sam) outS(str []rune) {
	s.outcopy([]byte(string(str)))
}

func (s *Sam) outTsS(typ Hmesg, s1 int, str []rune) {
	s.outstart(typ)
	s.outshort(s1)
	s.outS(str)
	s.outsend()
}

func (s *Sam) outTslS(typ Hmesg, s1 int, l1 Posn, str []rune) {
	s.outstart(typ)
	s.outshort(s1)
	s.outlong(int(l1))
	s.outS(str)
	s.outsend()
}

func (s *Sam) outTS(typ Hmesg, str []rune) {
	s.outstart(typ)
	s.outS(str)
	s.outsend()
}

func (s *Sam) outTsllS(typ Hmesg, s1 int, l1, l2 Posn, str []rune) {
	s.outstart(typ)
	s.outshort(s1)
	s.outlong(int(l1))
	s.outlong(int(l2))
	s.outS(str)
	s.outsend()
}

func (s *Sam) outTsll(typ Hmesg, sh int, l1, l2 Posn) {
	s.outstart(typ)
	s.outshort(sh)
	s.outlong(int(l1))
	s.outlong(int(l2))
	s.outsend()
}

func (s *Sam) outTsl(typ Hmesg, sh int, l Posn) {
	s.outstart(typ)
	s.outshort(sh)
	s.outlong(int(l))
	s.outsend()
}

func (s *Sam) outTsv(typ Hmesg, sh int, v int64) {
	s.outstart(typ)
	s.outshort(sh)
	s.outvlong(v)
	s.outsend()
}

func (s *Sam) outstart(typ Hmesg) {
	s.outdata[s.outmsg] = byte(typ)
	s.outp = s.outmsg + 3
}

func (s *Sam) outcopy(data []byte) {
	copy(s.outdata[s.outp:], data)
	s.outp += len(data)
}

func (s *Sam) outshort(v int) {
	binary.LittleEndian.PutUint16(s.outdata[s.outp:], uint16(v))
	s.outp += 2
}

func (s *Sam) outlong(l int) {
	binary.LittleEndian.PutUint32(s.outdata[s.outp:], uint32(l))
	s.outp += 4
}

func (s *Sam) outvlong(v int64) {
	binary.LittleEndian.PutUint64(s.outdata[s.outp:], uint64(v))
	s.outp += 8
}

func (s *Sam) outsend() {
	if s.outp >= len(s.outdata) {
		// unreachable
		s.panic("outsend")
	}
	outcount := s.outp - s.outmsg - 3
	binary.LittleEndian.PutUint16(s.outdata[s.outmsg+1:], uint16(outcount))
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
	for s.waitack {
		if s.rcv() == 0 {
			// untested
			s.rescue()
			s.exits("eof")
		}
	}
	s.outmsg = 0
	s.outbuffered = true
}
