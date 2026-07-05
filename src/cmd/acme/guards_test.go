package main

import (
	"fmt"
	"strings"
	"testing"

	"9fans.net/go/plan9"
	"9fans.net/go/plan9/srv9p"
	"9fans.net/plan9port/draw"
)

// TestFullrunewrite covers the 9P-write partial-rune carry (xfid.go
// fullrunewrite, port of xfid.c f->rpart): a multi-byte rune split across
// consecutive writes on a fid must be stitched back together, not corrupted.
func TestFullrunewrite(t *testing.T) {
	a := new(Acme)
	fid := new(srv9p.Fid)
	// '世' is e4 b8 96; split it after two bytes.
	if r := a.fullrunewrite(fid, []byte{0xe4, 0xb8}); len(r) != 0 {
		t.Errorf("incomplete first chunk yielded %q, want empty (carried)", string(r))
	}
	if r := a.fullrunewrite(fid, []byte{0x96}); string(r) != "世" {
		t.Errorf("stitched rune = %q, want 世", string(r))
	}
	// A write with complete text then a trailing partial: yields the text now,
	// carries the partial for next time.
	if r := a.fullrunewrite(fid, append([]byte("a"), 0xe4, 0xb8)); string(r) != "a" {
		t.Errorf("got %q, want a (partial carried)", string(r))
	}
	if r := a.fullrunewrite(fid, []byte{0x96}); string(r) != "世" {
		t.Errorf("got %q, want 世", string(r))
	}
	// NUL bytes are elided, like cvttorunes.
	if r := a.fullrunewrite(fid, []byte{'x', 0, 'y'}); string(r) != "xy" {
		t.Errorf("got %q, want xy (NUL elided)", string(r))
	}
	// A clunk-style reset (delete the fid) drops any pending partial.
	a.fullrunewrite(fid, []byte{0xe4})
	auxOf(fid).rpart = nil
	if r := a.fullrunewrite(fid, []byte("z")); string(r) != "z" {
		t.Errorf("got %q, want z (stale partial not carried after reset)", string(r))
	}
}

// These are the only non-script tests in the package. They cover paths that no
// testdata/debug script can reach: the -D driver's own malformed-input guards
// (the Go counterpart of debug.c's sysfatal, which would abort the C oracle
// rather than produce comparable output) and two address classifiers that
// belong to the not-yet-ported look3/expand path.

// TestDriverErrors exercises the in-process driver's malformed-script guards,
// each of which fatalf-panics (the Go counterpart of debug.c's sysfatal).
func TestDriverErrors(t *testing.T) {
	a := newTestAcme(t, "win\n") // one window (id 1) exists
	mustPanic := func(line string) {
		t.Helper()
		defer func() {
			if recover() == nil {
				t.Errorf("dcmd(%q) did not panic", line)
			}
		}()
		a.dcmd(line)
	}
	mustPanic("body 99 x")               // no window 99
	mustPanic("dot 99 0 1")              // dtext: no window 99
	mustPanic("col 5")                   // no column 5
	mustPanic("coladd 9 100")            // dcol: no column 9
	mustPanic("colgrow 99 1")            // dwin: no window 99
	mustPanic("boguscmd")                // unknown command
	mustPanic("key 1 bogus")             // unknown key name
	mustPanic("load 1 /no/such/file2")   // load: can't open
	mustPanic("mtime onlyone")           // mtime: needs FILE SEC
	mustPanic("mtime /no/such 1")        // mtime: chtimes of a missing file
	mustPanic("writefile f")             // writefile: needs NAME SEC TEXT
	mustPanic("writefile /no/dir/f 1 x") // writefile: create in a missing dir
	mustPanic("b2 1 0 99")               // execute reads past the buffer -> bug()
	// 9P write-argument parsing guards (dunquote, before any 9P request).
	mustPanic(`write 1/addr nope`)   // arg not a quoted string
	mustPanic(`write 1/addr "\q"`)   // bad escape in the quoted arg
	mustPanic(`write 1/addr "\x"`)   // truncated \x escape
	mustPanic(`write 1/addr "\xZZ"`) // invalid hex digits
	mustPanic(`write 1/addr "\`)     // trailing backslash
}

// TestPlan9Quote covers the ctl file's font-name quoting (winctlprint). The
// loaded test font's name needs no quoting, so the quoted forms are unreachable
// through a -D scenario and are checked directly here.
func TestPlan9Quote(t *testing.T) {
	cases := []struct{ in, want string }{
		{"", "''"},
		{"/lib/font/x.font", "/lib/font/x.font"},
		{"two words", "'two words'"},
		{"a'b", "'a''b'"},
	}
	for _, c := range cases {
		if got := plan9quote(c.in); got != c.want {
			t.Errorf("plan9quote(%q)=%q want %q", c.in, got, c.want)
		}
	}
}

// TestAddrClassifiers covers isaddrc/isregexc and nlcounttopos, helpers that
// belong to the look3/expand path (not yet ported) and so are not yet reachable
// through a -D command.
func TestAddrClassifiers(t *testing.T) {
	for _, r := range "0123456789+-/$.#,;?" {
		if !isaddrc(r) {
			t.Errorf("isaddrc(%q) false", r)
		}
	}
	if isaddrc('z') || isaddrc(0) {
		t.Error("isaddrc accepted a non-address rune")
	}
	for _, r := range "az5^+-.*?#,;[]()$" {
		if !isregexc(r) {
			t.Errorf("isregexc(%q) false", r)
		}
	}
	if isregexc(' ') || isregexc(0) {
		t.Error("isregexc accepted space/NUL")
	}

	a := new(Acme)
	f := new(File)
	f.b.Insert(0, []rune("ab\ncd\nef")) // nc=8, 2 newlines
	tx := &Text{file: f}
	if got := a.nlcounttopos(tx, 0, 1, 1); got != 4 {
		t.Errorf("nlcounttopos(0,1,1)=%d want 4", got)
	}
	if got := a.nlcounttopos(tx, 0, 10, 0); got != 8 { // nl past end
		t.Errorf("nlcounttopos(0,10,0)=%d want 8", got)
	}
	if got := a.nlcounttopos(tx, 0, 0, 99); got != 2 { // nr stops at first newline
		t.Errorf("nlcounttopos(0,0,99)=%d want 2", got)
	}
}

// TestFsysDenied covers the create/remove/wstat 9P callbacks (fsys.c
// fsyscreate/fsysremove/fsyswstat), which all deny with C's "permission denied".
// They return a constant error with no window state, so they are exercised
// directly through the in-process 9P client rather than a -D script (the C
// 9pclient has no wstat, so a differential verb would be lopsided anyway).
func TestFsysDenied(t *testing.T) {
	a := new(Acme)
	a.fsysinit()
	defer a.fsysclose()
	const want = "permission denied"
	if _, err := a.fsys.Create("foo", plan9.OREAD, 0o666); err == nil || err.Error() != want {
		t.Errorf("Create err = %v, want %q", err, want)
	}
	if err := a.fsys.Remove("index"); err == nil || err.Error() != want {
		t.Errorf("Remove err = %v, want %q", err, want)
	}
	var d plan9.Dir
	d.Null()      // no-change sentinels...
	d.Mode = 0644 // ...except a mode change, so the server's Wstat is reached
	if err := a.fsys.Wstat("index", &d); err == nil || err.Error() != want {
		t.Errorf("Wstat err = %v, want %q", err, want)
	}
}

// TestTimers covers the timer subsystem (time.go, port of time.c): arming,
// firing after the duration elapses, cancellation, and free-list reuse. Time is
// injected via timertick(dt) -- the body of C's timerproc loop -- so the test is
// deterministic with no wall-clock sleeps (this is how the timers are testable
// at all: the elapsed-ms input is decoupled from the firing logic).
func TestTimers(t *testing.T) {
	a := new(Acme)
	fired := func(tm *Timer) bool {
		select {
		case <-tm.c:
			return true
		default:
			return false
		}
	}

	t1 := a.timerstart(50)
	t2 := a.timerstart(100)
	t3 := a.timerstart(100)
	a.timercancel(t3)

	a.timertick(60) // t1 (50) fires; t2 has 40 left; t3 is cancelled (freed)
	if !fired(t1) {
		t.Error("t1 did not fire after its duration elapsed")
	}
	// Faithful to time.c: timertick only sends on a fired timer's channel and
	// drops it from the active list; the CONSUMER frees it via timerstop (as
	// scrsleep/keyboardthread do on receiving). Freeing it inside timertick too
	// would double-free once the consumer also stops it.
	a.timerstop(t1)
	if fired(t2) {
		t.Error("t2 fired early")
	}
	if fired(t3) {
		t.Error("cancelled t3 fired")
	}

	a.timertick(40) // t2 (now 0) fires
	if !fired(t2) {
		t.Error("t2 did not fire")
	}
	a.timerstop(t2)

	// Free-list reuse: a fired timer stopped with its fire still in the buffer
	// must have that stale fire drained when recycled, or the next buffered(1)
	// send would block. t5 reuses t4 (the same object).
	t4 := a.timerstart(10)
	a.timertick(10)        // t4 fires, left UNCONSUMED in the buffer
	a.timerstop(t4)        // stopped without draining -> stale fire on free list
	t5 := a.timerstart(10) // reuses t4, must drain the stale fire
	a.timertick(10)        // t5 fires exactly once
	if !fired(t5) {
		t.Error("reused timer did not fire (stale buffer not drained?)")
	}
	if fired(t5) {
		t.Error("reused timer had two fires queued (stale not drained)")
	}
	a.timerstop(t5)
	// A further tick with no active timers is a no-op.
	a.timertick(10)
}

// TestMousescrollsize covers the $mousescrollsize parsing in mousescrollsize
// (text.go), whose env-driven branches the -D scroll-wheel test cannot reach
// ($mousescrollsize is unset there). libdraw scroll.c is the reference.
func TestMousescrollsize(t *testing.T) {
	for _, tt := range []struct {
		env  string
		want int
	}{
		{"", 1},       // unset: one line
		{"3", 3},      // explicit line count
		{"0", 1},      // zero line count falls back to one
		{"bad", 1},    // unparsable falls back to one
		{"50%", 50},   // percentage of maxlines (100)
		{"150%", 100}, // clamped to 100%
		{"0%", 0},     // zero percent
	} {
		t.Setenv("mousescrollsize", tt.env)
		if got := mousescrollsize(100); got != tt.want {
			t.Errorf("mousescrollsize(100) with $mousescrollsize=%q = %d, want %d", tt.env, got, tt.want)
		}
	}
}

// TestTextframescroll covers framescroll/textframescroll (text.go), the
// frselect auto-scroll-while-selecting callback. The discrete -D mouse can't
// drive frselect's scroll loop (it busy-scrolls without reading events), so the
// callback is exercised directly here, like the other layout unit tests.
func TestTextframescroll(t *testing.T) {
	var b strings.Builder
	b.WriteString("win\nbody 1 ")
	for i := 0; i < 100; i++ {
		if i > 0 {
			b.WriteString("\\n")
		}
		fmt.Fprintf(&b, "L%02d", i)
	}
	a := newTestAcme(t, b.String())
	tb := &a.row.col[0].w[0].body
	a.selecttext = tb
	a.selectq = 0
	// textframescroll(0) now throttles via scrsleep, which drains a.mousectl;
	// give it an empty channel so the dl==0 call returns at once (timer branch).
	a.mousectl = &draw.Mousectl{C: make(chan draw.Mouse, 1)}

	org0 := tb.org
	a.textframescroll(tb, 3) // dl>0: scroll down
	if tb.org <= org0 {
		t.Errorf("scroll down: org %d not advanced from %d", tb.org, org0)
	}
	a.selectq = tb.org
	org1 := tb.org
	a.textframescroll(tb, -1) // dl<0: scroll up
	if tb.org >= org1 {
		t.Errorf("scroll up: org %d not retreated from %d", tb.org, org1)
	}
	org2 := tb.org
	a.textframescroll(tb, 0) // throttle no-op
	if tb.org != org2 {
		t.Errorf("dl==0 changed org from %d to %d", org2, tb.org)
	}
	a.framescroll(&tb.fr, 2) // dispatches to textframescroll
	// anchor past the visible selection edge: exercises the other extend branch
	// in each direction (selectq > org+P0 for up, selectq > org+P1 for down).
	a.selectq = tb.file.b.nc()
	a.textframescroll(tb, -1)
	a.textframescroll(tb, 3)
	// scroll to EOF, then a further down-scroll must early-return
	for i := 0; i < 200 && tb.org+tb.fr.NumChars != tb.file.b.nc(); i++ {
		a.textframescroll(tb, 5)
	}
	eof := tb.org
	a.textframescroll(tb, 1) // at EOF: early return, org unchanged
	if tb.org != eof {
		t.Errorf("EOF down-scroll moved org from %d to %d", eof, tb.org)
	}
	if absInt(-3) != 3 || absInt(3) != 3 {
		t.Errorf("absInt wrong")
	}
}

// TestTextselectChord covers textselect2's chord branches (cancel by button 3,
// and the 2-1 argument pick-up) plus the textselect23 button-drain read loop.
// These can't be driven through the m2 -D verb: a chord that runs execute/del
// under the verb's winlock crashes the C oracle, and a bare cancel leaves a
// sweep-restore frame divergence with nothing to repaint over it. So the
// gestures are fed straight to the real textselect2 through a synthetic
// Mousectl, exactly as dmousesweep does, and only the return values are checked
// (no frame compare).
func TestTextselectChord(t *testing.T) {
	a := newTestAcme(t, "win\nbody 1 hello world\n")
	tb := &a.row.col[0].w[0].body

	// build a synthetic Mousectl that presses button 2 at q0, drags to q1, then
	// changes buttons to chord (breaking xselect), then releases (draining the
	// textselect23 read loop).
	sweep := func(q0, q1, chord int) {
		pt := func(q int) draw.Point { return tb.fr.PointOf(q - tb.org) }
		ch := make(chan draw.Mouse, 16)
		mc := &draw.Mousectl{C: ch, Display: a.disp}
		mc.Mouse = draw.Mouse{Point: pt(q0), Buttons: 2, Msec: dmsec}
		ch <- draw.Mouse{Point: pt(q1), Buttons: 2, Msec: dmsec}     // drag
		ch <- draw.Mouse{Point: pt(q1), Buttons: chord, Msec: dmsec} // chord down -> breaks xselect
		ch <- draw.Mouse{Point: pt(q1), Buttons: 0, Msec: dmsec}     // release -> drains read loop
		a.mousectl = mc
	}

	// cancel: button 3 added (2|4=6) -> textselect2 reports ok=false.
	sweep(0, 5, 6)
	if q0, q1, tp, ok := a.textselect2(tb); ok || tp != nil {
		t.Errorf("cancel: textselect2 = (%d,%d,%v,%v), want ok=false tp=nil", q0, q1, tp, ok)
	}

	// argument: button 1 added (2|1=3) -> picks up a.argtext, ok=true.
	a.argtext = tb
	sweep(0, 5, 3)
	q0, q1, tp, ok := a.textselect2(tb)
	if !ok || tp != tb {
		t.Errorf("arg: textselect2 = (%d,%d,%v,%v), want ok=true tp=argtext", q0, q1, tp, ok)
	}
	if q0 != 0 || q1 != 5 {
		t.Errorf("arg: range = (%d,%d), want (0,5)", q0, q1)
	}

	// null-DELAY collapse: a fast release (msec unchanged) landing within
	// MINMOVE px of the press point cancels a selection the drag had grown
	// (p0!=p1). The m2/m3 -D verb can't reach this -- its release point is
	// always the last drag waypoint, so a release back at the start also resets
	// p1 to p0 before the check. Here the raw channel keeps the last drag at
	// char 5 (p1=5) but releases at char 0, so the block fires and collapses the
	// range back to empty.
	pt := func(q int) draw.Point { return tb.fr.PointOf(q - tb.org) }
	ch := make(chan draw.Mouse, 16)
	mc := &draw.Mousectl{C: ch, Display: a.disp}
	mc.Mouse = draw.Mouse{Point: pt(0), Buttons: 2, Msec: dmsec}
	ch <- draw.Mouse{Point: pt(5), Buttons: 2, Msec: dmsec} // drag grows p1 to 5
	ch <- draw.Mouse{Point: pt(0), Buttons: 0, Msec: dmsec} // fast release at start
	a.mousectl = mc
	if rp0, rp1 := a.xselect(&tb.fr, mc, a.but2col); rp0 != rp1 {
		t.Errorf("null-DELAY fwd: xselect = (%d,%d), want empty range", rp0, rp1)
	}

	// same collapse but with a backward drag (reg<0): press char 5, drag to
	// char 0, release back at char 5.
	ch = make(chan draw.Mouse, 16)
	mc = &draw.Mousectl{C: ch, Display: a.disp}
	mc.Mouse = draw.Mouse{Point: pt(5), Buttons: 2, Msec: dmsec}
	ch <- draw.Mouse{Point: pt(0), Buttons: 2, Msec: dmsec} // drag shrinks p1 to 0
	ch <- draw.Mouse{Point: pt(5), Buttons: 0, Msec: dmsec} // fast release at start
	a.mousectl = mc
	if rp0, rp1 := a.xselect(&tb.fr, mc, a.but2col); rp0 != rp1 {
		t.Errorf("null-DELAY bwd: xselect = (%d,%d), want empty range", rp0, rp1)
	}
}
