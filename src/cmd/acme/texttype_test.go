package main

import (
	"strings"
	"testing"
)

// These tests exercise texttype/textfill/textshow paths that the -D command set
// cannot reach as differential tests: scrolled frames and tag/directory redraw
// depend on pixel layout (font metrics differ between the Go and C builds, so
// origins and line counts cannot be byte-matched), and autoindent and directory
// windows have no -D toggle (the Indent command and directory loading are not
// ported). They are Go-only invariant tests; see the note in cols_test.go.

// TestDirectoryRedraw covers textredraw's directory branches (narrower tab stop
// and the directory-columnation path) by marking a window as a directory.
func TestDirectoryRedraw(t *testing.T) {
	a := newTestAcme(t, "win\nbody 1 file1\\nfile2\\n")
	w := a.row.col[0].w[0]
	w.isdir = true
	r := w.body.all
	r.Max.X -= 30 // change the width so odx != all.Dx()
	a.textresize(&w.body, r, true)
}

// TestTexttypeEdge covers the typing edge cases the -D type/key path does not
// reach: textfill flushing an uncommitted cache, textshow on a zero-height
// frame, ESC after backspacing below the typing start, and a backspace at the
// top of a scrolled frame (which clamps the erase width to zero).
func TestTexttypeEdge(t *testing.T) {
	a := newTestAcme(t, "win\n")
	b := &a.row.col[0].w[0].body

	// textfill with an uncommitted typing cache commits it first.
	a.texttype(b, 'h')
	a.textfill(b)
	a.textcommit(b, true)

	// textshow on a frame with no room shows nothing.
	save := b.fr.MaxLines
	b.fr.MaxLines = 0
	a.textshow(b, 0, 0, true)
	b.fr.MaxLines = save
	a.textcommit(b, true)

	// ESC after a ^U that erases past the typing start: the typing origin is now
	// ahead of the dot.
	a.textinsert(b, b.file.b.nc(), []rune("abc\ndef"), true)
	a.textsetselect(b, b.file.b.nc(), b.file.b.nc())
	b.eq0 = ^0           // fresh typing origin (as a mouse selection would set)
	a.texttype(b, 'X')   // typing origin = before X
	a.texttype(b, Knack) // ^U erases the whole line, dropping the dot below eq0
	a.texttype(b, Kesc)
	a.textcommit(b, true)

	// Backspace at the very top of a scrolled frame clamps the erase to nothing.
	var sb strings.Builder
	for i := 0; i < 400; i++ {
		sb.WriteString("L")
		sb.WriteString(itoa(i))
		sb.WriteByte('\n')
	}
	a.textinsert(b, b.file.b.nc(), []rune(sb.String()), true)
	a.textsetorigin(b, b.file.b.nc()/2, false)
	a.textsetselect(b, b.org, b.org)
	a.texttype(b, Kbs)
}

// TestTextScrollDirect drives a text scrolled off the top so that
// textinsert/textdelete edit above or across the viewport, textsetorigin runs
// its inexact-origin newline scan, and textshow/texttype operate on a scrolled
// frame — none of which the small -D windows reach.
func TestTextScrollDirect(t *testing.T) {
	a := newTestAcme(t, "win\n")
	w := a.row.col[0].w[0]
	var sb strings.Builder
	for i := 0; i < 400; i++ {
		sb.WriteString("line")
		sb.WriteString(itoa(i))
		sb.WriteByte('\n')
	}
	a.textinsert(&w.body, 0, []rune(sb.String()), true)

	// scroll into the middle with an inexact origin (forces the newline scan).
	mid := w.body.file.b.nc() / 2
	a.textsetorigin(&w.body, mid, false)
	if w.body.org == 0 {
		t.Fatal("text did not scroll")
	}
	org := w.body.org

	// insert above the viewport: only the origin shifts.
	a.textinsert(&w.body, 0, []rune("TOP\n"), true)
	if w.body.org != org+4 {
		t.Errorf("org after insert-above = %d want %d", w.body.org, org+4)
	}

	// delete a span crossing the top of the viewport.
	a.textdelete(&w.body, 0, w.body.org+10, true)

	// delete a long span (past the frame's bottom) so the frame-delete clamps.
	a.textsetselect(&w.body, w.body.org, w.body.org)
	a.textdelete(&w.body, w.body.org, w.body.file.b.nc(), true)

	// bring the very top back into view, then move/type/backspace there.
	a.textshow(&w.body, 0, 0, true)
	a.textsetselect(&w.body, 5, 5)
	a.texttype(&w.body, Kright)
	a.textsetselect(&w.body, 0, 0)
	a.texttype(&w.body, 'Q')
	a.texttype(&w.body, Kbs)

	// a headless text (no frame image): textfill is a no-op and typecommit
	// commits directly (no window).
	ht := &Text{file: new(File)}
	ht.file.b.Insert(0, []rune("x"))
	a.textfill(ht)
	a.typecommit(ht)
}

// TestTextScrollNav drives small forward scrolls with a visible selection (the
// fixup redraw), the inexact-origin newline scan, a rightward move, and a show
// far below the viewport (the scroll-down loop).
func TestTextScrollNav(t *testing.T) {
	a := newTestAcme(t, "win\n")
	w := a.row.col[0].w[0]
	var sb strings.Builder
	for i := 0; i < 400; i++ {
		sb.WriteString("line")
		sb.WriteString(itoa(i))
		sb.WriteByte('\n')
	}
	a.textinsert(&w.body, 0, []rune(sb.String()), true)

	// scroll a little so a selection well below the new origin stays visible.
	a.textsetorigin(&w.body, 12, true)
	a.textsetselect(&w.body, 40, 50)
	// a small inexact forward scroll: the origin lands mid-line, so the newline
	// scan steps over several characters, and the visible selection drives the
	// fixup redraw.
	a.textsetorigin(&w.body, 14, false)

	// move right (textshow of q1+1).
	a.textsetselect(&w.body, 30, 30)
	a.texttype(&w.body, Kright)

	// show the very end, far below the viewport (the scroll-down loop).
	nc := w.body.file.b.nc()
	a.textshow(&w.body, nc, nc, true)
}

// TestTexttypeBranches covers texttype paths that the -D key/type commands do
// not reach: autoindent on newline, typing into a tag (tagsafe + textshow on a
// non-body text), and a newline typed into a column tag (ignored).
func TestTexttypeBranches(t *testing.T) {
	a := newTestAcme(t, "win\n")
	w := a.row.col[0].w[0]

	// autoindent: a newline copies the leading blanks of the previous line.
	w.autoindent = true
	a.textinsert(&w.body, 0, []rune("  hi"), true)
	a.textsetselect(&w.body, 4, 4)
	a.texttype(&w.body, '\n')
	a.textcommit(&w.body, true)
	if got := string(w.body.file.b.c); got != "  hi\n  " {
		t.Errorf("autoindent: got %q want %q", got, "  hi\n  ")
	}

	// typing into a tag marks it unsafe and navigation calls textshow on a
	// non-body text.
	a.texttype(&w.tag, 'x')
	a.texttype(&w.tag, Kleft)

	// a newline typed into a column tag (neither Body nor Tag) is ignored.
	a.texttype(&a.row.col[0].tag, '\n')
}
