package main

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"strconv"
	"strings"
	"time"

	"9fans.net/plan9port/draw"
)

// In-process -D driver: the Go counterpart of src/cmd/acme/debug.c. It reads a
// line script, mutates/prints acme's internal state, and writes results to
// a.stdout — matching the C oracle's output byte-for-byte. See HACKING.md.

// runD sets up an in-process acme against a real devdraw display (fixed size,
// like the C oracle's -D mode) and runs the script.
func (a *Acme) runD(script io.Reader, out, errw io.Writer) error {
	a.stdout = out
	a.stderr = errw
	a.reinit()
	// startup working directory (acme.c threadmain getwd; C's wdir global), used to
	// resolve unrooted Look paths -- the live editor sets it in threadmain.
	if wd, err := os.Getwd(); err == nil {
		a.wdir = wd
	}
	if err := a.initdraw("acme", "1024x768"); err != nil {
		// untested: display init failure
		return err
	}
	a.rowinit(&a.row, a.screen.Clipr)
	a.fsysinit()
	err := a.debugLoop(script)
	a.fsysclose()
	return err
}

// debugLoop reads -D command lines from script and drives the editor, flushing
// warnings and the display after each, until EOF or a clean Exit -- the read
// loop of debug.c's debugthread. runD calls it after its synchronous setup; the
// live editor's debugthread (debug.go) calls it against the threadmain-set-up
// editor, so the same verb dispatch (dcmd) serves both the frame-differential
// tests and the live `acme -D` binary.
func (a *Acme) debugLoop(script io.Reader) error {
	sc := bufio.NewScanner(script)
	sc.Buffer(make([]byte, 0, 1<<20), 1<<20)
	for sc.Scan() {
		line := sc.Text()
		if line == "" || line[0] == '#' {
			continue
		}
		a.dcmd(line)
		if a.exiting {
			// Exit on a clean row: C sends cexit and the process dies, so it
			// neither flushes nor processes further script lines. Match that by
			// unwinding immediately, leaving the prior command's frame as the last.
			break
		}
		// Drain any warning text into the +Errors window before pushing the
		// frame, as acme.c's mousethread flushes warnings each event.
		a.flushwarnings()
		// flush after every command, as the C debug driver does, so each
		// command's drawing is pushed to the display (and captured as a frame).
		a.disp.FlushImage(true)
	}
	return sc.Err()
}

func (a *Acme) dprintf(format string, args ...any) { fmt.Fprintf(a.stdout, format, args...) }

// dmsec is the synthetic mouse timestamp the m1 verb stamps on every event; a
// fixed value means a repeated in-place m1 (same q0==q1) reads as a double-click
// (mc.Msec-clickmsec < 500), matching the C oracle's m1.
const dmsec = 1000

// dmouse1 drives the real textselect with a scripted button-1 sweep: press at
// q0, drag to q1, release. It builds a synthetic Mousectl whose channel yields
// the move-then-release events frselect consumes (entry state = button down at
// q0; channel = drag to q1, then button up), so the whole live selection path
// runs deterministically. The C oracle's m1 (debug.c) does the same.
// dmouseargs parses "WHO Q0 Q1 [Qi...]" for m2/m3: the numeric tokens are drag
// waypoints (the first is the press point, the last the release point).
func (a *Acme) dmouseargs(rest string) (w *Window, t *Text, ways []int) {
	f := strings.Fields(rest)
	w, t = a.dtext(f[0])
	for _, tok := range f[1:] {
		q, _ := strconv.Atoi(tok)
		ways = append(ways, q)
	}
	return w, t, ways
}

// dmousesweep installs a synthetic Mousectl for a button-2/3 sweep: press the
// given button at ways[0], drag through the remaining waypoints, then release,
// and run the press through mousedispatch (the ported mousethread routing).
// Used by m2/m3. (Chord cut/cancel/argument is not driven through the verb; it
// crashes or diverges under -D, so textselect2's chord branches are unit-tested
// directly -- see TestTextselectChord.)
func (a *Acme) dmousesweep(t *Text, button int, ways []int) {
	pt := func(q int) draw.Point { return t.fr.PointOf(q - t.org) }
	ch := make(chan draw.Mouse, 16)
	mc := &draw.Mousectl{C: ch, Display: a.disp}
	mc.Mouse = draw.Mouse{Point: pt(ways[0]), Buttons: button, Msec: dmsec}
	last := pt(ways[len(ways)-1])
	for _, q := range ways[1:] {
		ch <- draw.Mouse{Point: pt(q), Buttons: button, Msec: dmsec}
	}
	ch <- draw.Mouse{Point: last, Buttons: 0, Msec: dmsec}
	a.mousectl = mc
	a.mousedispatch(mc.Mouse)
}

func atoiv(s string) int { n, _ := strconv.Atoi(s); return n }

// dragChord returns the optional 7th drag-verb token (a chord button held at
// release), or 0 for a clean release.
func dragChord(f []string) int {
	if len(f) > 6 {
		return atoiv(f[6])
	}
	return 0
}

// ddrag installs a synthetic Mousectl for a tag drag: button BUT (1/2/3, here as
// its bitmask 1<<(but-1)) pressed at from, dragged to, then released, and runs
// the press through mousedispatch, which hit-tests `from` and routes
// Tag->coldragwin, Columntag->rowdragcol. The drag loops read button-down at
// from on entry, then the move and the release from the channel. A non-zero
// chord button is held at release instead of a clean lift, so the loop exits
// with buttons still down (the abort path); a final clean release then drains
// it.
func (a *Acme) ddrag(from, to draw.Point, but, chord int) {
	mask := 1 << (but - 1)
	rel := mask
	if chord != 0 {
		rel = mask | 1<<(chord-1)
	}
	ch := make(chan draw.Mouse, 8)
	mc := &draw.Mousectl{C: ch, Display: a.disp}
	mc.Mouse = draw.Mouse{Point: from, Buttons: mask, Msec: dmsec}
	ch <- draw.Mouse{Point: to, Buttons: rel, Msec: dmsec} // drag/chord
	ch <- draw.Mouse{Point: to, Buttons: 0, Msec: dmsec}   // release
	a.mousectl = mc
	a.mousedispatch(mc.Mouse)
}

func (a *Acme) dmouse1(t *Text, q0, q1 int, chords []int) {
	p0 := t.fr.PointOf(q0 - t.org)
	p1 := t.fr.PointOf(q1 - t.org)
	ch := make(chan draw.Mouse, 16)
	mc := &draw.Mousectl{C: ch, Display: a.disp}
	mc.Mouse = draw.Mouse{Point: p0, Buttons: 1, Msec: dmsec}
	ch <- draw.Mouse{Point: p1, Buttons: 1, Msec: dmsec} // drag to q1
	for _, chord := range chords {
		ch <- draw.Mouse{Point: p1, Buttons: 1 | chord, Msec: dmsec} // chord button down
	}
	ch <- draw.Mouse{Point: p1, Buttons: 0, Msec: dmsec} // release all
	a.mousectl = mc
	a.mousedispatch(mc.Mouse)
}

// dscroll installs a synthetic Mousectl for a body-scrollbar press: button BUT
// pressed at ys[0] (x = scrollbar centre), dragged through ys[1:], then
// released, run through mousedispatch -> textscroll.
func (a *Acme) dscroll(t *Text, but int, ys []int) {
	x := (t.scrollr.Min.X + t.scrollr.Max.X) / 2
	mask := 1 << (but - 1)
	pt := func(y int) draw.Point { return draw.Pt(x, y) }
	ch := make(chan draw.Mouse, len(ys)+1)
	for _, y := range ys[1:] {
		ch <- draw.Mouse{Point: pt(y), Buttons: mask, Msec: dmsec} // drag
	}
	ch <- draw.Mouse{Point: pt(ys[len(ys)-1]), Buttons: 0, Msec: dmsec} // release
	mc := &draw.Mousectl{C: ch, Display: a.disp}
	mc.Mouse = draw.Mouse{Point: pt(ys[0]), Buttons: mask, Msec: dmsec}
	a.mousectl = mc
	a.mousedispatch(mc.Mouse)
}

// dwheel installs a synthetic Mousectl for a scroll-wheel notch (buttons 8 = up,
// 16 = down) at p, run through mousedispatch -> texttype. mousedispatch consumes
// only the notch; the trailing buttons=0 settle event (which C's next alt
// iteration would no-op on) is left unread, as the wheel handler does not loop.
func (a *Acme) dwheel(p draw.Point, n int) {
	ch := make(chan draw.Mouse, 4)
	mc := &draw.Mousectl{C: ch, Display: a.disp}
	mc.Mouse = draw.Mouse{Point: p, Buttons: n, Msec: dmsec}
	ch <- draw.Mouse{Point: p, Buttons: 0, Msec: dmsec} // settle
	a.mousectl = mc
	a.mousedispatch(mc.Mouse)
}

// dmouse1drag installs a synthetic Mousectl for a button-1 sweep that presses at
// char q0, drags through raw pixel waypoints pts (which may lie outside the
// frame, to drive frselect's auto-scroll past the top/bottom edge), then
// releases. Routed through mousedispatch like the other mouse verbs.
func (a *Acme) dmousedrag(t *Text, but, q0 int, pts []draw.Point) {
	mask := 1 << (but - 1)
	p0 := t.fr.PointOf(q0 - t.org)
	ch := make(chan draw.Mouse, len(pts)+2)
	for _, p := range pts {
		ch <- draw.Mouse{Point: p, Buttons: mask, Msec: dmsec} // drag
	}
	last := p0
	if len(pts) > 0 {
		last = pts[len(pts)-1]
	}
	ch <- draw.Mouse{Point: last, Buttons: 0, Msec: dmsec} // release
	mc := &draw.Mousectl{C: ch, Display: a.disp}
	mc.Mouse = draw.Mouse{Point: p0, Buttons: mask, Msec: dmsec}
	a.mousectl = mc
	a.mousedispatch(mc.Mouse)
}

// dwin looks up a window by its integer id, fatalf-ing if it doesn't exist.
func (a *Acme) dwin(s string) *Window {
	id, _ := strconv.Atoi(s)
	w := a.lookid(id)
	if w == nil {
		a.fatalf("no window %s", s)
	}
	return w
}

// dcol returns column i, fatalf-ing if out of range.
func (a *Acme) dcol(i int) *Column {
	if i < 0 || i >= a.row.ncol() {
		a.fatalf("no column %d", i)
	}
	return a.row.col[i]
}

// drect parses four "X0 Y0 X1 Y1" fields into a rectangle.
func drect(f []string) draw.Rectangle {
	n := make([]int, 4)
	for i := 0; i < 4; i++ {
		n[i], _ = strconv.Atoi(f[i])
	}
	return draw.Rect(n[0], n[1], n[2], n[3])
}

// dcommitTyping flushes a typing burst the way the real event loop does via
// wincommit: it commits the typed text's cache to the file, then clears the
// caches of any other texts on the same file (zerox copies), so none is left
// with a stale uncommitted cache.
func (a *Acme) dcommitTyping(t *Text) {
	a.textcommit(t, true)
	if len(t.file.text) > 1 {
		for _, u := range t.file.text {
			a.textcommit(u, false) // no-op for t; clears the others
		}
	}
}

// dEvalAddr evaluates address ar against t's dot (with optional limit lim and
// search direction) and prints the resulting range, like debug.c's addr cmds.
func (a *Acme) dEvalAddr(t *Text, ar []rune, lim Range, reverse bool) {
	getc := func(q int) int {
		if q < len(ar) {
			return int(ar[q])
		}
		return 0
	}
	eval := true
	var q int
	res := a.address(true, t, lim, Range{t.q0, t.q1}, getc, 0, len(ar), &eval, &q, reverse)
	ev := 0
	if eval {
		ev = 1
	}
	a.dprintf("addr %d %d eval=%d\n", res.q0, res.q1, ev)
}

// torunes converts a script string to runes, applying \t \n \\ escapes (debug.c).
func torunes(s string) []rune {
	var r []rune
	for i := 0; i < len(s); {
		if s[i] == '\\' && i+1 < len(s) {
			switch s[i+1] {
			case 't':
				r = append(r, '\t')
			case 'n':
				r = append(r, '\n')
			case 'b':
				r = append(r, '\b')
			case '\\':
				r = append(r, '\\')
			default:
				r = append(r, rune(s[i+1]))
			}
			i += 2
			continue
		}
		dr, w := decodeFirstRune(s[i:])
		r = append(r, dr)
		i += w
	}
	return r
}

func decodeFirstRune(s string) (rune, int) {
	for _, r := range s {
		return r, len(string(r))
	}
	// unreachable
	return 0, 0
}

// dwin returns the window/text named by "ID" (body) or "ID.tag" (tag).
func (a *Acme) dtext(s string) (*Window, *Text) {
	tag := false
	if i := strings.IndexByte(s, '.'); i >= 0 {
		tag = s[i+1:] == "tag"
		s = s[:i]
	}
	id, _ := strconv.Atoi(s)
	w := a.lookid(id)
	if w == nil {
		a.fatalf("no window %d", id)
	}
	if tag {
		return w, &w.tag
	}
	return w, &w.body
}

func (a *Acme) lookid(id int) *Window {
	for _, c := range a.row.col {
		for _, w := range c.w {
			if w.id == id {
				return w
			}
		}
	}
	return nil
}

func (a *Acme) fatalf(format string, args ...any) {
	panic(fmt.Sprintf(format, args...))
}

func (a *Acme) dcmd(line string) {
	cmd := line
	rest := ""
	if i := strings.IndexByte(line, ' '); i >= 0 {
		cmd = line[:i]
		rest = strings.TrimLeft(line[i+1:], " ")
	}
	switch cmd {
	case "newcol":
		c := a.rowadd(&a.row, nil, -1)
		if c == nil {
			a.fatalf("rowadd failed")
		}
		a.activecol = c
		a.dprintf("col %d\n", a.row.ncol()-1)
	case "col":
		i, _ := strconv.Atoi(rest)
		if i < 0 || i >= a.row.ncol() {
			a.fatalf("no column %d", i)
		}
		a.activecol = a.row.col[i]
	case "win":
		w := a.makenewwindow(nil)
		a.winsettag(w)
		a.dprintf("win %d\n", w.id)
	case "name":
		who := firstTok(&rest)
		_, _ = who, rest
		w, _ := a.dtext(who)
		a.winsetname(w, torunes(rest))
		a.winsettag(w)
	case "body", "tag":
		who := firstTok(&rest)
		id, _ := strconv.Atoi(who)
		w := a.lookid(id)
		if w == nil {
			a.fatalf("no window %d", id)
		}
		t := &w.body
		if cmd == "tag" {
			t = &w.tag
		}
		a.textinsert(t, t.file.b.nc(), torunes(rest), true)
	case "type":
		who := firstTok(&rest)
		w, t := a.dtext(who)
		for _, r := range torunes(rest) {
			a.wintype(w, t, r)
		}
		a.dcommitTyping(t)
	case "key":
		f := strings.Fields(rest)
		w, t := a.dtext(f[0])
		for _, name := range f[1:] {
			a.wintype(w, t, keysym(name))
		}
		a.dcommitTyping(t)
	case "dot":
		f := strings.Fields(rest)
		_, t := a.dtext(f[0])
		q0, _ := strconv.Atoi(f[1])
		q1, _ := strconv.Atoi(f[2])
		a.textsetselect(t, q0, q1)
	case "dclick":
		// double-click expand at POS; leave the result selected
		f := strings.Fields(rest)
		_, t := a.dtext(f[0])
		pos, _ := strconv.Atoi(f[1])
		q0, q1 := pos, pos
		a.textdoubleclick(t, &q0, &q1)
		a.textsetselect(t, q0, q1)
	case "addr", "addrrev":
		// evaluate the address against WHO's dot and print the resulting range
		who := firstTok(&rest)
		_, t := a.dtext(who)
		a.dEvalAddr(t, torunes(rest), Range{-1, -1}, cmd == "addrrev")
	case "addrlim":
		// addrlim WHO LO HI ADDR: forward address bounded by [LO,HI)
		who := firstTok(&rest)
		_, t := a.dtext(who)
		lo, _ := strconv.Atoi(firstTok(&rest))
		hi, _ := strconv.Atoi(firstTok(&rest))
		a.dEvalAddr(t, torunes(rest), Range{lo, hi}, false)
	case "b1":
		f := strings.Fields(rest)
		w, t := a.dtext(f[0])
		q0, _ := strconv.Atoi(f[1])
		q1, _ := strconv.Atoi(f[2])
		a.textsetselect(t, q0, q1)
		a.seltext = t
		a.argtext = t
		a.mousetext = t
		if t.col != nil {
			a.activecol = t.col
		}
		if t == &w.body {
			a.activewin = w
		}
	case "m1":
		// m1 WHO Q0 Q1 [cut|paste...]: button-1 sweep selecting [q0,q1) via the
		// real textselect; each trailing chord token presses button 2 (cut) or 3
		// (paste) while button 1 stays down (multiple tokens chord in sequence,
		// e.g. "cut paste" exercises the change-your-mind undo path).
		f := strings.Fields(rest)
		_, t := a.dtext(f[0])
		q0, _ := strconv.Atoi(f[1])
		q1, _ := strconv.Atoi(f[2])
		var chords []int
		for _, c := range f[3:] {
			switch c {
			case "cut":
				chords = append(chords, 2) // button 2: buttons 1|2 = 3
			case "paste":
				chords = append(chords, 4) // button 3: buttons 1|4 = 5
			}
		}
		a.dmouse1(t, q0, q1, chords)
	case "m1drag", "m2drag", "m3drag":
		// mNdrag WHO Q0 X1 Y1 [X2 Y2 ...]: press button N at char Q0, then drag
		// through raw pixel waypoints (which may be above/below the frame).
		// button 1 -> textselect (frselect auto-scrolls off the edge); button
		// 2/3 -> textselect2/3 sweep, which xselect clamps to the frame edge (no
		// scroll), then execute/look3 via mousedispatch.
		but := int(cmd[1] - '0')
		f := strings.Fields(rest)
		_, t := a.dtext(f[0])
		q0, _ := strconv.Atoi(f[1])
		var pts []draw.Point
		for i := 2; i+1 < len(f); i += 2 {
			pts = append(pts, draw.Pt(atoiv(f[i]), atoiv(f[i+1])))
		}
		a.dmousedrag(t, but, q0, pts)
	case "m2":
		// m2 WHO Q0 Q1 [Qi...]: button-2 sweep pressing at Q0, dragging through
		// Q1,Qi..., then (via mousedispatch) executing the swept range.
		_, t, ways := a.dmouseargs(rest)
		a.dmousesweep(t, 2, ways)
	case "m3":
		// m3 WHO Q0 Q1 [Qi...]: button-3 sweep, then (via mousedispatch) look3
		// the swept range.
		_, t, ways := a.dmouseargs(rest)
		a.dmousesweep(t, 4, ways)
	case "b2":
		f := strings.Fields(rest)
		w, t := a.dtext(f[0])
		q0, _ := strconv.Atoi(f[1])
		q1, _ := strconv.Atoi(f[2])
		a.winlock(w, 'D') // C debug.c b2: winlock(w,'D'); execute; winunlock(w)
		a.execute(t, q0, q1, false, nil)
		a.winunlock(w)
	case "b3", "b3r":
		// button 3: look/search/open [q0,q1) (look.go look3); b3r is reverse
		f := strings.Fields(rest)
		w, t := a.dtext(f[0])
		q0, _ := strconv.Atoi(f[1])
		q1, _ := strconv.Atoi(f[2])
		a.winlock(w, 'D') // C debug.c b3: winlock(w,'D'); look3; winunlock(w)
		a.look3(t, q0, q1, false, cmd == "b3r")
		a.winunlock(w)
	case "load":
		// load FILE's bytes into WHO's buffer (bufload/cvttorunes)
		who := firstTok(&rest)
		_, t := a.dtext(who)
		fd, err := os.Open(rest)
		if err != nil {
			a.fatalf("load: can't open %s", rest)
		}
		nulls := false
		n := t.file.b.Load(t.file.b.nc(), fd, &nulls)
		fd.Close()
		nn := 0
		if nulls {
			nn = 1
		}
		a.dprintf("load n=%d nulls=%d\n", n, nn)
	case "mtime":
		// mtime FILE SEC: set FILE's modification time, so get/put
		// mtime-recheck tests can control it deterministically.
		f := strings.Fields(rest)
		if len(f) != 2 {
			a.fatalf("mtime FILE SEC")
		}
		sec, _ := strconv.Atoi(f[1])
		tm := time.Unix(int64(sec), 0)
		if err := os.Chtimes(f[0], tm, tm); err != nil {
			a.fatalf("mtime: %v", err)
		}
	case "writefile":
		// writefile NAME SEC TEXT: (over)write NAME with the raw remainder TEXT
		// and set its mtime to SEC, so a get/put recheck test can change a file
		// on disk between the read and the put.
		name := firstTok(&rest)
		secStr := firstTok(&rest)
		if name == "" || secStr == "" {
			a.fatalf("writefile NAME SEC TEXT")
		}
		sec, _ := strconv.Atoi(secStr)
		if err := os.WriteFile(name, []byte(rest), 0o644); err != nil {
			a.fatalf("writefile: %v", err)
		}
		tm := time.Unix(int64(sec), 0)
		if err := os.Chtimes(name, tm, tm); err != nil {
			// untested: Chtimes of the file WriteFile just created won't fail
			a.fatalf("writefile: %v", err)
		}
	case "which":
		// which X Y: print the Text under the point (rowwhich), exercising the
		// hit-test dispatch.
		f := strings.Fields(rest)
		a.dwhich(draw.Pt(atoiv(f[0]), atoiv(f[1])))
	case "dragwin":
		// dragwin WHO BUT FX FY TX TY [CHORD]: press button BUT at (FX,FY),
		// drag to (TX,TY), release. mousedispatch hit-tests the press in a
		// window tag's scrollbar and routes to coldragwin (window drag/resize).
		// WHO is kept for script readability only.
		f := strings.Fields(rest)
		but, _ := strconv.Atoi(f[1])
		from := draw.Pt(atoiv(f[2]), atoiv(f[3]))
		to := draw.Pt(atoiv(f[4]), atoiv(f[5]))
		a.ddrag(from, to, but, dragChord(f))
	case "dragcol":
		// dragcol COL BUT FX FY TX TY [CHORD]: press button BUT at (FX,FY),
		// drag to (TX,TY), release. mousedispatch hit-tests the press in a
		// column tag's scrollbar and routes to rowdragcol. An optional CHORD
		// button is held at release to exercise the abort path. COL is kept for
		// script readability only.
		f := strings.Fields(rest)
		but, _ := strconv.Atoi(f[1])
		from := draw.Pt(atoiv(f[2]), atoiv(f[3]))
		to := draw.Pt(atoiv(f[4]), atoiv(f[5]))
		a.ddrag(from, to, but, dragChord(f))
	case "scroll":
		// scroll WHO BUT Y [Y...]: press button BUT in WHO's body scrollbar at
		// vertical position Y (x = scrollbar centre), drag through any further Y,
		// release -> textscroll via mousedispatch. but 1 = back, 2 = jump, 3 =
		// forward.
		f := strings.Fields(rest)
		_, t := a.dtext(f[0])
		but, _ := strconv.Atoi(f[1])
		var ys []int
		for _, s := range f[2:] {
			ys = append(ys, atoiv(s))
		}
		a.dscroll(t, but, ys)
	case "wheel":
		// wheel WHO N X Y: a scroll-wheel notch (N = 8 up, 16 down) at (X,Y) in
		// WHO -> mousedispatch maps it to a Kscrolloneup/down texttype.
		f := strings.Fields(rest)
		a.dtext(f[0]) // validate WHO exists
		n, _ := strconv.Atoi(f[1])
		a.dwheel(draw.Pt(atoiv(f[2]), atoiv(f[3])), n)
	case "catdump":
		// catdump PATH: print the dump file PATH to stdout, replacing the -D temp
		// root (the -T prefix) with "$ROOT" so the cwd/path lines are stable
		// across runs (each run uses a fresh temp dir).
		data, err := os.ReadFile(rest)
		if err != nil {
			// untested: catdump always follows a successful Dump of the file.
			a.fatalf("catdump: %v", err)
		}
		s := strings.ReplaceAll(string(data), string(a.trimpre), "$ROOT")
		fmt.Fprint(a.stdout, s)
	case "dumpto":
		// dumpto PATH: write the session dump to PATH (rowdump directly, no
		// command window to pollute the dump).
		a.rowdump(&a.row, rest)
	case "resize":
		// resize W H: simulate a window resize (acme.c mousethread MResize body).
		// Go's -D has no mousethread loop, so drive it directly: ask devdraw to
		// resize, reattach to the new screen image, then redraw and reflow
		// everything. The C oracle's test backend resizes quietly for this
		// client-initiated Tresize, so its mousethread does the same work here.
		f := strings.Fields(rest)
		a.disp.Resize(draw.Rect(0, 0, atoiv(f[0]), atoiv(f[1])))
		if err := a.disp.Attach(draw.RefNone); err != nil {
			// untested: reattach to the just-resized window does not fail.
			a.fatalf("resize: %v", err)
		}
		a.screen = a.disp.ScreenImage
		a.screen.Draw(a.screen.R, a.disp.White, nil, draw.ZP)
		a.iconinit()
		a.scrlresize()
		a.rowresize(&a.row, a.screen.Clipr)
	case "undo", "redo":
		// run the Undo/Redo builtin on WHO's window
		who := firstTok(&rest)
		w, _ := a.dtext(who)
		a.undo(&w.body, nil, nil, cmd == "undo", false, nil)
	case "b2arg":
		// button-2 with a 2-1 chord argument selected in another text
		f := strings.Fields(rest)
		w, t := a.dtext(f[0])
		q0, _ := strconv.Atoi(f[1])
		q1, _ := strconv.Atoi(f[2])
		_, at := a.dtext(f[3])
		aq0, _ := strconv.Atoi(f[4])
		aq1, _ := strconv.Atoi(f[5])
		a.textsetselect(at, aq0, aq1)
		a.winlock(w, 'D') // C debug.c b2arg: winlock(w,'D'); execute; winunlock(w)
		a.execute(t, q0, q1, false, at)
		a.winunlock(w)
	case "print":
		w, t := a.dtext(rest)
		if t.what == Body {
			a.printwin(w)
		} else {
			a.printtext(t, "tag")
		}
	case "colgrow":
		// grow window ID with mouse button BUT (cols.go colgrow)
		f := strings.Fields(rest)
		w := a.dwin(f[0])
		but, _ := strconv.Atoi(f[1])
		a.colgrow(w.col, w, but)
	case "colclose":
		w := a.dwin(rest)
		a.colclose(w.col, w, true)
	case "coladd":
		// coladd COLIDX Y: add a new window to column COLIDX near y
		f := strings.Fields(rest)
		ci, _ := strconv.Atoi(f[0])
		y, _ := strconv.Atoi(f[1])
		a.coladd(a.dcol(ci), nil, nil, y)
	case "colresize":
		// colresize COLIDX X0 Y0 X1 Y1
		f := strings.Fields(rest)
		ci, _ := strconv.Atoi(f[0])
		a.colresize(a.dcol(ci), drect(f[1:]))
	case "rowresize":
		// rowresize X0 Y0 X1 Y1
		a.rowresize(&a.row, drect(strings.Fields(rest)))
	case "rowadd":
		// rowadd X: add a new column whose left edge lands near x
		x, _ := strconv.Atoi(rest)
		a.rowadd(&a.row, nil, x)
	case "rects":
		a.dumpRects()
	case "dump":
		a.dumpRow()
	case "write":
		// write ID/file "quoted data": write an acme 9P file as a 9P client
		who := firstTok(&rest)
		data, dok := dunquote(rest)
		if !dok {
			a.fatalf("write arg must be a quoted string")
		}
		a.do9pWrite(who, []byte(data))
	case "read":
		// read ID/file: read an acme 9P file as a 9P client
		a.do9pRead(rest)
	case "stat":
		// stat ID/file: 9P stat of an acme file, printing the deterministic
		// fields (name, qid, perm, length) -- not the owner or times.
		a.do9pStat(rest)
	case "readdir":
		// readdir ID: 9P read of a directory, printing each entry's name.
		a.do9pReaddir(rest)
	case "cmderr":
		// cmderr DIR TEXT: mount DIR, attach a fid with its id, write TEXT to
		// that mount's /cons (routed to DIR/+Errors), then release it -- the
		// command-namespace path (fsysmount/attach/cons/clunk/delid).
		dir := firstTok(&rest)
		a.docmderr(dir, rest)
	case "wait":
		// wait N: block until the +Errors window's body holds >= N runes, i.e.
		// an async external command's output has been applied (debug.c wait). C
		// polls the window under row.lk while its xfid procs apply the cons
		// writes; here the output streams over a.cexec and we apply it on this,
		// the main goroutine, until +Errors is big enough.
		want := atoiv(rest)
		for a.errorsNc() < want && a.nexec > 0 {
			// drain the command lifecycle (ccommand/cwait, normally waitthread's)
			// and the cons output (ccons, normally consthread's) on this single
			// goroutine, deterministically, since -D runs no background threads.
			select {
			case c := <-a.ccommand:
				a.registerCommand(c)
			case ch := <-a.ccons:
				a.consToErrors(ch.data)
			case w := <-a.cwait:
				// untested: -D driver error path not exercised
				a.reapCommand(w)
			}
		}
	case "open":
		// open ID/file: open (and keep open) a 9P file for reading without
		// reading it, so a later action can queue events before the (otherwise
		// blocking) read -- used for the event file.
		if _, err := a.open9p(rest, 0 /*OREAD*/); err != nil {
			// untested: open only fails for bad names (not differentiable)
			a.dprintf("error %s\n", err)
		}
	default:
		a.fatalf("unknown command: %s", cmd)
	}
}

func keysym(name string) rune {
	switch name {
	case "bs":
		return Kbs
	case "^w":
		return Ketb // erase word
	case "^u":
		return Knack // erase line
	case "^a":
		return 0x01 // beginning of line
	case "^e":
		return 0x05 // end of line
	case "^f":
		return 0x06 // filename completion
	case "%c":
		return Kcmd + 'c' // copy
	case "%x":
		return Kcmd + 'x' // cut
	case "%v":
		return Kcmd + 'v' // paste
	case "%z":
		return Kcmd + 'z' // undo
	case "%Z":
		return Kcmd + 'Z' // redo
	case "del":
		return Kdel
	case "esc":
		return Kesc
	case "nl":
		return '\n'
	case "tab":
		return '\t'
	case "left":
		return Kleft
	case "right":
		return Kright
	case "up":
		return Kup
	case "down":
		return Kdown
	case "home":
		return Khome
	case "end":
		return Kend
	case "pgup":
		return Kpgup
	case "pgdn":
		return Kpgdown
	case "scrollup":
		return Kscrolloneup // mouse-wheel one line up
	case "scrolldown":
		return Kscrollonedown // mouse-wheel one line down
	}
	panic("unknown key " + name)
}

func firstTok(p *string) string {
	s := *p
	i := strings.IndexByte(s, ' ')
	if i < 0 {
		*p = ""
		return s
	}
	tok := s[:i]
	*p = strings.TrimLeft(s[i+1:], " ")
	return tok
}

func (a *Acme) printtext(t *Text, who string) {
	n := t.file.b.nc()
	r := make([]rune, n)
	t.file.b.Read(0, r)
	if t.what == Body {
		a.dprintf("win %d name=%s\n", t.w.id, string(t.file.name))
	}
	a.dprintf("  %s nc=%d dot=%d,%d\n", who, n, t.q0, t.q1)
	a.dprintf("  %s text=%s\n", who, string(r))
}

func (a *Acme) printwin(w *Window) {
	a.printtext(&w.tag, "tag")
	a.printtext(&w.body, "body")
}

func (a *Acme) dumpRects() {
	r := a.row.r
	a.dprintf("row ncol=%d r=%d,%d,%d,%d\n", a.row.ncol(), r.Min.X, r.Min.Y, r.Max.X, r.Max.Y)
	for i, c := range a.row.col {
		a.dprintf("col %d r=%d,%d,%d,%d\n", i, c.r.Min.X, c.r.Min.Y, c.r.Max.X, c.r.Max.Y)
		for _, w := range c.w {
			a.dprintf("  win %d r=%d,%d,%d,%d\n", w.id, w.r.Min.X, w.r.Min.Y, w.r.Max.X, w.r.Max.Y)
			tr := w.tag.fr.R
			a.dprintf("    tag r=%d,%d,%d,%d\n", tr.Min.X, tr.Min.Y, tr.Max.X, tr.Max.Y)
			br := w.body.fr.R
			a.dprintf("    body r=%d,%d,%d,%d\n", br.Min.X, br.Min.Y, br.Max.X, br.Max.Y)
			sr := w.body.lastsr
			a.dprintf("    scroll r=%d,%d,%d,%d\n", sr.Min.X, sr.Min.Y, sr.Max.X, sr.Max.Y)
		}
	}
}

func (a *Acme) dumpRow() {
	a.dprintf("row ncol=%d\n", a.row.ncol())
	for i, c := range a.row.col {
		a.dprintf("col %d nw=%d\n", i, c.nw())
		for _, w := range c.w {
			a.printwin(w)
		}
	}
}
