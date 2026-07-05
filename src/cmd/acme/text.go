package main

import (
	"fmt"
	"os"
	"sort"
	"strconv"
	"strings"

	"9fans.net/plan9port/draw"
	"9fans.net/plan9port/draw/frame"
)

// Text is one editable text region (a tag or body) in a window: a frame over a
// File (dat.h Text). It ports text.c — the buffer/selection mutation core
// (textinsert/textdelete/textsetselect/textfill) plus the frame drawing and
// scrolling helpers. Drawing is performed via the go/frame package; when the
// frame has no image (B==nil, as in headless unit tests) those operations are
// no-ops, so the buffer and selection logic is still exercised without a display.
type Text struct {
	file    *File
	fr      frame.Frame
	reffont *Reffont
	org     int
	q0      int
	q1      int
	what    int
	tabstop int
	w       *Window
	scrollr draw.Rectangle
	lastsr  draw.Rectangle
	all     draw.Rectangle
	row     *Row
	col     *Column

	iq1         int // last input position
	eq0         int // start of typing for ESC
	cq0         int // cache position
	ncache      int // storage for insert
	ncachealloc int
	cache       []rune
	nofill      bool
	needundo    bool
}

// TABDIR is the tab width used in directory windows (dat.h).
const TABDIR = 3

// textinit initializes text t over file f within rectangle r, laying it out in
// the frame (text.c).
func (a *Acme) textinit(t *Text, f *File, r draw.Rectangle, rf *Reffont, cols [frame.NCOL]*draw.Image) {
	t.file = f
	t.all = r
	t.scrollr = r
	t.scrollr.Max.X = r.Min.X + a.scrollwid()
	t.lastsr = a.nullrect
	r.Min.X += a.scrollwid() + a.scrollgap()
	t.eq0 = ^0
	t.ncache = 0
	t.reffont = rf
	t.tabstop = a.maxtab
	copy(t.fr.Cols[:], cols[:])
	a.textredraw(t, r, rf.f, a.screen, -1)
}

// textredraw (re)initializes the frame and fills it from the file (text.c).
func (a *Acme) textredraw(t *Text, r draw.Rectangle, f *draw.Font, b *draw.Image, odx int) {
	t.fr.Init(r, f, b, t.fr.Cols[:])
	rr := t.fr.R
	rr.Min.X -= a.scrollwid() + a.scrollgap() // back-fill to scroll bar
	if t.fr.NoRedraw == 0 {
		t.fr.B.Draw(rr, t.fr.Cols[frame.BACK], nil, draw.ZP)
	}
	// use no wider than 3-space tabs in a directory
	maxt := a.maxtab
	if t.what == Body {
		if t.w.isdir {
			maxt = min(TABDIR, a.maxtab)
		} else {
			maxt = t.tabstop
		}
	}
	t.fr.MaxTab = maxt * f.StringWidth("0")
	if t.what == Body && t.w.isdir && odx != t.all.Dx() {
		// a directory window changed width: recolumnate the listing to the new
		// width (text.c). With no visible lines there is nothing to redo.
		if t.fr.MaxLines > 0 {
			a.textreset(t)
			a.textcolumnate(t, t.w.dlp)
			a.textshow(t, 0, 0, true)
		}
	} else {
		a.textfill(t)
		a.textsetselect(t, t.q0, t.q1)
	}
}

// textresize lays text t out in rectangle r, returning the resulting bottom y
// (text.c). keepextra leaves a partial line of slack at the bottom.
func (a *Acme) textresize(t *Text, r draw.Rectangle, keepextra bool) int {
	if r.Dy() <= 0 {
		r.Max.Y = r.Min.Y
	} else if !keepextra {
		r.Max.Y -= r.Dy() % t.fr.Font.Height
	}
	odx := t.all.Dx()
	t.all = r
	t.scrollr = r
	t.scrollr.Max.X = r.Min.X + a.scrollwid()
	t.lastsr = a.nullrect
	r.Min.X += a.scrollwid() + a.scrollgap()
	t.fr.Clear(false)
	a.textredraw(t, r, t.fr.Font, t.fr.B, odx)
	if keepextra && t.fr.R.Max.Y < t.all.Max.Y && t.fr.NoRedraw == 0 {
		// draw background in the bottom fringe of the window
		r.Min.X -= a.scrollgap()
		r.Min.Y = t.fr.R.Max.Y
		r.Max.Y = t.all.Max.Y
		a.screen.Draw(r, t.fr.Cols[frame.BACK], nil, draw.ZP)
	}
	return t.all.Max.Y
}

// textreadc returns the rune at position q: from the typing cache if q falls
// within it, else from the file buffer (text.c). (The C UTF cache for 9P reads
// is an optimization with no effect on the returned rune and is dropped.)
func (a *Acme) textreadc(t *Text, q int) rune {
	if t.cq0 <= q && q < t.cq0+t.ncache {
		return t.cache[q-t.cq0]
	}
	return t.file.b.c[q]
}

// textconstrain clamps q0,q1 into [0,nc] for the text's file (text.c).
func (a *Acme) textconstrain(t *Text, q0, q1 int) (p0, p1 int) {
	nc := t.file.b.nc()
	p0 = min(q0, nc)
	p1 = min(q1, nc)
	return p0, p1
}

// textinsert inserts r at q0, optionally into the file (tofile), propagating to
// zerox copies, shifting the selection, and updating the frame (text.c).
func (a *Acme) textinsert(t *Text, q0 int, r []rune, tofile bool) {
	n := len(r)
	if tofile && t.ncache != 0 {
		// unreachable
		bug("text.insert")
	}
	if n == 0 {
		return
	}
	if tofile {
		a.fileinsert(t.file, q0, r)
		if t.what == Body {
			t.w.dirty = true
			t.w.utflastqid = -1
		}
		if len(t.file.text) > 1 {
			for _, u := range t.file.text {
				if u != t {
					u.w.dirty = true // always a body
					a.textinsert(u, q0, r, false)
					a.textsetselect(u, u.q0, u.q1)
					a.textscrdraw(u)
				}
			}
		}
	}
	if q0 < t.iq1 {
		t.iq1 += n
	}
	if q0 < t.q1 {
		t.q1 += n
	}
	if q0 < t.q0 {
		t.q0 += n
	}
	if q0 < t.org {
		t.org += n
	} else if q0 <= t.org+t.fr.NumChars {
		t.fr.Insert(r[:n], q0-t.org)
	}
	if t.w != nil {
		a.winevent(t.w, eventInsert(t.what), q0, q0+n, r)
	}
}

// textdelete removes [q0,q1), optionally from the file, propagating to zerox
// copies, shifting the selection, and updating the frame (text.c).
func (a *Acme) textdelete(t *Text, q0, q1 int, tofile bool) {
	n := q1 - q0
	if tofile && t.ncache != 0 {
		// unreachable
		bug("text.delete")
	}
	if n == 0 {
		return
	}
	if tofile {
		a.filedelete(t.file, q0, q1)
		if t.what == Body {
			t.w.dirty = true
			t.w.utflastqid = -1
		}
		if len(t.file.text) > 1 {
			for _, u := range t.file.text {
				if u != t {
					u.w.dirty = true // always a body
					a.textdelete(u, q0, q1, false)
					a.textsetselect(u, u.q0, u.q1)
					a.textscrdraw(u)
				}
			}
		}
	}
	if q0 < t.iq1 {
		t.iq1 -= min(n, t.iq1-q0)
	}
	if q0 < t.q0 {
		t.q0 -= min(n, t.q0-q0)
	}
	if q0 < t.q1 {
		t.q1 -= min(n, t.q1-q0)
	}
	if q1 <= t.org {
		t.org -= n
	} else if q0 < t.org+t.fr.NumChars {
		p1 := q1 - t.org
		if p1 > t.fr.NumChars {
			p1 = t.fr.NumChars
		}
		var p0 int
		if q0 < t.org {
			t.org = q0
			p0 = 0
		} else {
			p0 = q0 - t.org
		}
		t.fr.Delete(p0, p1)
		a.textfill(t)
	}
	if t.w != nil {
		a.winevent(t.w, eventDelete(t.what), q0, q1, nil)
	}
}

// textsetselect sets the selection to [q0,q1) and updates the on-screen
// highlight (text.c). t.fr.P0/P1 are always right; t.q0/t.q1 may be off-screen.
func (a *Acme) textsetselect(t *Text, q0, q1 int) {
	t.q0 = q0
	t.q1 = q1
	// compute desired p0,p1 from q0,q1
	p0 := q0 - t.org
	p1 := q1 - t.org
	ticked := true
	if p0 < 0 {
		ticked = false
		p0 = 0
	}
	if p1 < 0 {
		p1 = 0
	}
	if p0 > t.fr.NumChars {
		p0 = t.fr.NumChars
	}
	if p1 > t.fr.NumChars {
		ticked = false
		p1 = t.fr.NumChars
	}
	if p0 == t.fr.P0 && p1 == t.fr.P1 {
		if p0 == p1 && ticked != t.fr.Ticked {
			t.fr.Tick(t.fr.PointOf(p0), ticked)
		}
		return
	}
	if p0 > p1 {
		// unreachable
		bug("textsetselect p0>p1")
	}
	// screen disagrees with desired selection
	if t.fr.P1 <= p0 || p1 <= t.fr.P0 || p0 == p1 || t.fr.P1 == t.fr.P0 {
		// no overlap or too easy to bother trying
		t.fr.Drawsel(t.fr.PointOf(t.fr.P0), t.fr.P0, t.fr.P1, false)
		if p0 != p1 || ticked {
			t.fr.Drawsel(t.fr.PointOf(p0), p0, p1, true)
		}
		t.fr.P0 = p0
		t.fr.P1 = p1
		return
	}
	// overlap; avoid unnecessary painting
	if p0 < t.fr.P0 {
		t.fr.Drawsel(t.fr.PointOf(p0), p0, t.fr.P0, true) // extend backwards
	} else if p0 > t.fr.P0 {
		t.fr.Drawsel(t.fr.PointOf(t.fr.P0), t.fr.P0, p0, false) // trim first part
	}
	if p1 > t.fr.P1 {
		t.fr.Drawsel(t.fr.PointOf(t.fr.P1), t.fr.P1, p1, true) // extend forwards
	} else if p1 < t.fr.P1 {
		t.fr.Drawsel(t.fr.PointOf(p1), p1, t.fr.P1, false) // trim last part
	}
	t.fr.P0 = p0
	t.fr.P1 = p1
}

// textfill brings the frame up to date with the file, appending text until the
// last line is full (text.c). With no frame image there is nothing to fill.
func (a *Acme) textfill(t *Text) {
	if t.fr.LastLineFull || t.nofill {
		return
	}
	if t.fr.B == nil {
		// headless: no image to fill
		return
	}
	if t.ncache > 0 {
		a.typecommit(t)
	}
	for {
		n := t.file.b.nc() - (t.org + t.fr.NumChars)
		if n == 0 {
			break
		}
		if n > 2000 { // educated guess at a reasonable amount
			n = 2000
		}
		rp := make([]rune, n)
		t.file.b.Read(t.org+t.fr.NumChars, rp)
		// it's expensive to frinsert more than we need, so count newlines
		nl := t.fr.MaxLines - t.fr.NumLines
		m := 0
		i := 0
		for i < n {
			c := rp[i]
			i++
			if c == '\n' {
				m++
				if m >= nl {
					break
				}
			}
		}
		t.fr.Insert(rp[:i], t.fr.NumChars)
		if t.fr.LastLineFull {
			break
		}
	}
}

// eventInsert/eventDelete pick the winevent type byte for body vs tag.
func eventInsert(what int) byte {
	if what == Body {
		return 'I'
	}
	return 'i'
}

func eventDelete(what int) byte {
	if what == Body {
		return 'D'
	}
	return 'd'
}

// Bracket/quote tables for double-click selection (text.c). left2/left3 match
// themselves; right reuses them.
var (
	dclickLeft  = [][]rune{[]rune("{[(<«"), []rune("\n"), []rune("'\"`")}
	dclickRight = [][]rune{[]rune("}])>»"), []rune("\n"), []rune("'\"`")}
)

func runeIndex(s []rune, c rune) int {
	for i, r := range s {
		if r == c {
			return i
		}
	}
	return -1
}

// isalnum reports whether r is alphanumeric for word selection (util.c isalnum):
// hard to get absolutely right, so assume anything above the Latin control
// characters that is not punctuation is potentially alphanumeric. Notably this
// includes '_' (not in the punctuation set) and all non-ASCII letters/CJK, so
// double-click and Look widen across foo_bar and café.txt as C does.
func isalnum(r rune) bool {
	if r <= ' ' {
		return false
	}
	if 0x7F <= r && r <= 0xA0 {
		return false
	}
	if strings.ContainsRune("!\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~", r) {
		return false
	}
	return true
}

// textclickmatch scans from *q in direction dir for the bracket cr matching cl,
// tracking nesting; it advances *q and returns whether a match was found
// (text.c).
func (a *Acme) textclickmatch(t *Text, cl, cr, dir int, q *int) bool {
	nest := 1
	for {
		var c rune
		if dir > 0 {
			if *q == t.file.b.nc() {
				break
			}
			c = a.textreadc(t, *q)
			*q++
		} else {
			if *q == 0 {
				break
			}
			*q--
			c = a.textreadc(t, *q)
		}
		if int(c) == cr {
			nest--
			if nest == 0 {
				return true
			}
		} else if int(c) == cl {
			nest++
		}
	}
	return cl == '\n' && nest == 1
}

// textdoubleclick expands [*q0,*q1) to a bracketed region or alphanumeric word
// around the click (text.c).
func (a *Acme) textdoubleclick(t *Text, q0, q1 *int) {
	if a.textclickhtmlmatch(t, q0, q1) {
		return
	}
	nc := t.file.b.nc()
	for i := range dclickLeft {
		q := *q0
		l := dclickLeft[i]
		r := dclickRight[i]
		// try matching character to the left, looking right
		var c rune
		if q == 0 {
			c = '\n'
		} else {
			c = a.textreadc(t, q-1)
		}
		if p := runeIndex(l, c); p >= 0 {
			if a.textclickmatch(t, int(c), int(r[p]), 1, &q) {
				*q1 = q
				if c != '\n' {
					*q1--
				}
			}
			return
		}
		// try matching character to the right, looking left
		if q == nc {
			c = '\n'
		} else {
			c = a.textreadc(t, q)
		}
		if p := runeIndex(r, c); p >= 0 {
			if a.textclickmatch(t, int(c), int(l[p]), -1, &q) {
				v := 0
				if *q0 < nc && c == '\n' {
					v = 1
				}
				*q1 = *q0 + v
				*q0 = q
				if c != '\n' || q != 0 || a.textreadc(t, 0) == '\n' {
					*q0++
				}
			}
			return
		}
	}
	// try filling out the word to the right, then the left
	for *q1 < nc && isalnum(a.textreadc(t, *q1)) {
		*q1++
	}
	for *q0 > 0 && isalnum(a.textreadc(t, *q0-1)) {
		*q0--
	}
}

// textbacknl returns the position n display lines back from p, treating a run
// of 128 chars without a newline as a line (text.c).
func (a *Acme) textbacknl(t *Text, p, n int) int {
	// look for start of this line if n==0
	if n == 0 && p > 0 && a.textreadc(t, p-1) != '\n' {
		// Every caller passes a line-start origin (or n>0), so the mid-line
		// n==0 snap-back is never reached via -D.
		// untested
		n = 1
	}
	for i := n; i > 0 && p > 0; i-- {
		p-- // it's at a newline now; back over it
		if p == 0 {
			break
		}
		// at 128 chars, call it a line anyway
		for j := 128; j > 1 && p > 0; j-- {
			if a.textreadc(t, p-1) == '\n' {
				break
			}
			p--
		}
	}
	return p
}

// textbswidth returns how many characters a backspace key c should erase:
// ^H one char, ^W a word, ^U to the start of the line (text.c).
func (a *Acme) textbswidth(t *Text, c rune) int {
	// there is known to be at least one character to erase
	if c == 0x08 { // ^H: erase character
		return 1
	}
	q := t.q0
	skipping := true
	for q > 0 {
		r := a.textreadc(t, q-1)
		if r == '\n' { // eat at most one more character
			if q == t.q0 { // eat the newline
				q--
			}
			break
		}
		if c == 0x17 { // ^W
			eq := isalnum(r)
			if eq && skipping { // found one; stop skipping
				skipping = false
			} else if !eq && !skipping {
				break
			}
		}
		q--
	}
	return t.q0 - q
}

// Keyboard rune values (keyboard.h).
const (
	KF      = 0xF000
	Kleft   = KF | 0x11
	Kright  = KF | 0x12
	Kup     = KF | 0x0E
	Kdown   = 0x80
	Khome   = KF | 0x0D
	Kend    = KF | 0x18
	Kpgup   = KF | 0x0F
	Kpgdown = KF | 0x13
	// Mouse-wheel one-line scroll (acme dat.h), delivered as runes to texttype.
	Kscrolloneup   = KF | 0x20
	Kscrollonedown = KF | 0x21
	Kbs            = 0x08
	Kdel           = 0x7f
	Kesc           = 0x1b
	Knack          = 0x15 // ^U
	Ketb           = 0x17 // ^W
	Kins           = KF | 0x14
	Kcmd           = 0xF100 // Cmd/%-key base (Cmd+'a' = Kcmd+'a', as on the Mac)
)

// textsetorigin scrolls the frame so it begins at rune org (text.c).
func (a *Acme) textsetorigin(t *Text, org int, exact bool) {
	if org > 0 && !exact && a.textreadc(t, org-1) != '\n' {
		// org is an estimate; find a newline (don't try harder than 256)
		for i := 0; i < 256 && org < t.file.b.nc(); i++ {
			if a.textreadc(t, org) == '\n' {
				org++
				break
			}
			org++
		}
	}
	d := org - t.org
	fixup := false
	switch {
	case d >= 0 && d < t.fr.NumChars:
		t.fr.Delete(0, d)
		fixup = true
	case d < 0 && -d < t.fr.NumChars:
		n := t.org - org
		r := make([]rune, n)
		t.file.b.Read(org, r)
		t.fr.Insert(r, 0)
	default:
		t.fr.Delete(0, t.fr.NumChars)
	}
	t.org = org
	a.textfill(t)
	a.textscrdraw(t)
	a.textsetselect(t, t.q0, t.q1)
	if fixup && t.fr.P1 > t.fr.P0 {
		t.fr.Drawsel(t.fr.PointOf(t.fr.P1-1), t.fr.P1-1, t.fr.P1, true)
	}
}

// textshow makes [q0,q1) visible, optionally selecting it (text.c).
func (a *Acme) textshow(t *Text, q0, q1 int, doselect bool) {
	if t.what != Body {
		if doselect {
			a.textsetselect(t, q0, q1)
		}
		return
	}
	if t.w != nil && t.fr.MaxLines == 0 {
		// window too small to show the target: grow it (text.c)
		a.colgrow(t.col, t.w, 1)
	}
	if doselect {
		a.textsetselect(t, q0, q1)
	}
	qe := t.org + t.fr.NumChars
	tsd := false
	nc := t.file.b.nc() + t.ncache
	if t.org <= q0 {
		if nc == 0 || q0 < qe {
			tsd = true
		} else if q0 == qe && qe == nc {
			if a.textreadc(t, nc-1) == '\n' {
				if t.fr.NumLines < t.fr.MaxLines {
					tsd = true
				}
			} else {
				tsd = true
			}
		}
	}
	if tsd {
		a.textscrdraw(t)
	} else {
		// back up so the target is not jammed against the top edge: 3/4 of the
		// window when a program is watching the event file (it likely wants
		// context), else 1/4 (text.c).
		nl := t.fr.MaxLines / 4
		if t.w.nopen[qWevent] > 0 {
			nl = 3 * t.fr.MaxLines / 4
		}
		q := a.textbacknl(t, q0, nl)
		if !(q0 > t.org && q < t.org) {
			a.textsetorigin(t, q, true)
		}
		for q0 > t.org+t.fr.NumChars {
			// textbacknl positions the origin at most nl lines above q0, so after
			// the textsetorigin above q0 is always within the frame; q0 cannot be
			// both within that range of org and beyond the viewport.
			// unreachable
			a.textsetorigin(t, t.org+1, false)
		}
	}
}

// tagexpandwin expands w's tag to show all its text (text.c Tagdown).
func (a *Acme) tagexpandwin(w *Window) {
	if !w.tagexpand {
		w.tagexpand = true
		a.winresize(w, w.r, false, true)
	}
}

// tagshrinkwin shrinks w's tag back to a single line (text.c Tagup).
func (a *Acme) tagshrinkwin(w *Window) {
	if w.tagexpand {
		w.tagexpand = false
		w.taglines = 1
		a.winresize(w, w.r, false, true)
	}
}

// texttype handles one typed rune r into text t (text.c): cursor motion and
// scroll keys, the ^A/^E line motions, the %-command copy/undo/redo/cut/paste
// shortcuts, ESC, the ^H/^U/^W erasures, ^F/Insert completion, and newline.
func (a *Acme) texttype(t *Text, r rune) {
	var nr int
	var rp []rune
	if t.what != Body && t.what != Tag && r == '\n' {
		return
	}
	if t.what == Tag {
		t.w.tagsafe = false
	}
	nr = 1
	rp = []rune{r}
	switch r {
	case Kleft:
		a.typecommit(t)
		if t.q0 > 0 {
			a.textshow(t, t.q0-1, t.q0-1, true)
		}
		return
	case Kright:
		a.typecommit(t)
		if t.q1 < t.file.b.nc() {
			a.textshow(t, t.q1+1, t.q1+1, true)
		}
		return
	case Kdown:
		if t.what == Tag {
			a.tagexpandwin(t.w) // expand the tag to show all its text
			return
		}
		n := t.fr.MaxLines / 3
		a.textdown(t, n)
		return
	case Kpgdown:
		a.textdown(t, 2*t.fr.MaxLines/3)
		return
	case Kscrollonedown:
		if t.what == Tag {
			a.tagexpandwin(t.w)
			return
		}
		n := mousescrollsize(t.fr.MaxLines)
		if n <= 0 {
			// untested: mousescrollsize returns >=1 under -D ($mousescrollsize unset)
			n = 1
		}
		a.textdown(t, n)
		return
	case Kup:
		if t.what == Tag {
			a.tagshrinkwin(t.w) // shrink the tag to a single line
			return
		}
		a.textup(t, t.fr.MaxLines/3)
		return
	case Kpgup:
		a.textup(t, 2*t.fr.MaxLines/3)
		return
	case Kscrolloneup:
		if t.what == Tag {
			a.tagshrinkwin(t.w)
			return
		}
		a.textup(t, mousescrollsize(t.fr.MaxLines))
		return
	case Khome:
		a.typecommit(t)
		if t.org > t.iq1 {
			// scrolled past the last typing point: bring it back into view
			q0 := a.textbacknl(t, t.iq1, 1)
			a.textsetorigin(t, q0, true)
		} else {
			a.textshow(t, 0, 0, false)
		}
		return
	case Kend:
		a.typecommit(t)
		if t.iq1 > t.org+t.fr.NumChars {
			if t.iq1 > t.file.b.nc() {
				// untested: iq1 past EOF "should not happen but does" (text.c)
				t.iq1 = t.file.b.nc()
			}
			q0 := a.textbacknl(t, t.iq1, 1)
			a.textsetorigin(t, q0, true)
		} else {
			a.textshow(t, t.file.b.nc(), t.file.b.nc(), false)
		}
		return
	case 0x01: // ^A: beginning of line
		a.typecommit(t)
		// go to where ^U would erase, if not already at BOL
		nnb := 0
		if t.q0 > 0 && a.textreadc(t, t.q0-1) != '\n' {
			nnb = a.textbswidth(t, 0x15)
		}
		a.textshow(t, t.q0-nnb, t.q0-nnb, true)
		return
	case 0x05: // ^E: end of line
		a.typecommit(t)
		q0 := t.q0
		for q0 < t.file.b.nc() && a.textreadc(t, q0) != '\n' {
			q0++
		}
		a.textshow(t, q0, q0, true)
		return
	case Kcmd + 'c': // %C: copy
		a.typecommit(t)
		a.cut(t, t, nil, true, false, nil)
		return
	case Kcmd + 'z': // %Z: undo
		a.typecommit(t)
		a.undo(t, nil, nil, true, false, nil)
		return
	case Kcmd + 'Z': // %-shift-Z: redo
		a.typecommit(t)
		a.undo(t, nil, nil, false, false, nil)
		return
	}
	if t.what == Body {
		a.seq++
		a.filemark(t.file)
	}
	// cut/paste must be done after the seq++/filemark
	switch r {
	case Kcmd + 'x': // %X: cut
		a.typecommit(t)
		if t.what == Body {
			a.seq++
			a.filemark(t.file)
		}
		a.cut(t, t, nil, true, true, nil)
		a.textshow(t, t.q0, t.q0, true)
		t.iq1 = t.q0
		return
	case Kcmd + 'v': // %V: paste
		a.typecommit(t)
		if t.what == Body {
			a.seq++
			a.filemark(t.file)
		}
		a.paste(t, t, nil, true, false, nil)
		a.textshow(t, t.q0, t.q1, true)
		t.iq1 = t.q1
		return
	}
	if t.q1 > t.q0 {
		if t.ncache != 0 {
			// unreachable
			bug("text.type")
		}
		a.cut(t, t, nil, true, true, nil) // snarf + delete the selection
		t.eq0 = ^0
	}
	a.textshow(t, t.q0, t.q0, true)
	switch r {
	case Kesc:
		if t.eq0 != ^0 {
			if t.eq0 <= t.q0 {
				a.textsetselect(t, t.eq0, t.q0)
			} else {
				// eq0 marks where typing began, so it is <= the dot unless the
				// user backspaced past it before pressing ESC.
				a.textsetselect(t, t.q0, t.eq0)
			}
		}
		if t.ncache > 0 {
			a.typecommit(t)
		}
		t.iq1 = t.q0
		return
	case Kbs, Knack, Ketb: // ^H erase char, ^U erase line, ^W erase word
		if t.q0 == 0 {
			return
		}
		nnb := a.textbswidth(t, r)
		q1 := t.q0
		q0 := q1 - nnb
		if q0 < t.org { // erasing past the top of a scrolled frame
			q0 = t.org
			nnb = q1 - q0
		}
		if nnb <= 0 {
			return
		}
		for _, u := range t.file.text {
			u.nofill = true
			nb := nnb
			n := u.ncache
			if n > 0 {
				if q1 != u.cq0+n {
					// unreachable
					bug("text.type backspace")
				}
				if n > nb {
					n = nb
				}
				u.ncache -= n
				a.textdelete(u, q1-n, q1, false)
				nb -= n
			}
			if u.eq0 == q1 || u.eq0 == ^0 {
				u.eq0 = q0
			}
			if nb != 0 && u == t {
				a.textdelete(u, q0, q0+nb, true)
			}
			if u != t {
				a.textsetselect(u, u.q0, u.q1)
			} else {
				a.textsetselect(t, q0, q0)
			}
			u.nofill = false
		}
		for _, u := range t.file.text {
			a.textfill(u)
		}
		t.iq1 = t.q0
		return
	case 0x06, Kins: // ^F or Insert: filename completion
		a.typecommit(t)
		rp = a.textcomplete(t)
		if rp == nil {
			return
		}
		nr = len(rp) // fall through to the ordinary insertion below
	case '\n':
		if t.w.autoindent {
			// copy the leading blanks of the previous line
			nnb := a.textbswidth(t, Knack)
			rp = []rune{r}
			for i := 0; i < nnb; i++ {
				c := a.textreadc(t, t.q0-nnb+i)
				if c != ' ' && c != '\t' {
					break
				}
				rp = append(rp, c)
			}
			nr = len(rp)
		}
	}
	// ordinary character; insert into the caches of all texts
	for _, u := range t.file.text {
		if u.eq0 == ^0 {
			u.eq0 = t.q0
		}
		if u.ncache == 0 {
			u.cq0 = t.q0
		} else if t.q0 != u.cq0+u.ncache {
			// unreachable
			bug("text.type cq1")
		}
		// change the tag before adding to ncache so a resize-driven commit
		// won't find anything in ncache
		if u.what == Body && u.ncache == 0 {
			u.needundo = true
			a.winsettag(t.w)
			u.needundo = false
		}
		a.textinsert(u, t.q0, rp[:nr], false)
		if u != t {
			a.textsetselect(u, u.q0, u.q1)
		}
		u.cache = append(u.cache[:u.ncache], rp[:nr]...)
		u.ncache += nr
	}
	a.textsetselect(t, t.q0+nr, t.q0+nr)
	if r == '\n' && t.w != nil {
		a.wincommit(t.w, t)
	}
	t.iq1 = t.q0
}

func (a *Acme) textdown(t *Text, n int) {
	q0 := t.org + t.fr.CharOf(draw.Pt(t.fr.R.Min.X, t.fr.R.Min.Y+n*t.fr.Font.Height))
	a.textsetorigin(t, q0, true)
}

func (a *Acme) textup(t *Text, n int) {
	q0 := a.textbacknl(t, t.org, n)
	a.textsetorigin(t, q0, true)
}

// mousescrollsize returns how many lines a one-notch mouse-wheel scroll moves,
// given the window's maxlines (libdraw scroll.c). $mousescrollsize is a line
// count, or a percentage of the window ("N%"); unset means one line.
func mousescrollsize(maxlines int) int {
	mss := os.Getenv("mousescrollsize")
	if mss == "" {
		return 1
	}
	if i := strings.IndexByte(mss, '%'); i >= 0 {
		pcnt, _ := strconv.Atoi(mss[:i])
		if pcnt >= 100 {
			pcnt = 100
		}
		return pcnt * maxlines / 100
	}
	if lines, err := strconv.Atoi(mss); err == nil && lines != 0 {
		return lines
	}
	return 1
}

// wintype types rune r into text t of window w and updates the tag (wind.c).
func (a *Acme) wintype(w *Window, t *Text, r rune) {
	a.texttype(t, r)
	if t.what == Body {
		for _, u := range t.file.text {
			a.textscrdraw(u)
		}
	}
	a.winsettag(w)
}

// --- HTML double-click matching helpers (text.c) ---

// ishtmlstart reports whether the text starting at q is an HTML tag: 1 for
// <a>, -1 for </a>, 0 for no tag or <a/>. If q1 is non-nil it is set to the
// location after the tag (text.c).
func (a *Acme) ishtmlstart(t *Text, q int, q1 *int) int {
	nc := t.file.b.nc()
	if q+2 > nc {
		return 0
	}
	if a.textreadc(t, q) != '<' {
		return 0
	}
	q++
	c := a.textreadc(t, q)
	q++
	c1 := c
	c2 := c
	for c != '>' {
		if q >= nc {
			return 0
		}
		c2 = c
		c = a.textreadc(t, q)
		q++
	}
	if q1 != nil {
		*q1 = q
	}
	if c1 == '/' { // closing tag
		return -1
	}
	if c2 == '/' || c2 == '!' { // open + close tag or comment
		return 0
	}
	return 1
}

// ishtmlend reports whether the text ending at q is an HTML tag: 1 for <a>, -1
// for </a>, 0 for no tag or <a/>. If q0 is non-nil it is set to the start of
// the tag (text.c).
func (a *Acme) ishtmlend(t *Text, q int, q0 *int) int {
	if q < 2 {
		return 0
	}
	q--
	if a.textreadc(t, q) != '>' {
		return 0
	}
	q--
	c := a.textreadc(t, q)
	c1 := c
	c2 := c
	for c != '<' {
		if q == 0 {
			return 0
		}
		c1 = c
		q--
		c = a.textreadc(t, q)
	}
	if q0 != nil {
		*q0 = q
	}
	if c1 == '/' { // closing tag
		return -1
	}
	if c2 == '/' || c2 == '!' { // open + close tag or comment
		return 0
	}
	return 1
}

// textclickhtmlmatch handles double-click just inside an HTML tag pair: after an
// opening tag it scans forward for the matching close (setting *q1), before a
// closing tag it scans backward for the matching open (setting *q0). It reports
// whether a match was found (text.c).
func (a *Acme) textclickhtmlmatch(t *Text, q0, q1 *int) bool {
	nc := t.file.b.nc()
	q := *q0
	// after an opening tag? scan forward for the closing tag
	if a.ishtmlend(t, q, nil) == 1 {
		depth := 1
		for q < nc {
			var nq int
			n := a.ishtmlstart(t, q, &nq)
			if n != 0 {
				depth += n
				if depth == 0 {
					*q1 = q
					return true
				}
				q = nq
				continue
			}
			q++
		}
	}
	// before a closing tag? scan backward for the opening tag
	if a.ishtmlstart(t, q, nil) == -1 {
		depth := -1
		for q > 0 {
			var nq int
			n := a.ishtmlend(t, q, &nq)
			if n != 0 {
				depth += n
				if depth == 0 {
					*q0 = q
					return true
				}
				q = nq
				continue
			}
			q--
		}
	}
	return false
}

// winevent reports an insert ('I'/'i', r!=nil) or delete ('D'/'d', r==nil) to
// the window's event file (text.c textinsert/textdelete).
func (a *Acme) winevent(w *Window, c byte, q0, q1 int, r []rune) {
	if w.nopen[qWevent] == 0 {
		return
	}
	if r != nil && len(r) <= EVENTSIZE {
		a.winappend(w, fmt.Sprintf("%c%d %d 0 %d %s\n", c, q0, q1, len(r), string(r)))
	} else {
		a.winappend(w, fmt.Sprintf("%c%d %d 0 0 \n", c, q0, q1))
	}
}

// textscrdraw redraws the scroll bar (text.c); a no-op until scrl.c is ported.
// scrpos returns the scrollbar thumb rectangle for showing [p0,p1) of tot in
// the scrollbar rectangle r (scrl.c).
func scrpos(r draw.Rectangle, p0, p1, tot int) draw.Rectangle {
	q := r
	h := q.Max.Y - q.Min.Y
	if tot == 0 {
		return q
	}
	if tot > 1024*1024 {
		// untested: -D never builds a buffer larger than a megabyte
		tot >>= 10
		p0 >>= 10
		p1 >>= 10
	}
	if p0 > 0 {
		q.Min.Y += h * p0 / tot
	}
	if p1 < tot {
		q.Max.Y -= h * (tot - p1) / tot
	}
	if q.Max.Y < q.Min.Y+2 {
		if q.Min.Y+2 <= r.Max.Y {
			q.Max.Y = q.Min.Y + 2
		} else {
			q.Min.Y = q.Max.Y - 2
		}
	}
	return q
}

// scrlresize allocates the scratch image used for scrollbar drawing (scrl.c).
func (a *Acme) scrlresize() {
	img, err := a.disp.AllocImage(draw.Rect(0, 0, 32, a.screen.R.Max.Y), a.screen.Pix, false, draw.NoFill)
	if err != nil {
		// untested: the in-memory test display does not fail image allocation
		a.fatalf("scroll alloc")
	}
	a.scrtmp = img
}

// textselect tracks a button-1 sweep in t: it double-clicks immediately if the
// gesture repeats a click in place, runs frselect to follow the drag (with
// framescroll for auto-scroll past the edges), then handles 1+2 / 1+3 chord
// cut/paste while the button stays down (text.c). Driven by a.mousectl, which
// under -D is the scripted synthetic mouse (driver.go m1).
func (a *Acme) textselect(t *Text) {
	const (
		selNone = iota
		selCut
		selPaste
	)
	mc := a.mousectl
	a.selecttext = t
	// To have double-clicking and chording, double-click immediately if it
	// might make sense.
	b := mc.Buttons
	q0 := t.q0
	q1 := t.q1
	a.selectq = t.org + t.fr.CharOf(mc.Point)
	if a.clicktext == t && mc.Msec-a.clickmsec < 500 && q0 == q1 && a.selectq == q0 {
		a.textdoubleclick(t, &q0, &q1)
		a.textsetselect(t, q0, q1)
		a.disp.Flush()
		x := mc.Point.X
		y := mc.Point.Y
		// stay here until something interesting happens
		for {
			mc.Read()
			if mc.Buttons != b || absInt(mc.Point.X-x) >= 3 || absInt(mc.Point.Y-y) >= 3 {
				break
			}
		}
		mc.Point.X = x // in case we're calling frselect
		mc.Point.Y = y
		q0 = t.q0 // may have changed
		q1 = t.q1
		a.selectq = q0
	}
	if mc.Buttons == b {
		t.fr.Scroll = a.framescroll
		t.fr.Select(mc)
		// horrible botch: while asleep, may have lost selection altogether
		if a.selectq > t.file.b.nc() {
			// unreachable: this guards selectq landing past the buffer end
			// because the buffer shrank during the sweep (a concurrent edit);
			// under -D nothing edits the buffer mid-drag, so selectq stays valid.
			a.selectq = t.org + t.fr.P0
		}
		t.fr.Scroll = nil
		if a.selectq < t.org {
			// the anchor scrolled above the origin (drag-scroll up): text_seldragdown
			q0 = a.selectq
		} else {
			q0 = t.org + t.fr.P0
		}
		if a.selectq > t.org+t.fr.NumChars {
			// the anchor scrolled below the view (drag-scroll up past it):
			// text_seldragupstop
			q1 = a.selectq
		} else {
			q1 = t.org + t.fr.P1
		}
	}
	if q0 == q1 {
		if q0 == t.q0 && a.clicktext == t && mc.Msec-a.clickmsec < 500 {
			// unreachable: clicktext is set only by a point-click (which leaves
			// q0==q1), so a same-spot repeat triggers the entry double-click above
			// before reaching here; the two conditions never co-occur.
			a.textdoubleclick(t, &q0, &q1)
			a.clicktext = nil
		} else {
			a.clicktext = t
			a.clickmsec = mc.Msec
		}
	} else {
		a.clicktext = nil
	}
	a.textsetselect(t, q0, q1)
	a.disp.Flush()
	state := selNone // what we've done; undo when possible
	for mc.Buttons != 0 {
		mc.Msec = 0
		b = mc.Buttons
		if b&1 != 0 && b&6 != 0 {
			if state == selNone && t.what == Body {
				a.seq++
				a.filemark(t.w.body.file)
			}
			if b&2 != 0 {
				if state == selPaste && t.what == Body {
					a.winundo(t.w, true)
					a.textsetselect(t, q0, t.q1)
					state = selNone
				} else if state != selCut {
					a.cut(t, t, nil, true, true, nil)
					state = selCut
				}
			} else {
				if state == selCut && t.what == Body {
					a.winundo(t.w, true)
					a.textsetselect(t, q0, t.q1)
					state = selNone
				} else if state != selPaste {
					a.paste(t, t, nil, true, false, nil)
					state = selPaste
				}
			}
			a.textscrdraw(t)
			a.clearmouse()
		}
		a.disp.Flush()
		for mc.Buttons == b {
			mc.Read()
		}
		a.clicktext = nil
	}
}

// framescroll is the frselect scroll callback: scroll selecttext by dl lines
// while a sweep drags past the frame edge (text.c).
func (a *Acme) framescroll(f *frame.Frame, dl int) {
	if f != &a.selecttext.fr {
		// unreachable
		bug("frameselect not right frame")
	}
	a.textframescroll(a.selecttext, dl)
}

// textframescroll scrolls t by dl lines mid-sweep and extends the selection to
// the newly exposed text, anchored at selectq (text.c).
func (a *Acme) textframescroll(t *Text, dl int) {
	if dl == 0 {
		// The auto-repeat throttle. frame.Select does not read the mouse while
		// it is scrolling (mp stays outside the frame); like C's scrsleep(100)
		// this is where the held drag/release is consumed, so the loop can see
		// the mouse move back in or the button release and stop.
		a.scrsleep(100)
		return
	}
	var q0 int
	if dl < 0 {
		q0 = a.textbacknl(t, t.org, -dl)
		if a.selectq > t.org+t.fr.P0 {
			a.textsetselect(t, t.org+t.fr.P0, a.selectq)
		} else {
			a.textsetselect(t, a.selectq, t.org+t.fr.P0)
		}
	} else {
		if t.org+t.fr.NumChars == t.file.b.nc() {
			return
		}
		q0 = t.org + t.fr.CharOf(draw.Pt(t.fr.R.Min.X, t.fr.R.Min.Y+dl*t.fr.Font.Height))
		if a.selectq > t.org+t.fr.P1 {
			a.textsetselect(t, t.org+t.fr.P1, a.selectq)
		} else {
			a.textsetselect(t, a.selectq, t.org+t.fr.P1)
		}
	}
	a.textsetorigin(t, q0, true)
}

func absInt(n int) int {
	if n < 0 {
		return -n
	}
	return n
}

// Release the button within DELAY ms having moved less than MINMOVE and it is a
// null (point) selection, not a sweep (text.c).
const (
	selDELAY   = 2
	selMINMOVE = 4
)

// region reports the sign of a-b (text.c).
func region(a, b int) int {
	if a < b {
		return -1
	}
	if a == b {
		return 0
	}
	return 1
}

// selrestore repaints [p0,p1) (starting at pt0) in selected or unselected colors
// according to its overlap with the frame's current selection (text.c).
func selrestore(f *frame.Frame, pt0 draw.Point, p0, p1 int) {
	if p1 <= f.P0 || p0 >= f.P1 {
		// no overlap
		f.Drawsel0(pt0, p0, p1, f.Cols[frame.BACK], f.Cols[frame.TEXT])
		return
	}
	if p0 >= f.P0 && p1 <= f.P1 {
		// entirely inside
		f.Drawsel0(pt0, p0, p1, f.Cols[frame.HIGH], f.Cols[frame.HTEXT])
		return
	}
	// they now are known to overlap
	if p0 < f.P0 { // before selection
		f.Drawsel0(pt0, p0, f.P0, f.Cols[frame.BACK], f.Cols[frame.TEXT])
		p0 = f.P0
		pt0 = f.PointOf(p0)
	}
	if p1 > f.P1 { // after selection
		f.Drawsel0(f.PointOf(f.P1), f.P1, p1, f.Cols[frame.BACK], f.Cols[frame.TEXT])
		p1 = f.P1
	}
	f.Drawsel0(pt0, p0, p1, f.Cols[frame.HIGH], f.Cols[frame.HTEXT]) // inside selection
}

// xselect tracks a button-2/3 sweep in frame f (button is down on entry),
// highlighting with col, and returns the selected [p0,p1) (text.c). Unlike
// frselect it does not scroll; it ticks/untacks and uses selrestore.
func (a *Acme) xselect(f *frame.Frame, mc *draw.Mousectl, col *draw.Image) (rp0, rp1 int) {
	mp := mc.Point
	b := mc.Buttons
	msec := mc.Msec

	// remove tick
	if f.P0 == f.P1 {
		f.Tick(f.PointOf(f.P0), false)
	}
	p0 := f.CharOf(mp)
	p1 := p0
	pt0 := f.PointOf(p0)
	pt1 := f.PointOf(p1)
	reg := 0
	f.Tick(pt0, true)
	for {
		q := f.CharOf(mc.Point)
		if p1 != q {
			if p0 == p1 {
				f.Tick(pt0, false)
			}
			if reg != region(q, p0) { // crossed starting point; reset
				if reg > 0 {
					selrestore(f, pt0, p0, p1)
				} else if reg < 0 {
					selrestore(f, pt1, p1, p0)
				}
				p1 = p0
				pt1 = pt0
				reg = region(q, p0)
				if reg == 0 {
					f.Drawsel0(pt0, p0, p1, col, a.disp.White)
				}
			}
			qt := f.PointOf(q)
			if reg > 0 {
				if q > p1 {
					f.Drawsel0(pt1, p1, q, col, a.disp.White)
				} else if q < p1 {
					selrestore(f, qt, q, p1)
				}
			} else if reg < 0 {
				if q > p1 {
					selrestore(f, pt1, p1, q)
				} else {
					f.Drawsel0(qt, q, p1, col, a.disp.White)
				}
			}
			p1 = q
			pt1 = qt
		}
		if p0 == p1 {
			f.Tick(pt0, true)
		}
		a.disp.Flush()
		mc.Read()
		if mc.Buttons != b {
			break
		}
	}
	if mc.Msec-msec < selDELAY && p0 != p1 &&
		absInt(mp.X-mc.Point.X) < selMINMOVE && absInt(mp.Y-mc.Point.Y) < selMINMOVE {
		if reg > 0 {
			selrestore(f, pt0, p0, p1)
		} else if reg < 0 {
			selrestore(f, pt1, p1, p0)
		}
		p1 = p0
	}
	if p1 < p0 {
		p0, p1 = p1, p0
	}
	pt0 = f.PointOf(p0)
	if p0 == p1 {
		f.Tick(pt0, false)
	}
	selrestore(f, pt0, p0, p1)
	// restore tick
	if f.P0 == f.P1 {
		f.Tick(f.PointOf(f.P0), true)
	}
	a.disp.Flush()
	return p0, p1
}

// textselect23 runs an xselect sweep for button 2 or 3, returning the swept
// range (in buffer coordinates) and the button state that ended it; mask names
// the buttons that cancel the action (text.c).
func (a *Acme) textselect23(t *Text, high *draw.Image, mask int) (q0, q1, buts int) {
	p0, p1 := a.xselect(&t.fr, a.mousectl, high)
	buts = a.mousectl.Buttons
	if buts&mask == 0 {
		q0 = p0 + t.org
		q1 = p1 + t.org
	}
	for a.mousectl.Buttons != 0 {
		a.mousectl.Read()
	}
	return q0, q1, buts
}

// textselect2 is the button-2 sweep: returns the range to execute and, for a
// 2-1 chord, the argument text; reports false if cancelled by button 3 (text.c).
func (a *Acme) textselect2(t *Text) (q0, q1 int, tp *Text, ok bool) {
	q0, q1, buts := a.textselect23(t, a.but2col, 4)
	if buts&4 != 0 {
		return q0, q1, nil, false
	}
	if buts&1 != 0 { // pick up argument
		return q0, q1, a.argtext, true
	}
	return q0, q1, nil, true
}

// textselect3 is the button-3 sweep: returns the range to look up, reporting
// false if cancelled by button 1 or 2 (text.c).
func (a *Acme) textselect3(t *Text) (q0, q1 int, ok bool) {
	q0, q1, buts := a.textselect23(t, a.but3col, 1|2)
	return q0, q1, buts == 0
}

// textscrdraw redraws t's scrollbar thumb to reflect the visible range (scrl.c).
func (a *Acme) textscrdraw(t *Text) {
	if t.w == nil || t != &t.w.body {
		// untested: textscrdraw is only ever called on a window's body
		return
	}
	if a.scrtmp == nil {
		a.scrlresize()
	}
	r := t.scrollr
	b := a.scrtmp
	r1 := r
	r1.Min.X = 0
	r1.Max.X = r.Dx()
	r2 := scrpos(r1, t.org, t.org+t.fr.NumChars, t.file.b.nc())
	if r2 != t.lastsr {
		t.lastsr = r2
		b.Draw(r1, t.fr.Cols[frame.BORD], nil, draw.ZP)
		b.Draw(r2, t.fr.Cols[frame.BACK], nil, draw.ZP)
		r2.Min.X = r2.Max.X - 1
		b.Draw(r2, t.fr.Cols[frame.BORD], nil, draw.ZP)
		t.fr.B.Draw(r, b, nil, draw.Pt(0, r1.Min.Y))
	}
}

// textcommit flushes the typing cache into the file at cq0 (text.c).
func (a *Acme) textcommit(t *Text, tofile bool) {
	if t.ncache == 0 {
		return
	}
	if tofile {
		a.fileinsert(t.file, t.cq0, t.cache[:t.ncache])
	}
	if t.what == Body {
		t.w.dirty = true
		t.w.utflastqid = -1
	}
	t.ncache = 0
}

// typecommit flushes the typing cache (text.c). The tag-name update done by
// wincommit for tag texts is added when wind.c is ported; for body texts and
// detached texts this is the complete behavior.
func (a *Acme) typecommit(t *Text) {
	if t.w != nil {
		a.wincommit(t.w, t)
	} else {
		a.textcommit(t, true)
	}
}

// wincommit commits typing in t; for a tag, if the file-name field of the tag
// changed it renames the window's body to match (wind.c).
func (a *Acme) wincommit(w *Window, t *Text) {
	a.textcommit(t, true)
	f := t.file
	if len(f.text) > 1 {
		for _, u := range f.text {
			a.textcommit(u, false) // no-op for t
		}
	}
	if t.what == Body {
		return
	}
	r, i := a.parsetag(w, 0)
	if !runeeqRange(r, i, w.body.file.name, w.body.file.nname()) {
		a.seq++
		a.filemark(w.body.file)
		w.body.file.mod = true
		w.dirty = true
		a.winsetname(w, r[:i])
		a.winsettag(w)
	}
}

// textfilewidth returns the number of runes back from q0 forming a file name,
// or one path element when oneelement, stopping at white space (text.c).
func (a *Acme) textfilewidth(t *Text, q0 int, oneelement bool) int {
	q := q0
	for q > 0 {
		r := a.textreadc(t, q-1)
		if r <= ' ' {
			break
		}
		if oneelement && r == '/' {
			break
		}
		q--
	}
	return q0 - q
}

// textcomplete performs ^F filename completion at dot, returning the runes to
// insert, or nil (text.c).
func (a *Acme) textcomplete(t *Text) []rune {
	// must be at the end of a word
	if t.q0 < t.file.b.nc() && a.textreadc(t, t.q0) > ' ' {
		return nil
	}
	nstr := a.textfilewidth(t, t.q0, true)
	npath := a.textfilewidth(t, t.q0-nstr, false)
	str := make([]rune, nstr)
	q := t.q0 - nstr
	for i := 0; i < nstr; i++ {
		str[i] = a.textreadc(t, q)
		q++
	}
	path := make([]rune, npath)
	q = t.q0 - nstr - npath
	for i := 0; i < npath; i++ {
		path[i] = a.textreadc(t, q)
		q++
	}
	// is the path rooted? if not, make it relative to the window's directory
	var dir []rune
	if npath > 0 && path[0] == '/' {
		// untested: an absolute completion path can't be scripted deterministically
		// (the test working directory's absolute name is unknown to the script)
		dir = path
	} else {
		dir = a.dirname(t, nil)
		if len(dir)+1+npath > 200 {
			// C bounds dir+/+path by a fixed tmp[200] and gives up if it won't
			// fit (text.c textcomplete); match that limit.
			// untested: -D completion paths are far shorter than 200 runes
			return nil
		}
		if len(dir) == 0 {
			dir = []rune(".")
		}
		nd := append([]rune(nil), dir...)
		nd = append(nd, '/')
		nd = append(nd, path...)
		dir = cleanrname(nd)
	}
	c, err := acomplete(string(dir), string(str))
	if c == nil {
		// C's complete() opens the directory; on failure the caller reports %r.
		a.warning(nil, "error attempting completion: %s\n", errstr(err))
		return nil
	}
	if !c.advance {
		sep := ""
		if len(dir) > 0 && dir[len(dir)-1] != '/' {
			sep = "/"
		}
		nomatch := ""
		if c.nmatch == 0 {
			nomatch = ": no matches in:"
		}
		a.warning(nil, "%s%s%s*%s\n", string(dir), sep, string(str), nomatch)
		for _, fn := range c.filename {
			a.warning(nil, " %s\n", fn)
		}
	}
	if c.advance {
		return []rune(c.str)
	}
	return nil
}

// completion is libcomplete's Completion (complete.c).
type completion struct {
	advance  bool
	complete bool
	str      string
	nmatch   int
	filename []string
}

// acomplete completes name s within directory dir, returning the common-prefix
// advance and the candidate list (libcomplete complete.c).
func acomplete(dir, s string) (*completion, error) {
	if strings.Contains(s, "/") {
		// untested: textcomplete passes one path element (no slash) as s
		return nil, nil
	}
	ents, err := os.ReadDir(dir)
	if err != nil {
		return nil, err
	}
	if len(ents) == 0 {
		// C's complete() treats an empty directory (dirreadall n<=0, complete.c:61)
		// as a failure and returns nil, so textcomplete reports "error attempting
		// completion" rather than "no matches in:". C's %r is whatever errstr held
		// (undefined after an empty read); Go gives a definite message.
		// untested: -D completion directories always contain entries
		return nil, fmt.Errorf("empty directory")
	}
	c := &completion{}
	var names []string
	var isdir []bool
	minlen := 1 << 30
	for _, e := range ents {
		if strings.HasPrefix(e.Name(), s) {
			names = append(names, e.Name())
			isdir = append(isdir, e.IsDir())
			if len(e.Name()) < minlen {
				minlen = len(e.Name())
			}
		}
	}
	if len(names) > 0 {
		for i := 1; i < len(names); i++ {
			minlen = commonPrefixLen(names[0], names[i], minlen)
		}
		c.complete = len(names) == 1
		c.advance = c.complete || minlen > len(s)
		c.str = names[0][len(s):minlen]
		if c.complete {
			if isdir[0] {
				// untested: -D cannot create a subdirectory to complete to
				c.str += "/"
			} else {
				c.str += " "
			}
		}
		c.nmatch = len(names)
		fns := make([]string, len(names))
		for i, n := range names {
			if isdir[i] {
				// untested: -D cannot create a subdirectory in the completion dir
				fns[i] = n + "/"
			} else {
				fns[i] = n
			}
		}
		sort.Strings(fns)
		c.filename = fns
	} else {
		fns := make([]string, len(ents))
		for i, e := range ents {
			if e.IsDir() {
				// untested: -D cannot create a subdirectory in the completion dir
				fns[i] = e.Name() + "/"
			} else {
				fns[i] = e.Name()
			}
		}
		sort.Strings(fns)
		c.filename = fns
		c.nmatch = 0
	}
	return c, nil
}

// commonPrefixLen returns the length of the common byte prefix of a and b,
// capped at n (libcomplete longestprefixlength).
func commonPrefixLen(a, b string, n int) int {
	i := 0
	for i < n && i < len(a) && i < len(b) && a[i] == b[i] {
		i++
	}
	return i
}
