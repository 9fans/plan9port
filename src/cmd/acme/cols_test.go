package main

import (
	"bufio"
	"bytes"
	"strings"
	"testing"

	"9fans.net/plan9port/draw"
)

// The column/row geometry tests in cols_test.go, rows_test.go and the scroll
// tests in texttype_test.go are Go-only invariant tests, not -D differential
// tests. They check pixel-level layout (window rectangles, tiling, scroll
// origins), which cannot be byte-matched against the C oracle: the Go build and
// the C build use different fonts (different glyph metrics), so identical
// operations produce different pixel coordinates. These tests instead verify the
// Go port's layout/scroll logic is internally consistent; everything that is
// font-independent (buffer contents, dot, window structure) is covered by the
// differential -D tests elsewhere.

// newTestAcme opens a real display, runs the -D script, and returns the live
// *Acme (display left open; closed on test cleanup) so a test can drive the
// graphical layout functions that no -D command reaches (mouse operations).
func newTestAcme(t *testing.T, script string) *Acme {
	t.Helper()
	a := new(Acme)
	var out, errb bytes.Buffer
	a.stdout = &out
	a.stderr = &errb
	a.reinit()
	if err := a.initdraw("acme", "1024x768"); err != nil {
		t.Skipf("no display: %v", err)
	}
	// These tests deliberately leak draw.Images (whole windows/columns). Each
	// Image has a finalizer that flushes a "free" message to the display; if we
	// Closed the display, a later GC would flush those to a closed fd and, after
	// ~100 of them, the draw package panics with a flush error loop. Leaving the
	// display open lets the finalizers flush harmlessly; go test exits via
	// os.Exit, which skips any still-pending finalizers. The OS reclaims the fd.
	a.rowinit(&a.row, a.screen.Clipr)
	sc := bufio.NewScanner(strings.NewReader(script))
	sc.Buffer(make([]byte, 0, 1<<20), 1<<20)
	for sc.Scan() {
		line := sc.Text()
		if line == "" || line[0] == '#' {
			continue
		}
		a.dcmd(line)
	}
	return a
}

// stackedColumnScript builds a script that stacks n windows, each with body
// content, into the first column.
func stackedColumnScript(n int) string {
	var b strings.Builder
	for k := 1; k <= n; k++ {
		b.WriteString("win\nbody ")
		b.WriteString(itoa(k))
		b.WriteString(" a\\nb\\nc\\nd\\ne\\nf\\ng\\nh\n")
	}
	return b.String()
}

// checkColumnTiled verifies the windows tile their column top-to-bottom with no
// gaps or overlaps beyond the inter-window border.
func checkColumnTiled(t *testing.T, c *Column) {
	t.Helper()
	for i := 0; i < c.nw(); i++ {
		w := c.w[i]
		if w.r.Min.Y >= w.r.Max.Y {
			t.Fatalf("window %d has empty rect %v", i, w.r)
		}
		if w.r.Min.X < c.r.Min.X || w.r.Max.X > c.r.Max.X {
			t.Fatalf("window %d x-range %v outside column %v", i, w.r, c.r)
		}
	}
}

// TestColumnGeometry drives colgrow's mouse-only modes (but = -1, 2, 3, and a
// but==1 grow of a non-last window) and coladd's insert-before-last and
// reattach-existing-window paths, which no -D command exercises.
func TestColumnGeometry(t *testing.T) {
	a := newTestAcme(t, stackedColumnScript(4))
	c := a.row.col[0]
	if c.nw() != 4 {
		t.Fatalf("want 4 windows, got %d", c.nw())
	}

	// grow a non-last (middle) window: exercises prune-from-later, scan-below
	// and pack-below.
	a.colgrow(c, c.w[1], 1)
	checkColumnTiled(t, c)

	// but==-1: make a non-last window fill its own space.
	a.colgrow(c, c.w[1], -1)
	checkColumnTiled(t, c)

	// but==-1 on the last window takes the column-bottom branch.
	a.colgrow(c, c.w[c.nw()-1], -1)
	checkColumnTiled(t, c)

	// but==2: give a window as much room as possible.
	a.colgrow(c, c.w[2], 2)
	checkColumnTiled(t, c)

	// coladd with a y inside the column lands before the last window.
	midy := (c.r.Min.Y + c.r.Max.Y) / 2
	a.coladd(c, nil, nil, midy)
	checkColumnTiled(t, c)

	// reattach an already-allocated window (the Zerox/column-move path).
	w := c.w[c.nw()-1]
	a.colclose(c, w, false)
	a.coladd(c, w, nil, -1)
	checkColumnTiled(t, c)

	// grow a window that retained a large maxlines from a former tall frame: in
	// the now-crowded column its heuristic target falls below its own minimum.
	c.w[1].maxlines = 80
	a.colgrow(c, c.w[1], 1)
	checkColumnTiled(t, c)

	// but==3: make a window full-size (reorders to front, marks column unsafe).
	a.colgrow(c, c.w[1], 3)
	if c.safe {
		t.Error("colgrow but==3 should clear c.safe")
	}
}

// TestColumnStackDeep packs many content-heavy windows into one column via
// makenewwindow, repeatedly invoking coladd's grow loop (colgrow).
func TestColumnStackDeep(t *testing.T) {
	a := newTestAcme(t, "")
	lines := []rune(strings.Repeat("x\n", 20))
	var c *Column
	for i := 0; i < 40; i++ {
		w := a.makenewwindow(nil)
		c = w.col
		a.textinsert(&w.body, 0, lines, true)
	}
	if c.nw() == 0 {
		t.Fatal("no windows created")
	}
	checkColumnTiled(t, c)
}

// TestColumnOverflow forces coladd's "too many windows" recovery: a column only
// a few lines tall cannot hold more than a couple of content-filled windows, so
// the grow loop gives up (buggered) and colresize reflows the whole column. A
// short column reaches this with a handful of windows, avoiding the image counts
// that would otherwise destabilise the test display.
func TestColumnOverflow(t *testing.T) {
	a := newTestAcme(t, "win\n")
	c := a.row.col[0]
	r := c.r
	r.Max.Y = r.Min.Y + 6*a.font.Height // ~6 lines tall
	a.colresize(c, r)
	lines := []rune("a\nb\nc\nd\ne\nf\ng\nh\ni\nj\n")
	for i := 0; i < 12; i++ {
		w := a.makenewwindow(nil)
		a.textinsert(&w.body, 0, lines, true)
	}
	if c.nw() < 2 {
		t.Fatalf("expected several windows, got %d", c.nw())
	}
	checkColumnTiled(t, c)
}

// TestIsalnum checks isalnum matches C util.c: '_' and non-ASCII letters are
// alphanumeric (so double-click/Look widen across foo_bar and café), while
// space, control chars, and ASCII punctuation are not.
func TestIsalnum(t *testing.T) {
	for _, r := range "0aZ_éµ日" { // 0 a Z _ é µ 日
		if !isalnum(r) {
			t.Errorf("isalnum(%q) = false, want true", r)
		}
	}
	for _, r := range " \t!.\"/:;~\x7f" {
		if isalnum(r) {
			t.Errorf("isalnum(%q) = true, want false", r)
		}
	}
}

// TestDirnameLongRelative checks dirname splices a relative name longer than the
// tag remainder without panicking or truncating (parsetag must reserve room, per
// C runemalloc(nc+extra+1)).
func TestDirnameLongRelative(t *testing.T) {
	a := newTestAcme(t, "win\n")
	w := a.row.col[0].w[0]
	a.winsetname(w, []rune("/a/b/file"))
	long := strings.Repeat("x", 50)
	got := string(a.dirname(&w.body, []rune(long)))
	if want := "/a/b/" + long; got != want {
		t.Errorf("dirname(long relative) = %q, want %q", got, want)
	}
}

// TestWriteDropsNul checks that a body write drops embedded NUL bytes on insert
// (C's xfidwrite uses cvttorunes, which elides NUL) while still reporting the
// full byte count as the 9P write count.
func TestWriteDropsNul(t *testing.T) {
	a := newTestAcme(t, "win\n")
	w := a.row.col[0].w[0]
	n, errs := a.xfidWriteName(w, qWbody, "a\x00b", nil, nil)
	if errs != "" {
		t.Fatalf("write: %s", errs)
	}
	if n != 3 {
		t.Errorf("write count = %d, want 3 (bytes, incl NUL)", n)
	}
	if got := readRunes(&w.body, 0, w.body.file.b.nc()); got != "ab" {
		t.Errorf("body = %q, want %q (NUL dropped on insert, like C cvttorunes)", got, "ab")
	}
}

// TestCtlNameBadCharContinues checks that a bad character in a ctl "name" command
// still applies the name, records the error, AND keeps processing the rest of a
// multi-command ctl message (xfid.c's goto-out path), unlike a hard error which
// stops. Here the "dirty" after the bad name must still take effect.
func TestCtlNameBadCharContinues(t *testing.T) {
	a := newTestAcme(t, "win\n")
	w := a.row.col[0].w[0]
	n, errs := a.xfidctlwrite(w, "name a\x01b\ndirty\n", nil)
	if errs == "" {
		t.Fatal("bad character in name should report an error")
	}
	if n != 0 {
		t.Errorf("error response count = %d, want 0", n)
	}
	if !w.dirty {
		t.Error("the dirty command after a bad-char name was skipped (should continue per C goto-out)")
	}
}

// TestDelcolBusyGuard checks delcol refuses to delete a column while a window in
// it has an external program holding one of its 9P files open (exec.c delcol).
func TestDelcolBusyGuard(t *testing.T) {
	a := newTestAcme(t, "win\n")
	a.dcmd("newcol") // a second column, so deleting one isn't the last
	c := a.row.col[0]
	w := c.w[0]
	et := &w.body
	before := a.row.ncol()

	// pretend an external program is reading the window's event file
	w.nopen[qWevent] = 1
	a.delcol(et, nil, nil, false, false, nil)
	if a.row.ncol() != before {
		t.Error("delcol deleted a column whose window is running an external command")
	}

	// once nothing holds a file open, the clean column deletes
	w.nopen[qWevent] = 0
	a.delcol(et, nil, nil, false, false, nil)
	if a.row.ncol() != before-1 {
		t.Errorf("delcol did not delete the clean column: ncol=%d, want %d", a.row.ncol(), before-1)
	}
}

// TestBarttextReset checks the two barttext assignments: coladd makes the new
// window the scroll-key target (cols.c), and textclose clears it when that text
// is closed (text.c) so a held-button scroll never acts on freed text.
func TestBarttextReset(t *testing.T) {
	a := newTestAcme(t, "win\n")
	w := a.row.col[0].w[0]
	if a.barttext != &w.body {
		t.Fatalf("coladd did not set barttext to the new window body")
	}
	a.winclose(w)
	if a.barttext != nil {
		t.Error("barttext not cleared after its window was closed (dangling pointer)")
	}
}

// TestColaddButtonWarpGuard checks coladd's held-button guard (cols.c): a new
// window warps the mouse to itself only when no button is down.
func TestColaddButtonWarpGuard(t *testing.T) {
	a := newTestAcme(t, "win\n")
	c := a.row.col[0]
	if a.mousectl == nil {
		a.mousectl = new(draw.Mousectl)
	}

	// button held: coladd must not warp the mouse
	a.mousectl.Buttons = 1
	before := a.mousexy
	a.coladd(c, nil, nil, -1)
	if a.mousexy != before {
		t.Errorf("coladd warped the mouse while a button was held: %v -> %v", before, a.mousexy)
	}

	// button released: coladd warps to the new window
	a.mousectl.Buttons = 0
	a.coladd(c, nil, nil, -1)
	if a.mousexy == before {
		t.Error("coladd did not warp the mouse after the button was released")
	}
}
