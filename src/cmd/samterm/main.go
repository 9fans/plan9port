package main

import (
	"fmt"
	"io"
	"os"
	"strings"

	"9fans.net/plan9port/draw"
)

//FIX: add const chording = false and port C chording code so we can fix it later

// keyboard constants (../../../include/keyboard.h).
//FIX: use draw constants instead of defining these (draw.KeyFn, etc)
const (
	KF      = 0xF000
	Khome   = KF | 0x0D
	Kup     = KF | 0x0E
	Kpgup   = KF | 0x0F
	Kleft   = KF | 0x11
	Kright  = KF | 0x12
	Kdown   = 0x80
	Kpgdown = KF | 0x13
	Kend    = KF | 0x18
	Kcmd    = 0xF100
)

//FIX: lost notifyf handling interrupts

func main() {
	//FIX: refactor to avoid duplication with NewTerm
	t := &Term{
		maxtab:    8,
		hostlock:  1,
		typestart: -1,
		typeend:   -1,
		typeesc:   -1,
		kpeekc:    -1,
		plumbfd:   -1,
	}
	if err := t.getscreen(os.Args[1:]); err != nil {
		fmt.Fprintf(os.Stderr, "samterm: initdraw: %v\n", err)
		os.Exit(1)
	}
	// sam talks to us on stdin/stdout.
	//FIX: reopen /dev/null onto os.Stdin
	// and /dev/tty or else /dev/null onto os.Stdout,
	// reassigning the globals
	t.hostr = os.Stdin
	t.hostw = os.Stdout
	t.menuinit()
	t.startup()
	t.run()
}

// NewTerm builds a terminal on display disp, talking the sam protocol over
// hostr/hostw (fds 3/4 in the real program, a pipe in tests).
func NewTerm(disp *draw.Display, hostr io.Reader, hostw io.Writer) *Term {
	t := &Term{
		disp:      disp,
		font:      disp.Font,
		maxtab:    8,
		hostlock:  1,
		typestart: -1,
		typeend:   -1,
		typeesc:   -1,
		kpeekc:    -1,
		plumbfd:   -1,
		hostr:     hostr,
		hostw:     hostw,
	}
	t.menuinit()
	return t
}

//TODO: fix comments

// startup mirrors the second half of threadmain: create the command window,
// announce our version, and register the command file with the host.
func (t *Term) startup() {
	//TODO: ignore interrupts?
	t.iconinit()
	t.initio()
	t.scratch = make([]rune, 100)
	r := t.disp.ScreenImage.R
	r.Max.Y = r.Min.Y + r.Dy()/5
	t.flstart(t.disp.ScreenImage.Clipr)
	t.cmd.rasp.init()
	t.flnew(&t.cmd.l[0], t.gettext, 1, &t.cmd)
	t.flinit(&t.cmd.l[0], r, t.font, t.cmdcols[:])
	t.cmd.nwin = 1
	t.which = &t.cmd.l[0]
	t.cmd.tag = Untagged
	t.outTs(Tversion, VERSION)
	t.startnewfile(Tstartcmdfile, &t.cmd)
}

// run is the event loop (threadmain's for(;;)). It returns when the host
// sends Hexit (the C version calls threadexitsall).
func (t *Term) run() {
	for got := 0; ; got = t.waitforio() {
		if t.hasunlocked != 0 && t.RESIZED() {
			t.resize()
		}
		if got&(1<<RHost) != 0 {
			t.rcv()
		}
		//FIX: where did this t.exiting come from?
		if t.exiting {
			return
		}
		//FIX: do plumbing
		// RPlumb is not handled (plumbing not yet ported).
		if got&(1<<RKeyboard) != 0 {
			if t.which != nil {
				t.typing(t.which, RKeyboard)
			} else {
				t.kbdblock()
			}
		}
		if got&(1<<RMouse) != 0 {
			//FIX: inline handlemouse here
			t.handlemouse()
		}
		//FIX: add chording code
	}
}

func (t *Term) handlemouse() {
	//FIX: why is t.mousep a function and not a pointer?
	if t.hostlock == 2 || !t.mousep().Point.In(t.disp.ScreenImage.R) {
		t.mouseunblock()
		return
	}
	nwhich := t.flwhich(t.mousep().Point)
	scr := t.which != nil && t.mousep().Point.In(t.which.scroll)
	if t.mousep().Buttons != 0 {
		t.flushtyping(1)
	}
	//FIX: add chording
	switch {
	case t.mousep().Buttons&1 != 0:
		if nwhich != nil {
			if nwhich != t.which {
				t.current(nwhich)
			} else if scr {
				t.scroll(t.which, 1)
			} else {
				tx := t.which.user1
				//FIX: change flselect to return bool?
				if t.flselect(t.which) != 0 {
					t.outTsl(Tdclick, tx.tag, t.which.p0)
					tx.lock++
				} else if tx != &t.cmd {
					t.outcmd()
				}
				//FIX: chord
			}
		}
	case t.mousep().Buttons&2 != 0 && t.which != nil:
		if scr {
			t.scroll(t.which, 2)
		} else {
			t.menu2hit()
		}
	case t.mousep().Buttons&4 != 0:
		if scr {
			t.scroll(t.which, 3)
		} else {
			t.menu3hit()
		}
	}
	t.mouseunblock()
}

func (t *Term) resize() {
	t.flresize(t.disp.ScreenImage.Clipr)
	//FIX: is nname always len(t.text) ?
	//FIX: at least use for _, tx := range t.text[:t.nname]
	for i := 0; i < t.nname; i++ {
		if t.text[i] != nil {
			t.hcheck(t.text[i].tag)
		}
	}
}

func (t *Term) current(nw *Flayer) {
	if t.which != nil {
		t.flborder(t.which, 0)
	}
	if nw != nil {
		t.flushtyping(1)
		t.flupfront(nw)
		t.flborder(nw, 1)
		t.buttons(Up)
		tx := nw.user1
		tx.front = t.windex(tx, nw)
		if tx != &t.cmd {
			t.work = nw
		}
	}
	t.which = nw
}

func (t *Term) closeup(l *Flayer) {
	tx := l.user1
	m := t.whichmenu(tx.tag)
	if m < 0 {
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
		//FIX: use range
		for i := 0; i < NL; i++ { // find one; any one will do
			if tx.l[i].textfn != nil {
				tx.front = i
				return
			}
		}
		panic("close")
	}
}

func (t *Term) findl(tx *Text) *Flayer {
	//FIX: use range
	for i := 0; i < NL; i++ {
		if tx.l[i].textfn == nil {
			return &tx.l[i]
		}
	}
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

//FIX: rename t.mousectl to t.mouse and then don't need t.mousep (t.mouse.Buttons etc).

// mousep returns the current mouse state.
func (t *Term) mousep() *draw.Mouse {
	return &t.mousectl.Mouse
}

func (t *Term) buttons(updown int) {
	//FIX: compare against updown == Up instead of using boolToInt.
	for boolToInt(t.mousep().Buttons&7 != 0) != updown {
		t.getmouse()
	}
}

func (t *Term) getr(rp *draw.Rectangle) bool {
	*rp = draw.SweepRect(3, t.mousectl)
	if rp.Max.X != 0 && rp.Dx() <= 5 && rp.Dy() <= 5 {
		p := rp.Min
		r := t.cmd.l[t.cmd.front].entire
		*rp = t.disp.ScreenImage.R
		if t.cmd.nwin == 1 {
			if p.Y <= r.Min.Y {
				rp.Max.Y = r.Min.Y
			} else if p.Y >= r.Max.Y {
				rp.Min.Y = r.Max.Y
			}
			if p.X <= r.Min.X {
				rp.Max.X = r.Min.X
			} else if p.X >= r.Max.X {
				rp.Min.X = r.Max.X
			}
		}
	}
	return draw.RectClip(rp, t.disp.ScreenImage.R) && rp.Dx() > 100 && rp.Dy() > 40
}

func (t *Term) snarf(tx *Text, w int) {
	l := &tx.l[w]
	if l.p1 > l.p0 {
		t.snarflen = l.p1 - l.p0
		t.outTsll(Tsnarf, tx.tag, l.p0, l.p1)
	}
}

//FIX: make save and check bools
func (t *Term) cut(tx *Text, w, save, check int) {
	l := &tx.l[w]
	p0 := l.p0
	p1 := l.p1
	if p0 == p1 {
		return
	}
	if p0 < 0 {
		panic("cut")
	}
	if save != 0 {
		t.snarf(tx, w)
	}
	t.outTsll(Tcut, tx.tag, p0, p1)
	t.flsetselect(l, p0, p0)
	tx.lock++
	t.hcut(tx.tag, p0, p1-p0)
	if check != 0 {
		t.hcheck(tx.tag)
	}
}

func (t *Term) paste(tx *Text, w int) {
	if t.snarflen != 0 {
		t.cut(tx, w, 0, 0)
		tx.lock++
		t.outTsl(Tpaste, tx.tag, tx.l[w].p0)
	}
}

func (t *Term) scrorigin(l *Flayer, but, p0 int) {
	tx := l.user1
	switch but {
	case 1:
		t.outTsll(Torigin, tx.tag, l.origin, p0)
	case 2:
		t.outTsll(Torigin, tx.tag, p0, 1)
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
		return false
	}
	if strings.ContainsRune("!\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~", rune(c)) {
		return false
	}
	return true
}

func (t *Term) raspc(r *Rasp, p int) int {
	s := t.rload(r, p, p+1)
	if len(s) > 0 {
		return int(s[0])
	}
	return 0
}

func (t *Term) ctlw(r *Rasp, o, p int) int {
	if p--; p < o {
		return o
	}
	if t.raspc(r, p) == '\n' {
		return p
	}
	for ; p >= o; p-- {
		c := t.raspc(r, p)
		if alnum(c) {
			break
		}
		if c == '\n' {
			return p + 1
		}
	}
	for ; p > o && alnum(t.raspc(r, p-1)); p-- {
	}
	if p >= o {
		return p
	}
	return o
}

func (t *Term) ctlu(r *Rasp, o, p int) int {
	if p--; p < o {
		return o
	}
	if t.raspc(r, p) == '\n' {
		return p
	}
	for p-1 >= o && t.raspc(r, p-1) != '\n' {
		p--
	}
	//FIX: can p < 0 happen?
	if p >= o {
		return p
	}
	return o
}

func (t *Term) center(l *Flayer, a int) int {
	tx := l.user1
	if tx.lock == 0 && (a < l.origin || l.origin+l.f.NumChars < a) {
		if a > tx.rasp.nrunes {
			a = tx.rasp.nrunes
		}
		t.outTsll(Torigin, tx.tag, a, 2)
		return 1
	}
	return 0
}

func (t *Term) thirds(l *Flayer, a, n int) int {
	tx := l.user1
	if tx.lock == 0 && (a < l.origin || l.origin+l.f.NumChars < a) {
		if a > tx.rasp.nrunes {
			a = tx.rasp.nrunes
		}
		s := l.scroll.Inset(1)
		lines := (n*(s.Max.Y-s.Min.Y)/l.f.Font.Height + 1) / 3
		if lines < 2 {
			lines = 2
		}
		t.outTsll(Torigin, tx.tag, a, lines)
		return 1
	}
	return 0
}

func (t *Term) onethird(l *Flayer, a int) int {
	return t.thirds(l, a, 1)
}

func (t *Term) twothirds(l *Flayer, a int) int {
	return t.thirds(l, a, 2)
}

//FIX: make clearesc bool
func (t *Term) flushtyping(clearesc int) {
	if clearesc != 0 {
		t.typeesc = -1
	}
	if t.typestart == t.typeend {
		t.modified = 0
		return
	}
	tx := t.which.user1
	if tx != &t.cmd {
		t.modified = 1
	}
	s := t.rload(&tx.rasp, t.typestart, t.typeend)
	if tx == &t.cmd && t.typeend == tx.rasp.nrunes && len(s) > 0 && s[len(s)-1] == '\n' {
		t.setlock()
		t.outcmd()
	}
	t.outTslS(Ttype, tx.tag, t.typestart, s)
	t.typestart = -1
	t.typeend = -1
}

const (
	BACKSCROLLKEY = Kup
	ENDKEY        = Kend
	ESC           = 0x1B
	HOMEKEY       = Khome
	LEFTARROW     = Kleft
	LINEEND       = 0x05
	LINESTART     = 0x01
	PAGEDOWN      = Kpgdown
	PAGEUP        = Kpgup
	RIGHTARROW    = Kright
	SCROLLKEY     = Kdown
	CUT           = Kcmd + 'x'
	COPY          = Kcmd + 'c'
	PASTE         = Kcmd + 'v'
)

func nontypingkey(c int) bool {
	switch c {
	//FIX: one per line
	case BACKSCROLLKEY, ENDKEY, HOMEKEY, LEFTARROW, LINEEND, LINESTART,
		PAGEDOWN, PAGEUP, RIGHTARROW, SCROLLKEY, CUT, COPY, PASTE:
		return true
	}
	return false
}

func (t *Term) typing(l *Flayer, res int) { // what a bloody mess this is
	tx := l.user1
	//FIX: use buf := make([]rune, 0, 100) and then np is len(buf) and len(buf) below is cap(buf)
	var buf [100]rune
	np := 0
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
		t.flushtyping(1)
		t.cut(tx, tx.front, 1, 1)
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
		buf[np] = rune(c)
		np++
		//FIX: make t.autoindent bool
		if t.autoindent != 0 && c == '\n' {
			cursor := t.ctlu(&tx.rasp, 0, a+np-1)
			for np < len(buf) {
				ch := t.raspc(&tx.rasp, cursor)
				cursor++
				if ch == ' ' || ch == '\t' {
					buf[np] = rune(ch)
					np++
				} else {
					break
				}
			}
		}
		if c == '\n' || np >= len(buf) {
			break
		}
	}
	if np > 0 {
		if t.typestart < 0 {
			t.typestart = a
		}
		if t.typeesc < 0 {
			t.typeesc = a
		}
		t.hgrow(tx.tag, a, np, 0)
		tx.lock++ // pretend we Trequest'ed for hdatarune
		t.hdatarune(tx.tag, a, buf[:np], np)
		a += np
		l.p0 = a
		l.p1 = a
		t.typeend = a
		if c == '\n' || t.typeend-t.typestart > 100 {
			t.flushtyping(0)
		}
		t.onethird(l, a)
	}
	switch {
	case c == SCROLLKEY || c == PAGEDOWN:
		t.flushtyping(0)
		t.center(l, l.origin+l.f.NumChars+1)
	case c == BACKSCROLLKEY || c == PAGEUP:
		t.flushtyping(0)
		//FIX: use max
		a0 := l.origin - l.f.NumChars
		if a0 < 0 {
			a0 = 0
		}
		t.center(l, a0)
	case c == RIGHTARROW:
		t.flushtyping(0)
		//FIX: use min
		a0 := l.p0
		if a0 < tx.rasp.nrunes {
			a0++
		}
		t.flsetselect(l, a0, a0)
		t.center(l, a0)
	case c == LEFTARROW:
		t.flushtyping(0)
		//FIX: use max
		a0 := l.p0
		if a0 > 0 {
			a0--
		}
		t.flsetselect(l, a0, a0)
		t.center(l, a0)
	case c == HOMEKEY:
		t.flushtyping(0)
		t.center(l, 0)
	case c == ENDKEY:
		t.flushtyping(0)
		t.center(l, tx.rasp.nrunes)
	case c == LINESTART || c == LINEEND:
		t.flushtyping(1)
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
					t.flushtyping(0)
					t.cut(tx, tx.front, 0, 1)
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
					//FIX: can t.modified be bool?
					t.modified = 0
				}
			}
		}
	default:
		if c == ESC && t.typeesc >= 0 {
			l.p0 = t.typeesc
			l.p1 = a
			t.flushtyping(1)
		}
		//FIX: use range
		for i := 0; i < NL; i++ {
			if tx.l[i].textfn != nil {
				t.flsetselect(&tx.l[i], tx.l[i].p0, tx.l[i].p1)
			}
		}
		switch c {
		case CUT:
			t.flushtyping(0)
			t.cut(tx, tx.front, 1, 1)
		case COPY:
			t.flushtyping(0)
			t.snarf(tx, tx.front)
		case PASTE:
			t.flushtyping(0)
			t.paste(tx, tx.front)
		}
	}
}

func (t *Term) outcmd() {
	if t.work != nil {
		t.outTsll(Tworkfile, t.work.user1.tag, t.work.p0, t.work.p1)
	}
}

//FIX: C defines panic here. okay to keep using go panic, but recover and turn into error print and exit for panics that are expected as normal, like the write error in outsend

func (t *Term) gettext(l *Flayer, n int) []rune {
	return t.rload(&l.user1.rasp, l.origin, l.origin+n)
}

func (t *Term) scrtotal(l *Flayer) int {
	return l.user1.rasp.nrunes
}

func (t *Term) exit() {
	//FIX: doesn't the caller already set exit? what does this do?
	t.exiting = true
}
