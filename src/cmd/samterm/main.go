package main

import (
	"fmt"
	"io"
	"os"
	"os/signal"
	"strings"

	"9fans.net/plan9port/draw"
	"9fans.net/plan9port/internal/sam"
)

const chording = false // code here for reference but it causes deadlocks

func main() {
	// untested: the production entry point. The tests construct the terminal via
	// NewTerm (so they can run it in-process for coverage and wire the host to a
	// pipe), so this fd-and-os.Args wrapper is exercised only by the real binary.
	t := newTerm()
	// untested: see top of main
	if err := t.getscreen(os.Args[1:]); err != nil {
		// untested: see top of main
		fmt.Fprintf(os.Stderr, "samterm: initdraw: %v\n", err)
		os.Exit(1)
	}
	// sam talks to us on stdin/stdout. Move these elsewhere so that if we
	// accidentally use them in other code, nothing bad happens.
	// untested: see top of main
	t.hostr = os.Stdin
	t.hostw = os.Stdout
	if f, err := os.Open(os.DevNull); err == nil {
		// untested: see top of main
		os.Stdin = f
	}
	// untested: see top of main
	f, err := os.OpenFile("/dev/tty", os.O_WRONLY, 0)
	if err != nil {
		// untested: see top of main
		f, err = os.OpenFile(os.DevNull, os.O_WRONLY, 0)
	}
	// untested: see top of main
	if err == nil {
		// untested: see top of main
		os.Stdout = f
	}
	// C samterm's notifyf ignores the interrupt note and takes the default
	// action for any other. Installed here, not in NewTerm, so the tests'
	// in-process terminals don't swallow the test binary's interrupts.
	// untested: see top of main
	signal.Ignore(os.Interrupt)
	t.startup()
	t.run()
}

// newTerm builds a Term with the field defaults shared by main and NewTerm.
func newTerm() *Term {
	t := &Term{
		env:       os.Environ(),
		maxtab:    8,
		hostlock:  1,
		typestart: -1,
		typeend:   -1,
		typeesc:   -1,
		kpeekc:    -1,
		plumbfd:   -1,
		indata:    make([]byte, 0, DATASIZE),
	}
	t.menuinit()
	return t
}

// NewTerm builds a terminal on display disp, talking the sam protocol over
// hostr/hostw (fds 3/4 in the real program, a pipe in tests). The env is the
// terminal's environment (see Term.env); nil means os.Environ.
func NewTerm(disp *draw.Display, hostr io.Reader, hostw io.Writer, env []string) *Term {
	t := newTerm()
	if env != nil {
		t.env = env
	}
	t.display = disp
	t.screen = disp.ScreenImage
	t.font = disp.Font
	t.hostr = hostr
	t.hostw = hostw
	return t
}

//TODO: fix comments

// startup mirrors the second half of threadmain: create the command window,
// announce our version, and register the command file with the host.
func (t *Term) startup() {
	t.iconinit()
	t.initio()
	t.scratch = make([]rune, 100)
	r := t.screen.R
	r.Max.Y = r.Min.Y + r.Dy()/5
	t.flstart(t.screen.Clipr)
	t.cmd.rasp.init()
	t.flnew(&t.cmd.l[0], t.gettext, 1, &t.cmd)
	t.flinit(&t.cmd.l[0], r, t.font, t.cmdcols[:])
	t.cmd.nwin = 1
	t.which = &t.cmd.l[0]
	t.cmd.tag = Untagged
	t.outTs(sam.Tversion, VERSION)
	t.startnewfile(sam.Tstartcmdfile, &t.cmd)
}

// run is the event loop (threadmain's for(;;)). It returns when the host
// sends Hexit (the C version calls threadexitsall) or, via the recover below,
// when the host connection fails (see hostError).
func (t *Term) run() {
	defer func() {
		switch e := recover().(type) {
		case nil:
		case hostError:
			fmt.Fprintf(os.Stderr, "samterm: %s\n", e.msg)
		default:
			// untested: a non-hostError panic is a real bug; pass it on.
			panic(e)
		}
	}()
	for got := 0; ; got = t.waitforio() {
		if t.hasunlocked != 0 && t.RESIZED() {
			// untested: waitforio already handles a resize inline (and clears the
			// flag), so this top-of-loop re-check only fires for a resize that
			// arrived while blocked mid-update -- not reproducible deterministically.
			t.resize()
		}
		if got&(1<<RHost) != 0 {
			t.rcv()
		}
		// Note: t.exiting is set in inmesg (called from rcv).
		if t.exiting {
			return
		}
		if got&(1<<RPlumb) != 0 {
			// A plumbed file arrived: make the command window current, put the
			// cursor at the end, and type the formatted "B file\naddr\n" so sam
			// opens it (mirrors C main.c's RPlumb block).
			i := 0
			for t.cmd.l[i].textfn == nil {
				// untested: cmd.l[0] always holds the command window, so the
				// search never advances past it.
				i++
			}
			t.current(&t.cmd.l[i])
			t.flsetselect(t.which, t.cmd.rasp.nrunes, t.cmd.rasp.nrunes)
			t.typing(t.which, RPlumb)
		}
		if got&(1<<RKeyboard) != 0 {
			if t.which != nil {
				t.typing(t.which, RKeyboard)
			} else {
				// untested: a keystroke with no current window -- which is only
				// nil transiently while a window is being created/destroyed.
				t.kbdblock()
			}
		}
		if got&(1<<RMouse) != 0 {
			t.handlemouse()
		}
		if chording && t.chord != 0 {
			// untested: dead code -- chording is flagged off (see const chording).
			tx := t.which.user1
			// untested: see above
			if tx.lock == 0 && t.hostlock == 0 {
				// untested: see above
				w := t.windex(tx, t.which)
				switch {
				case t.chord&2 != 0:
					// untested: see above
					t.cut(tx, w, true, true)
					t.chord &^= 2
				case t.chord&4 != 0:
					// untested: see above
					t.paste(tx, w)
					t.chord &^= 4
				}
			}
		}
	}
}

func (t *Term) handlemouse() {
	if t.hostlock == 2 || !t.mouse.In(t.screen.R) {
		t.mouseunblock()
		return
	}
	nwhich := t.flwhich(t.mouse.Point)
	scr := t.which != nil && t.mouse.In(t.which.scroll)
	if t.mouse.Buttons != 0 {
		t.flushtyping(true)
	}
	if chording && t.chord == 1 && t.mouse.Buttons == 0 {
		// untested: dead code -- chording is flagged off (see const chording).
		t.chord = 0
	}
	switch {
	case chording && t.chord != 0:
		// untested: dead code -- chording is flagged off (see const chording).
		t.chord |= t.mouse.Buttons
	case t.mouse.Buttons&1 != 0:
		if nwhich != nil {
			if nwhich != t.which {
				t.current(nwhich)
			} else if scr {
				t.scroll(t.which, 1)
			} else {
				tx := t.which.user1
				if t.flselect(t.which) {
					t.outTsl(sam.Tdclick, tx.tag, t.which.p0)
					tx.lock++
				} else if tx != &t.cmd {
					t.outcmd()
				}
				if t.mouse.Buttons&1 != 0 {
					// untested: flselect's frame Select returns once button 1 is
					// released or another button joins it, so this runs only for a
					// chord press, which the deterministic scripts never send.
					t.chord = t.mouse.Buttons
				}
			}
		}
	case t.mouse.Buttons&2 != 0 && t.which != nil:
		if scr {
			t.scroll(t.which, 2)
		} else {
			t.menu2hit()
		}
	case t.mouse.Buttons&4 != 0:
		if scr {
			t.scroll(t.which, 3)
		} else {
			t.menu3hit()
		}
	}
	t.mouseunblock()
}

func (t *Term) resize() {
	t.flresize(t.screen.Clipr)
	for _, tx := range t.text {
		if tx != nil {
			t.hcheck(tx.tag)
		}
	}
}

func (t *Term) current(nw *Flayer) {
	if t.which != nil {
		t.flborder(t.which, false)
	}
	if nw != nil {
		t.flushtyping(true)
		t.flupfront(nw)
		t.flborder(nw, true)
		t.buttons(Up)
		tx := nw.user1
		tx.front = t.windex(tx, nw)
		if tx != &t.cmd {
			t.work = nw
		}
	}
	t.which = nw
}

func (t *Term) close(l *Flayer) {
	tx := l.user1
	m := t.whichmenu(tx.tag)
	if m < 0 {
		// untested: closing a window whose menu entry is already gone -- a
		// transient that the frame-synced harness does not reproduce.
		return
	}
	t.flclose(l)
	if l == t.which {
		t.which = nil
		t.current(t.flwhich(draw.Pt(0, 0)))
	}
	if l == t.work {
		t.work = nil
	}
	if tx.nwin--; tx.nwin == 0 {
		tx.rasp.clear()
		if tx.token != 0 {
			delete(t.texttoken, tx.token)
		}
		t.text[m] = nil
	} else if l == &tx.l[tx.front] {
		// untested: closing the exact front layer of a multi-window (zerox) file
		// so another must be promoted; which layer is front depends on focus
		// history that the blind mouse script cannot pin down.
		for i := range tx.l { // find one; any one will do
			// untested: see above
			if tx.l[i].textfn != nil {
				// untested: see above
				tx.front = i
				return
			}
		}
		// unreachable
		panic("close")
	}
}

func (t *Term) findl(tx *Text) *Flayer {
	for i := range tx.l {
		if tx.l[i].textfn == nil {
			return &tx.l[i]
		}
	}
	// untested: all NL layers of one file occupied (5 zerox/duplicate of the same
	// file) so no free slot remains.
	return nil
}

func (t *Term) duplicate(l *Flayer, r draw.Rectangle, f *draw.Font, closeit int) {
	tx := l.user1
	nl := t.findl(tx)
	if nl != nil {
		t.flnew(nl, t.gettext, l.user0, tx)
		t.flinit(nl, r, f, l.f.Cols[:])
		nl.origin = l.origin
		rp := l.textfn(l, l.f.NumChars)
		t.flinsert(nl, rp, l.origin)
		t.flsetselect(nl, l.p0, l.p1)
		if closeit != 0 {
			t.flclose(l)
			if l == t.which {
				t.which = nil
			}
		} else {
			tx.nwin++
		}
		t.current(nl)
		t.hcheck(tx.tag)
	}
	t.setcursor(t.cursor)
}

func (t *Term) buttons(updown int) {
	for (t.mouse.Buttons&7 != 0) == (updown == Up) {
		t.getmouse()
	}
}

func (t *Term) getr() (rp draw.Rectangle, ok bool) {
	rp = draw.SweepRect(3, t.mouse)
	if rp.Max.X != 0 && rp.Dx() <= 5 && rp.Dy() <= 5 {
		p := rp.Min
		r := t.cmd.l[t.cmd.front].entire
		rp = t.screen.R
		if t.cmd.nwin == 1 {
			if p.Y <= r.Min.Y {
				rp.Max.Y = r.Min.Y
			} else if p.Y >= r.Max.Y {
				rp.Min.Y = r.Max.Y
			}
			if p.X <= r.Min.X {
				// untested: the command window spans the full screen width, so no
				// on-screen tiny-sweep click is left or right of it (unlike the
				// vertical case, where it occupies only the top band).
				rp.Max.X = r.Min.X
			} else if p.X >= r.Max.X {
				// untested: see above
				rp.Min.X = r.Max.X
			}
		}
	}
	return rp, draw.RectClip(&rp, t.screen.R) && rp.Dx() > 100 && rp.Dy() > 40
}

func (t *Term) snarf(tx *Text, w int) {
	l := &tx.l[w]
	if l.p1 > l.p0 {
		t.snarflen = l.p1 - l.p0
		t.outTsll(sam.Tsnarf, tx.tag, l.p0, l.p1)
	}
}

func (t *Term) cut(tx *Text, w int, save, check bool) {
	l := &tx.l[w]
	p0 := l.p0
	p1 := l.p1
	if p0 == p1 {
		return
	}
	if p0 < 0 {
		// unreachable
		panic("cut")
	}
	if save {
		t.snarf(tx, w)
	}
	t.outTsll(sam.Tcut, tx.tag, p0, p1)
	t.flsetselect(l, p0, p0)
	tx.lock++
	t.hcut(tx.tag, p0, p1-p0)
	if check {
		t.hcheck(tx.tag)
	}
}

func (t *Term) paste(tx *Text, w int) {
	if t.snarflen != 0 {
		t.cut(tx, w, false, false)
		tx.lock++
		t.outTsl(sam.Tpaste, tx.tag, tx.l[w].p0)
	}
}

func (t *Term) scrorigin(l *Flayer, but, p0 int) {
	tx := l.user1
	switch but {
	case 1:
		t.outTsll(sam.Torigin, tx.tag, l.origin, p0)
	case 2:
		t.outTsll(sam.Torigin, tx.tag, p0, 1)
	case 3:
		t.horigin(tx.tag, p0)
	}
}

func alnum(c int) bool {
	// Hard to get absolutely right. Use what we know about ASCII
	// and assume anything above the Latin control characters is
	// potentially alphanumeric.
	if c <= ' ' {
		return false
	}
	if 0x7F <= c && c <= 0xA0 {
		// untested: classifying a DEL/high-control rune; the test content is plain
		// ASCII, so word-scanning never lands on one.
		return false
	}
	if strings.ContainsRune("!\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~", rune(c)) {
		// untested: classifying an ASCII punctuation rune happens only while
		// word-scanning (ctlw / word double-click) lands exactly on such a rune,
		// which the test content does not arrange at a scan boundary.
		return false
	}
	return true
}

func (t *Term) raspc(r *Rasp, p int) int {
	s := t.load(r, p, p+1)
	if len(s) > 0 {
		return int(s[0])
	}
	// untested: load returns empty only for a position past the rasp's filled
	// region; ctlw/ctlu only read positions they just stepped over, so this
	// past-end read does not occur through them.
	return 0
}

// ctlw/ctlu compute the word/line-erase target. Their boundary returns -- cursor
// exactly at the (scrolled) origin, or just past a newline with only whitespace
// before it -- depend on the precise rune at the cursor and the window's origin.
// Those rasp/cursor states are not reachable through the deterministic mouse+key
// script without visual cursor feedback, so the boundary returns are untested.
func (t *Term) ctlw(r *Rasp, o, p int) int {
	if p--; p < o {
		// untested: see ctlw doc
		return o
	}
	if t.raspc(r, p) == '\n' {
		// untested: see ctlw doc
		return p
	}
	for ; p >= o; p-- {
		c := t.raspc(r, p)
		if alnum(c) {
			break
		}
		if c == '\n' {
			// untested: see ctlw doc
			return p + 1
		}
	}
	for ; p > o && alnum(t.raspc(r, p-1)); p-- {
	}
	if p >= o {
		return p
	}
	// untested: see ctlw doc
	return o
}

func (t *Term) ctlu(r *Rasp, o, p int) int {
	if p--; p < o {
		// untested: see ctlw doc
		return o
	}
	if t.raspc(r, p) == '\n' {
		return p
	}
	for p-1 >= o && t.raspc(r, p-1) != '\n' {
		p--
	}
	if p >= o {
		return p
	}
	// unreachable: p < o cannot reach here in logic above.
	return o
}

func (t *Term) center(l *Flayer, a int) int {
	tx := l.user1
	if tx.lock == 0 && (a < l.origin || l.origin+l.f.NumChars < a) {
		if a > tx.rasp.nrunes {
			a = tx.rasp.nrunes
		}
		t.outTsll(sam.Torigin, tx.tag, a, 2)
		return 1
	}
	return 0
}

func (t *Term) thirds(l *Flayer, a, n int) int {
	tx := l.user1
	if tx.lock == 0 && (a < l.origin || l.origin+l.f.NumChars < a) {
		// untested: onethird is called after each typed insert with a = the new
		// cursor, but flinsert keeps that cursor on screen, so by here it is never
		// off-screen; the scroll body therefore does not run from typing. (center,
		// reached by the navigation keys, covers the equivalent off-screen scroll.)
		if a > tx.rasp.nrunes {
			// untested: see above
			a = tx.rasp.nrunes
		}
		// untested: see above
		s := l.scroll.Inset(1)
		lines := (n*(s.Max.Y-s.Min.Y)/l.f.Font.Height + 1) / 3
		if lines < 2 {
			// untested: see above
			lines = 2
		}
		// untested: see above
		t.outTsll(sam.Torigin, tx.tag, a, lines)
		return 1
	}
	return 0
}

func (t *Term) onethird(l *Flayer, a int) int {
	return t.thirds(l, a, 1)
}

// twothirds is unused -- only onethird is wired into typing -- but is kept for
// parity with C samterm, which also defines but never calls it.
func (t *Term) twothirds(l *Flayer, a int) int {
	// untested: see twothirds doc
	return t.thirds(l, a, 2)
}

func (t *Term) flushtyping(clearesc bool) {
	if clearesc {
		t.typeesc = -1
	}
	if t.typestart == t.typeend {
		t.modified = false
		return
	}
	tx := t.which.user1
	if tx != &t.cmd {
		t.modified = true
	}
	s := t.load(&tx.rasp, t.typestart, t.typeend)
	if tx == &t.cmd && t.typeend == tx.rasp.nrunes && len(s) > 0 && s[len(s)-1] == '\n' {
		t.setlock()
		t.outcmd()
	}
	t.outTslS(sam.Ttype, tx.tag, t.typestart, s)
	t.typestart = -1
	t.typeend = -1
}

const (
	BACKSCROLLKEY = draw.KeyUp
	ENDKEY        = draw.KeyEnd
	ESC           = 0x1B
	HOMEKEY       = draw.KeyHome
	LEFTARROW     = draw.KeyLeft
	LINEEND       = 0x05
	LINESTART     = 0x01
	PAGEDOWN      = draw.KeyPageDown
	PAGEUP        = draw.KeyPageUp
	RIGHTARROW    = draw.KeyRight
	SCROLLKEY     = draw.KeyDown
	CUT           = draw.KeyCmd + 'x'
	COPY          = draw.KeyCmd + 'c'
	PASTE         = draw.KeyCmd + 'v'
)

func nontypingkey(c int) bool {
	switch c {
	case BACKSCROLLKEY,
		ENDKEY,
		HOMEKEY,
		LEFTARROW,
		LINEEND,
		LINESTART,
		PAGEDOWN,
		PAGEUP,
		RIGHTARROW,
		SCROLLKEY,
		CUT,
		COPY,
		PASTE:
		return true
	}
	return false
}

func (t *Term) typing(l *Flayer, res int) { // what a bloody mess this is
	tx := l.user1
	buf := make([]rune, 0, 100)
	scrollkey := false
	if res == RKeyboard {
		scrollkey = nontypingkey(t.qpeekc()) // ICK
	}

	if t.hostlock != 0 || tx.lock != 0 {
		t.kbdblock()
		return
	}
	a := l.p0
	if a != l.p1 && !scrollkey {
		t.flushtyping(true)
		t.cut(tx, tx.front, true, true)
		return // it may now be locked
	}
	backspacing := false
	var c int
	for c = t.kbdchar(); c > 0; c = t.kbdchar() {
		if res == RKeyboard {
			if nontypingkey(c) || c == ESC {
				break
			}
			// backspace, ctrl-u, ctrl-w, del
			if c == '\b' || c == 0x15 || c == 0x17 || c == 0x7F {
				backspacing = true
				break
			}
		}
		buf = append(buf, rune(c))
		if t.autoindent && c == '\n' {
			// untested: autoindent is enabled only by the -a launch flag, which
			// the test harness does not pass to samterm.
			cursor := t.ctlu(&tx.rasp, 0, a+len(buf)-1)
			for len(buf) < cap(buf) {
				// untested: see above
				ch := t.raspc(&tx.rasp, cursor)
				cursor++
				if ch == ' ' || ch == '\t' {
					// untested: see above
					buf = append(buf, rune(ch))
				} else {
					// untested: see above
					break
				}
			}
		}
		if c == '\n' || len(buf) >= cap(buf) {
			break
		}
	}
	if len(buf) > 0 {
		if t.typestart < 0 {
			t.typestart = a
		}
		if t.typeesc < 0 {
			t.typeesc = a
		}
		t.hgrow(tx.tag, a, len(buf), 0)
		tx.lock++ // pretend we Trequest'ed for hdatarune
		t.hdatarune(tx.tag, a, buf)
		a += len(buf)
		l.p0 = a
		l.p1 = a
		t.typeend = a
		if c == '\n' || t.typeend-t.typestart > 100 {
			t.flushtyping(false)
		}
		t.onethird(l, a)
	}
	switch {
	case c == SCROLLKEY || c == PAGEDOWN:
		t.flushtyping(false)
		t.center(l, l.origin+l.f.NumChars+1)
	case c == BACKSCROLLKEY || c == PAGEUP:
		t.flushtyping(false)
		a0 := max(l.origin-l.f.NumChars, 0)
		t.center(l, a0)
	case c == RIGHTARROW:
		t.flushtyping(false)
		a0 := min(l.p0+1, tx.rasp.nrunes)
		t.flsetselect(l, a0, a0)
		t.center(l, a0)
	case c == LEFTARROW:
		t.flushtyping(false)
		a0 := max(l.p0-1, 0)
		t.flsetselect(l, a0, a0)
		t.center(l, a0)
	case c == HOMEKEY:
		t.flushtyping(false)
		t.center(l, 0)
	case c == ENDKEY:
		t.flushtyping(false)
		t.center(l, tx.rasp.nrunes)
	case c == LINESTART || c == LINEEND:
		t.flushtyping(true)
		if c == LINESTART {
			for a > 0 && t.raspc(&tx.rasp, a-1) != '\n' {
				a--
			}
		} else {
			for a < tx.rasp.nrunes && t.raspc(&tx.rasp, a) != '\n' {
				a++
			}
		}
		l.p0 = a
		l.p1 = a
		for i := 0; i < NL; i++ {
			if tx.l[i].textfn != nil {
				t.flsetselect(&tx.l[i], tx.l[i].p0, tx.l[i].p1)
			}
		}
	case backspacing && t.hostlock == 0:
		// backspacing immediately after outcmd(): sorry
		if l.f.P0 > 0 && a > 0 {
			switch c {
			case '\b', 0x7F: // del
				l.p0 = a - 1
			case 0x15: // ctrl-u
				l.p0 = t.ctlu(&tx.rasp, l.origin, a)
			case 0x17: // ctrl-w
				l.p0 = t.ctlw(&tx.rasp, l.origin, a)
			}
			l.p1 = a
			if l.p1 != l.p0 {
				// cut locally if possible
				if t.typestart <= l.p0 && l.p1 <= t.typeend {
					tx.lock++ // to call hcut
					t.hcut(tx.tag, l.p0, l.p1-l.p0)
					// hcheck is local because we know rasp is contiguous
					t.hcheck(tx.tag)
				} else {
					t.flushtyping(false)
					t.cut(tx, tx.front, false, true)
				}
			}
			if t.typeesc >= l.p0 {
				t.typeesc = l.p0
			}
			if t.typestart >= 0 {
				if t.typestart >= l.p0 {
					t.typestart = l.p0
				}
				t.typeend = l.p0
				if t.typestart == t.typeend {
					t.typestart = -1
					t.typeend = -1
					t.modified = false
				}
			}
		}
	default:
		if c == ESC && t.typeesc >= 0 {
			l.p0 = t.typeesc
			l.p1 = a
			t.flushtyping(true)
		}
		for i := range tx.l {
			if tx.l[i].textfn != nil {
				t.flsetselect(&tx.l[i], tx.l[i].p0, tx.l[i].p1)
			}
		}
		switch c {
		case CUT:
			t.flushtyping(false)
			t.cut(tx, tx.front, true, true)
		case COPY:
			t.flushtyping(false)
			t.snarf(tx, tx.front)
		case PASTE:
			t.flushtyping(false)
			t.paste(tx, tx.front)
		}
	}
}

func (t *Term) outcmd() {
	if t.work != nil {
		t.outTsll(sam.Tworkfile, t.work.user1.tag, t.work.p0, t.work.p1)
	}
}

// A hostError is panicked when the connection to the host (sam) fails: an
// unexpected EOF or read error from hostproc (raised in waitforio) or a write
// error in outsend. run's recover turns it into an error print and a clean
// return -- where C samterm exits the whole process (hostproc's threadexitsall,
// outsend's panic/abort), the in-process Go terminal must shut down only its
// own event loop, not the test binary hosting it.
type hostError struct{ msg string }

func (t *Term) gettext(l *Flayer, n int) []rune {
	return t.load(&l.user1.rasp, l.origin, l.origin+n)
}

func (t *Term) scrtotal(l *Flayer) int {
	return l.user1.rasp.nrunes
}
