package main

import (
	"os"
	"path/filepath"
	"testing"
)

// TestDirWindowRecolumnate checks that resizing a directory window re-flows its
// listing to the new width (text.c textredraw's isdir width-change branch).
// The exact layout is font-dependent, so this is a Go-only invariant test: it
// asserts the body text changes when the window is made much narrower.
func TestDirWindowRecolumnate(t *testing.T) {
	dir := t.TempDir()
	for _, n := range []string{"aa", "bb", "cc", "dd", "ee", "ff", "gg", "hh",
		"ii", "jj", "kk", "ll", "mm", "nn", "oo", "pp", "qq", "rr"} {
		if err := os.WriteFile(filepath.Join(dir, n), nil, 0o644); err != nil {
			t.Fatal(err)
		}
	}
	a := newTestAcme(t, "win\n")
	w := a.row.col[0].w[0]
	a.winsetname(w, []rune(dir))
	if a.textload(&w.body, 0, dir, true) < 0 {
		t.Fatal("textload(dir) failed")
	}
	if !w.isdir {
		t.Fatal("window is not a directory window after loading a directory")
	}
	wide := readRunes(&w.body, 0, w.body.file.b.nc())
	if wide == "" {
		t.Fatal("empty directory listing")
	}
	// Shrink the window's width so the listing must use fewer columns, then
	// resize (textresize -> textredraw with a changed odx).
	r := w.r
	r.Max.X = r.Min.X + r.Dx()/4
	a.winresize(w, r, false, false)
	narrow := readRunes(&w.body, 0, w.body.file.b.nc())
	if wide == narrow {
		t.Errorf("directory listing did not recolumnate on width change\nwide=%q\nnarrow=%q", wide, narrow)
	}
}

// These row-geometry tests are Go-only invariant tests, not -D differential
// tests: they check pixel-level layout that cannot be byte-matched against the C
// oracle because the two builds use different fonts. See the note in
// cols_test.go.

// TestRowGeometry drives rowresize and the makenewwindow column-selection
// fallbacks (seltext->col, t->col, and the t-in-populated-column path), plus
// rowadd's reattach-existing-column and tiny-new-column branches. None of these
// is reached by a -D command but all are exercised here directly.
func TestRowGeometry(t *testing.T) {
	a := newTestAcme(t, stackedColumnScript(2))
	a.dcmd("newcol") // a second column
	if a.row.ncol() < 2 {
		t.Fatalf("want >=2 columns, got %d", a.row.ncol())
	}

	// rowresize reflows every column (the multi-column loop), scaling each
	// column's width proportionally to the new row rect.
	r := a.row.r
	r.Max.X -= 80
	a.rowresize(&a.row, r)
	for i, c := range a.row.col {
		if c.r.Min.X >= c.r.Max.X {
			t.Fatalf("column %d has empty x-range %v after rowresize", i, c.r)
		}
	}

	// makenewwindow with no active column but a selected text: uses seltext->col.
	a.activecol = nil
	a.seltext = &a.row.col[0].w[0].body
	if w := a.makenewwindow(nil); w == nil {
		t.Fatal("makenewwindow(seltext) returned nil")
	}

	// makenewwindow given a text in a populated column: uses t->col and the
	// split-path branch (find biggest/emptiest window, pick a y, split there).
	a.activecol = nil
	a.seltext = nil
	w0 := a.row.col[0].w[0]
	col0 := w0.col
	nbefore := col0.nw()
	w := a.makenewwindow(&w0.body)
	if w == nil {
		t.Fatal("makenewwindow(t) returned nil")
	}
	if w.col != col0 {
		t.Errorf("split-path new window landed in a different column than its source text")
	}
	if col0.nw() != nbefore+1 {
		t.Errorf("source column nw = %d after split, want %d", col0.nw(), nbefore+1)
	}

	// rowadd re-attaching an already-initialised column (the load/move path).
	nc := new(Column)
	a.colinit(nc, a.row.col[0].r)
	a.rowadd(&a.row, nc, a.row.r.Max.X-150)

	// rowadd with x close to the landing column's left edge makes the new column
	// too narrow, so its width is clamped to the 50px minimum.
	d := a.row.col[a.row.ncol()-1]
	a.rowadd(&a.row, nil, d.r.Min.X+5)

	// rowadd onto a column already narrower than 100px refuses to split it.
	narrow := a.row.col[0]
	nr := narrow.r
	nr.Max.X = nr.Min.X + 80
	a.colresize(narrow, nr)
	if a.rowadd(&a.row, nil, narrow.r.Min.X+40) != nil {
		t.Error("rowadd onto a too-narrow column should return nil")
	}
}

// TestDriverNewcolNarrow covers the -D driver's "rowadd failed" guard: a newcol
// command when the row is too narrow to split a column.
func TestDriverNewcolNarrow(t *testing.T) {
	a := newTestAcme(t, "win\n")
	c := a.row.col[0]
	r := c.r
	r.Max.X = r.Min.X + 80
	a.colresize(c, r)
	a.row.r.Max.X = r.Max.X
	defer func() {
		if recover() == nil {
			t.Error("newcol on a too-narrow row should fatalf")
		}
	}()
	a.dcmd("newcol")
}
