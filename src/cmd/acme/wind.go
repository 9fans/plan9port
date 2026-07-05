package main

import (
	"os"

	"9fans.net/plan9port/draw"
	"9fans.net/plan9port/draw/frame"
)

// Window layout (wind.c). Reference counting and the mouse-cursor nudging in
// the C version are dropped (GC; no live mouse in -D); graphical-only helpers
// (windrawbutton, textscrdraw) are no-ops that don't affect the textual dump.

// rfget returns the Reffont named name (loading and caching it on first use),
// or the default var/fix font when name is empty (acme.c rfget). The C
// reference counting is dropped (GC). With save it becomes the var/fix default;
// with setfont it becomes the global font.
func (a *Acme) rfget(fix, save, setfont bool, name string) *Reffont {
	idx := 0
	if fix {
		idx = 1
	}
	var r *Reffont
	if name == "" {
		name = a.fontnames[idx]
		r = a.reffonts[idx]
	}
	if r == nil {
		for _, fc := range a.fontcache {
			if fc.f.Name == name {
				r = fc
				break
			}
		}
	}
	if r == nil {
		f, err := a.disp.OpenFont(name)
		if err != nil {
			a.warning(nil, "can't open font file %s: %s\n", name, errstr(err))
			return nil
		}
		r = &Reffont{f: f}
		a.fontcache = append(a.fontcache, r)
	}
	if save {
		a.reffonts[idx] = r
		a.fontnames[idx] = name
	}
	if setfont {
		// untested: -D never makes a font the global default
		a.reffont.f = r.f
		a.font = r.f
		a.reffonts[0] = r
		a.iconinit()
	}
	return r
}

// wininit sets up a new window's tag and body within rectangle r (wind.c),
// including the clone (zerox) path that shares the cloned window's body file.
func (a *Acme) wininit(w *Window, clone *Window, r draw.Rectangle) {
	w.tag.w = w
	w.taglines = 1
	w.tagexpand = true
	w.body.w = w
	w.filemenu = true
	a.winid++
	w.id = a.winid
	w.ref.Add(1) // wininit holds the base reference (wind.c incref)
	if a.globalincref {
		// Set during the X/Y alllocker dance (ecmd.c filelooper): a window created
		// while an X/Y loop is running starts with the extra ref the loop holds on
		// every window, so the loop's balancing winclose pass leaves it at the base
		// reference. untested: -D never creates a window mid-X/Y-loop.
		// untested: window edge path not exercised by -D
		w.ref.Add(1)
	}
	// w.ctlfid nil == C's ctlfid==~0 (no exclusive ctl-lock holder); w.ctllock is
	// this window's exclusive ctl lock (C's per-window QLock), zero-value ready.
	w.utflastqid = -1

	w.tagtop = r
	w.tagtop.Max.Y = r.Min.Y + a.font.Height
	r1 := r
	r1.Max.Y = r1.Min.Y + w.taglines*a.font.Height

	f := fileaddtext(nil, &w.tag)
	a.textinit(&w.tag, f, r1, &a.reffont, a.tagcols)
	w.tag.what = Tag
	// tag is a copy of the contents, not a tracked image
	if clone != nil {
		a.textdelete(&w.tag, 0, w.tag.file.b.nc(), true)
		nc := clone.tag.file.b.nc()
		rp := make([]rune, nc)
		clone.tag.file.b.Read(0, rp)
		a.textinsert(&w.tag, 0, rp, true)
		filereset(w.tag.file)
		a.textsetselect(&w.tag, nc, nc)
	}

	r1 = r
	r1.Min.Y += w.taglines*a.font.Height + 1
	if r1.Max.Y < r1.Min.Y {
		r1.Max.Y = r1.Min.Y
	}
	rf := a.rfget(false, false, false, "")
	var bf *File
	if clone != nil {
		bf = clone.body.file // share the body file with the cloned window
		w.body.org = clone.body.org
		w.isscratch = clone.isscratch
	}
	bf = fileaddtext(bf, &w.body)
	w.body.what = Body
	a.textinit(&w.body, bf, r1, rf, a.textcols)
	r1.Min.Y -= 1
	r1.Max.Y = r1.Min.Y + 1
	a.screen.Draw(r1, a.tagcols[frame.BORD], nil, draw.ZP)
	a.textscrdraw(&w.body)
	w.r = r
	var br draw.Rectangle
	br.Min = w.tag.scrollr.Min
	br.Max.X = br.Min.X + a.button.R.Dx()
	br.Max.Y = br.Min.Y + a.button.R.Dy()
	a.screen.Draw(br, a.button, nil, a.button.R.Min)
	w.maxlines = w.body.fr.MaxLines
	w.autoindent = a.globalautoindent // new windows inherit the global Indent setting (wind.c)
	if clone != nil {
		w.dirty = clone.dirty
		w.autoindent = clone.autoindent
		a.textsetselect(&w.body, clone.body.q0, clone.body.q1)
		a.winsettag(w)
	}
}

// winsetname sets the body file's name and updates the tag (wind.c).
// trimname removes the -T prefix from name (the temp-root path), so a window
// opened by absolute path under the root shows (and stores) a stable name
// relative to the root. Names outside the root are left unchanged.
func (a *Acme) trimname(name []rune) []rune {
	p := a.trimpre
	if len(p) > 0 && len(name) > len(p) && runeEq(name[:len(p)], p) && name[len(p)] == '/' {
		return name[len(p)+1:]
	}
	return name
}

func (a *Acme) winsetname(w *Window, name []rune) {
	name = a.trimname(name)
	t := &w.body
	if runeEq(t.file.name, name) {
		return
	}
	w.isscratch = false
	if n := len(name); (n >= 6 && string(name[n-6:]) == "/guide") ||
		(n >= 7 && string(name[n-7:]) == "+Errors") {
		w.isscratch = true
	}
	a.filesetname(t.file, name)
	for _, x := range t.file.text {
		v := x.w
		a.winsettag(v)
		v.isscratch = w.isscratch
	}
}

// parsetag returns the tag's runes and the length of the file-name prefix
// (wind.c).
func (a *Acme) parsetag(w *Window, extra int) (r []rune, length int) {
	// Reserve `extra` (+1) spare capacity so callers like dirname can splice a
	// relative name onto the end (C runemalloc(nc+extra+1)).
	nc := w.tag.file.b.nc()
	r = make([]rune, nc, nc+extra+1)
	w.tag.file.b.Read(0, r)
	// " |" or "\t|" ends the left half of the tag; " Del Snarf" before the
	// pipe also ends the file name.
	pipe := runeStrStr(r, " |")
	if p := runeStrStr(r, "\t|"); p >= 0 && (pipe < 0 || p < pipe) {
		// untested: a tab before the pipe only appears in a hand-mangled tag.
		pipe = p
	}
	if p := runeStrStr(r, " Del Snarf"); p >= 0 && (pipe < 0 || p < pipe) {
		length = p
	} else {
		// -D windows always carry the standard " Del Snarf" menu, so the
		// first-whitespace fallback is not reached.
		// untested
		length = len(r)
		for i := 0; i < len(r); i++ {
			// untested
			if r[i] == ' ' || r[i] == '\t' {
				// untested
				length = i
				break
			}
		}
	}
	return r, length
}

// winaddincl prepends directory r to w's include search list (wind.c). A
// relative name is resolved against the window's directory; a non-directory is
// rejected with a warning.
func (a *Acme) winaddincl(w *Window, r []rune) {
	name := string(r)
	fi, err := os.Stat(name)
	if err != nil {
		if len(r) > 0 && r[0] == '/' {
			a.warning(nil, "%s: %s\n", name, errstr(err))
			return
		}
		r = a.dirname(&w.body, r)
		name = string(r)
		fi, err = os.Stat(name)
		if err != nil {
			a.warning(nil, "%s: %s\n", name, errstr(err))
			return
		}
	}
	if !fi.IsDir() {
		a.warning(nil, "%s: not a directory\n", name)
		return
	}
	w.incl = append([][]rune{append([]rune(nil), r...)}, w.incl...)
}

// winsettag updates the tags of all windows sharing this window's file (wind.c).
func (a *Acme) winsettag(w *Window) {
	f := w.body.file
	for _, x := range f.text {
		v := x.w
		if v.col.safe || v.body.fr.MaxLines > 0 {
			a.winsettag1(v)
		}
	}
}

// winsettag1 rebuilds window w's tag text (wind.c).
func (a *Acme) winsettag1(w *Window) {
	if w.tag.ncache != 0 || w.tag.file.mod {
		a.wincommit(w, &w.tag) // sync the tag cache and file name
	}
	old, i := a.parsetag(w, 0)
	if !runeeqRange(old, i, w.body.file.name, w.body.file.nname()) {
		a.textdelete(&w.tag, 0, i, true)
		a.textinsert(&w.tag, 0, w.body.file.name, true)
		old = make([]rune, w.tag.file.b.nc())
		w.tag.file.b.Read(0, old)
	}

	// compute the desired whole tag, replacing only if it differs
	var newt []rune
	newt = append(newt, w.body.file.name...)
	newt = append(newt, []rune(" Del Snarf")...)
	if w.filemenu {
		if w.body.needundo || len(w.body.file.delta) > 0 || w.body.ncache != 0 {
			newt = append(newt, []rune(" Undo")...)
		}
		if len(w.body.file.epsilon) > 0 {
			newt = append(newt, []rune(" Redo")...)
		}
		dirty := w.body.file.nname() != 0 && (w.body.ncache != 0 || w.body.file.seq != w.putseq)
		if !w.isdir && dirty {
			newt = append(newt, []rune(" Put")...)
		}
	}
	if w.isdir {
		newt = append(newt, []rune(" Get")...)
	}
	newt = append(newt, []rune(" |")...)
	var k int
	if r := runeIndexAll(old, '|'); r >= 0 {
		k = r + 1
	} else {
		k = w.tag.file.b.nc()
		if w.body.file.seq == 0 {
			newt = append(newt, []rune(" Look ")...)
		}
	}

	// replace the tag if the new one differs from the old left part
	i = len(newt)
	resize := false
	if !runeeqRange(newt, i, old, k) {
		resize = true
		n := k
		if n > i {
			n = i
		}
		j := 0
		for j < n && old[j] == newt[j] {
			j++
		}
		q0 := w.tag.q0
		q1 := w.tag.q1
		a.textdelete(&w.tag, j, k, true)
		a.textinsert(&w.tag, j, newt[j:i], true)
		// try to preserve the user's selection
		if r := runeIndexAll(old, '|'); r >= 0 {
			bar := r
			if q0 > bar {
				bar = runeIndexAll(newt, '|') - bar
				w.tag.q0 = q0 + bar
				w.tag.q1 = q1 + bar
			}
		}
	}
	w.tag.file.mod = false
	n := w.tag.file.b.nc() + w.tag.ncache
	if w.tag.q0 > n {
		// untested: the tag only shrinks below the selection on Put, not ported.
		w.tag.q0 = n
	}
	if w.tag.q1 > n {
		// untested
		w.tag.q1 = n
	}
	a.textsetselect(&w.tag, w.tag.q0, w.tag.q1)
	a.windrawbutton(w)
	if resize {
		w.tagsafe = false
		a.winresize(w, w.r, true, true)
	}
}

// winresize lays out the tag and body of w within r, returning the bottom y
// (wind.c).
func (a *Acme) winresize(w *Window, r draw.Rectangle, safe, keepextra bool) int {
	w.tagtop = r
	w.tagtop.Max.Y = r.Min.Y + a.font.Height

	r1 := r
	r1.Max.Y = minInt(r.Max.Y, r1.Min.Y+w.taglines*a.font.Height)

	// Snapshot whether the cursor sits in the tag/body before the reflow, so a
	// shrinking tag pulls it up and an expanding tag pushes it down (-D mirrors
	// the live mouse position in a.mousexy; see moveto).
	mouseintag := a.mousexy.In(w.tag.all)
	mouseinbody := a.mousexy.In(w.body.all)

	if !safe || !w.tagsafe || !eqrect(w.tag.all, r1) {
		w.taglines = a.wintaglines(w, r)
		r1.Max.Y = minInt(r.Max.Y, r1.Min.Y+w.taglines*a.font.Height)
	}

	y := r1.Max.Y
	if !safe || !w.tagsafe || !eqrect(w.tag.all, r1) {
		a.textresize(&w.tag, r1, true)
		y = w.tag.fr.R.Max.Y
		a.windrawbutton(w)
		w.tagsafe = true

		// If mouse is in tag, pull up as tag closes.
		if mouseintag && !a.mousexy.In(w.tag.all) {
			p := a.mousexy
			p.Y = w.tag.all.Max.Y - 3
			a.moveto(p)
		}
		// If mouse is in body, push down as tag expands.
		if mouseinbody && a.mousexy.In(w.tag.all) {
			p := a.mousexy
			p.Y = w.tag.all.Max.Y + 3
			a.moveto(p)
		}
	}

	r1 = r
	r1.Min.Y = y
	if !safe || !eqrect(w.body.all, r1) {
		oy := y
		if y+1+w.body.fr.Font.Height <= r.Max.Y { // room for one line
			r1.Min.Y = y
			r1.Max.Y = y + 1
			a.screen.Draw(r1, a.tagcols[frame.BORD], nil, draw.ZP)
			y++
			r1.Min.Y = minInt(y, r.Max.Y)
			r1.Max.Y = r.Max.Y
		} else {
			a.screen.Draw(r1, a.textcols[frame.BACK], nil, draw.ZP)
			r1.Min.Y = y
			r1.Max.Y = y
		}
		y = a.textresize(&w.body, r1, keepextra)
		w.r = r
		w.r.Max.Y = y
		a.textscrdraw(&w.body)
		w.body.all.Min.Y = oy
	}
	w.maxlines = minInt(w.body.fr.NumLines, maxInt(w.maxlines, w.body.fr.MaxLines))
	return w.r.Max.Y
}

// delrunepos returns the rune position just past " Del" in w's tag (where the
// cursor lands to delete the window), or -1 if the tag has no room (wind.c).
func (a *Acme) delrunepos(w *Window) int {
	_, i := a.parsetag(w, 0)
	i += 2
	if i >= w.tag.file.b.nc() {
		// untested: -D tags always carry " Del Snarf", so there is room
		return -1
	}
	return i
}

// winmousebut warps the cursor onto the center of w's tag button (wind.c).
func (a *Acme) winmousebut(w *Window) {
	sr := w.tag.scrollr
	a.moveto(sr.Min.Add(draw.Pt(sr.Dx()/2, a.font.Height/2)))
}

// movetodel warps the cursor onto w's Del command in the tag (wind.c).
func (a *Acme) movetodel(w *Window) {
	n := a.delrunepos(w)
	if n < 0 {
		// untested: tag too short for a Del position
		return
	}
	a.moveto(w.tag.fr.PointOf(n).Add(draw.Pt(4, w.tag.fr.Font.Height-4)))
}

// wintaglines computes how many lines window w's tag needs (wind.c).
func (a *Acme) wintaglines(w *Window, r draw.Rectangle) int {
	if !w.tagexpand && !w.showdel {
		// a single-line (shrunk) tag, e.g. after Kup
		return 1
	}
	w.showdel = false
	w.tag.fr.NoRedraw = 1
	a.textresize(&w.tag, r, true)
	w.tag.fr.NoRedraw = 0
	w.tagsafe = false

	if !w.tagexpand {
		// collapsed tag: use just as many lines as needed to show the Del (wind.c)
		n := a.delrunepos(w)
		if n < 0 {
			// untested: the tag always carries a Del, so delrunepos >= 0
			return 1
		}
		p := w.tag.fr.PointOf(n).Sub(w.tag.fr.R.Min)
		return 1 + p.Y/w.tag.fr.Font.Height
	}
	if w.tag.fr.NumLines >= w.tag.fr.MaxLines {
		return w.tag.fr.MaxLines
	}
	// if tag ends with \n, include an empty line for typing
	n := w.tag.fr.NumLines
	if w.tag.file.b.nc() > 0 {
		var rune [1]rune
		w.tag.file.b.Read(w.tag.file.b.nc()-1, rune[:])
		if rune[0] == '\n' {
			// untested
			n++
		}
	}
	if n == 0 {
		n = 1
	}
	return n
}

// windrawbutton draws the modified/dirty button in the tag's top-left (wind.c).
func (a *Acme) windrawbutton(w *Window) {
	b := a.button
	if !w.isdir && !w.isscratch && (w.body.file.mod || w.body.ncache != 0) {
		b = a.modbutton
	}
	br := w.tag.scrollr
	br.Max.X = br.Min.X + b.R.Dx()
	br.Max.Y = br.Min.Y + b.R.Dy()
	a.screen.Draw(br, b, nil, b.R.Min)
}

// winclean reports whether window w has no unsaved changes, warning if dirty
// (wind.c).
func (a *Acme) winclean(w *Window, conservative bool) bool {
	if w.isscratch || w.isdir { // guide/error/etc. windows
		return true
	}
	if !conservative && w.nopen[qWevent] > 0 {
		// a program owns the window's event file; let it delete without whining
		return true
	}
	if w.dirty {
		if w.body.file.nname() != 0 {
			a.warning(nil, "%s modified\n", string(w.body.file.name))
		} else {
			if w.body.file.b.nc() < 100 {
				return true
			}
			a.warning(nil, "unnamed file modified\n")
		}
		w.dirty = false
		return false
	}
	return true
}

// textclose releases text t's frame and detaches it from its file (text.c).
func (a *Acme) textclose(t *Text) {
	t.cache = nil
	t.fr.Clear(true)
	a.filedeltext(t.file, t)
	t.file = nil
	if a.argtext == t {
		a.argtext = nil
	}
	if a.typetext == t {
		// a.typetext is never assigned in the port, so it never matches the
		// text being closed.
		// untested
		a.typetext = nil
	}
	if a.seltext == t {
		a.seltext = nil
	}
	if a.mousetext == t {
		a.mousetext = nil
	}
	if a.barttext == t {
		// clear the scroll-key target so a held-button scroll doesn't act on the
		// closed text (text.c). barttext is live (mouse.go sets it).
		a.barttext = nil
	}
}

// winlock1 locks window w and records the lock owner (wind.c winlock1). The
// incref (before the lock, so it can race with a concurrent winlock1 -- hence
// atomic) defers w's free to the matching winunlock: a window closed while
// locked is not actually torn down until its last holder unlocks.
func (a *Acme) winlock1(w *Window, owner int) {
	w.ref.Add(1)
	w.lk.Lock()
	w.owner = owner
}

// winlock locks every window sharing w's file -- the window and all its Zerox
// copies -- so an edit applies to them atomically (wind.c winlock).
func (a *Acme) winlock(w *Window, owner int) {
	f := w.body.file
	for _, t := range f.text {
		a.winlock1(t.w, owner)
	}
}

// winunlock unlocks the windows winlock locked and winclose's each (wind.c
// winunlock): winclose decrefs, and a window closed while locked is finally torn
// down here when its last reference drops. The loop runs backwards over a
// snapshot of the file's texts because winclose -> textclose removes a text from
// f.text (and frees the file on the last one), which would otherwise trip the
// live iteration.
func (a *Acme) winunlock(w *Window) {
	f := w.body.file
	if f == nil {
		// untested: with the refcount restored, winclose only tears down (nils
		// body.file) at ref 0, which a still-locked window never reaches before
		// winunlock; so a locked window always still has its file here.
		// untested: window edge path not exercised by -D
		w.owner = 0
		w.lk.Unlock()
		return
	}
	ws := make([]*Window, len(f.text))
	for i, t := range f.text {
		ws[i] = t.w
	}
	for i := len(ws) - 1; i >= 0; i-- {
		u := ws[i]
		u.owner = 0
		u.lk.Unlock()
		a.winclose(u)
	}
}

// winclose drops a reference to w and, when the last one goes, tears it down:
// logs the deletion, frees the directory listing, and closes both texts (wind.c
// winclose). The C free()s of incl/events/the Window become GC, but the visible
// teardown (windirfree, textclose) happens here, deterministically, not whenever
// GC runs.
func (a *Acme) winclose(w *Window) {
	if w.ref.Add(-1) != 0 {
		return
	}
	a.xfidlog(w, "del")
	a.windirfree(w)
	a.textclose(&w.tag)
	a.textclose(&w.body)
	if a.activewin == w {
		a.activewin = nil
	}
}

// windelete clears w's queued events and wakes any client blocked reading its
// event file, so the reader sees the window vanish at once (wind.c windelete).
// C sends to the per-window eventx; the Go event reader blocks on the shared
// a.event cond, so a Broadcast wakes it (it re-checks and finds the window gone).
func (a *Acme) windelete(w *Window) {
	w.events = nil
	if a.event != nil {
		a.event.Broadcast()
	}
}

// windirfree clears a directory window's cached listing (wind.c windirfree). The
// per-entry frees become GC, but the slice is cleared now -- the listing must be
// gone at this point (a reload or close), not at the next GC.
func (a *Acme) windirfree(w *Window) {
	if w.isdir {
		// untested: directory windows are not built under -D (no real dir read).
		w.dlp = nil
	}
}

// --- small rune helpers ---

func minInt(a, b int) int {
	if a < b {
		return a
	}
	return b
}
func maxInt(a, b int) int {
	if a > b {
		return a
	}
	return b
}

// runeeqRange reports whether a[:na] equals b[:nb] (C runeeq).
func runeeqRange(a []rune, na int, b []rune, nb int) bool {
	if na != nb || len(a) < na || len(b) < nb {
		return false
	}
	for i := 0; i < na; i++ {
		if a[i] != b[i] {
			return false
		}
	}
	return true
}

func runeIndexAll(s []rune, c rune) int {
	for i, r := range s {
		if r == c {
			return i
		}
	}
	return -1
}

func runeStrStr(s []rune, sub string) int {
	subr := []rune(sub)
	if len(subr) == 0 {
		// untested: every caller passes a non-empty needle.
		return 0
	}
	for i := 0; i+len(subr) <= len(s); i++ {
		ok := true
		for j := range subr {
			if s[i+j] != subr[j] {
				ok = false
				break
			}
		}
		if ok {
			return i
		}
	}
	return -1
}
