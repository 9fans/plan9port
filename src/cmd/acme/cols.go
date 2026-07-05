package main

import (
	"sort"

	"9fans.net/plan9port/draw"
	"9fans.net/plan9port/draw/frame"
)

// Column layout (cols.c): window stacking within a column (colgrow and the
// split path in coladd) is ported and exercised by the geometry scripts.

// Lheader is the column tag's command menu (cols.c).
var Lheader = []rune("New Cut Paste Snarf Sort Zerox Delcol ")

func (a *Acme) colinit(c *Column, r draw.Rectangle) {
	a.screen.Draw(r, a.disp.White, nil, draw.ZP)
	c.r = r
	c.w = nil
	t := &c.tag
	t.w = nil
	t.col = c
	r1 := r
	r1.Max.Y = r1.Min.Y + a.font.Height
	a.textinit(t, fileaddtext(nil, t), r1, &a.reffont, a.tagcols)
	t.what = Columntag
	r1.Min.Y = r1.Max.Y
	r1.Max.Y += a.border()
	a.screen.Draw(r1, a.disp.Black, nil, draw.ZP)
	a.textinsert(t, 0, Lheader, true)
	a.textsetselect(t, t.file.b.nc(), t.file.b.nc())
	a.screen.Draw(t.scrollr, a.colbutton, nil, a.colbutton.R.Min)
	c.safe = true
}

// colgrow grows or shrinks window w within column c (cols.c). but<0 just makes
// w fill its own space; but==2 packs everyone to give w all the room; but==3
// makes w full-size; otherwise w grows by a heuristic amount, stealing lines
// from its neighbours.
func (a *Acme) colgrow(c *Column, w *Window, but int) {
	i := -1
	for j := 0; j < c.nw(); j++ {
		if c.w[j] == w {
			i = j
			break
		}
	}
	if i < 0 {
		// unreachable
		bug("can't find window")
	}

	cr := c.r
	if but < 0 { // make sure window fills its own space properly
		r := w.r
		if i == c.nw()-1 || !c.safe {
			r.Max.Y = cr.Max.Y
		} else {
			r.Max.Y = c.w[i+1].r.Min.Y - a.border()
		}
		a.winresize(w, r, false, true)
		return
	}
	cr.Min.Y = c.w[0].r.Min.Y
	if but == 3 { // full size
		if i != 0 {
			v := c.w[0]
			c.w[0] = w
			c.w[i] = v
		}
		a.screen.Draw(cr, a.textcols[frame.BACK], nil, draw.ZP)
		a.winresize(w, cr, false, true)
		for k := 1; k < c.nw(); k++ {
			c.w[k].body.fr.MaxLines = 0
		}
		c.safe = false
		return
	}
	// store old #lines for each window
	onl := w.body.fr.MaxLines
	nl := make([]int, c.nw())
	ny := make([]int, c.nw())
	tot := 0
	for j := 0; j < c.nw(); j++ {
		l := c.w[j].taglines - 1 + c.w[j].body.fr.MaxLines
		nl[j] = l
		tot += l
	}
	// approximate new #lines for this window
	if but == 2 { // as big as can be
		for j := range nl {
			nl[j] = 0
		}
		goto Pack
	}
	{
		nnl := minInt(onl+maxInt(minInt(5, w.taglines-1+w.maxlines), onl/2), tot)
		if nnl < w.taglines-1+w.maxlines {
			nnl = (w.taglines - 1 + w.maxlines + nnl) / 2
		}
		if nnl == 0 {
			nnl = 2
		}
		dnl := nnl - onl
		// compute new #lines for each window
		for k := 1; k < c.nw(); k++ {
			// prune from later window
			j := i + k
			if j < c.nw() && nl[j] != 0 {
				l := minInt(dnl, maxInt(1, nl[j]/2))
				nl[j] -= l
				nl[i] += l
				dnl -= l
			}
			// prune from earlier window
			j = i - k
			if j >= 0 && nl[j] != 0 {
				l := minInt(dnl, maxInt(1, nl[j]/2))
				nl[j] -= l
				nl[i] += l
				dnl -= l
			}
		}
	}
Pack:
	// pack everyone above
	y1 := cr.Min.Y
	for j := 0; j < i; j++ {
		v := c.w[j]
		r := v.r
		r.Min.Y = y1
		r.Max.Y = y1 + v.tagtop.Dy()
		if nl[j] != 0 {
			r.Max.Y += 1 + nl[j]*v.body.fr.Font.Height
		}
		r.Min.Y = a.winresize(v, r, c.safe, false)
		r.Max.Y = r.Min.Y + a.border()
		a.screen.Draw(r, a.disp.Black, nil, draw.ZP)
		y1 = r.Max.Y
	}
	// scan to see new size of everyone below
	y2 := c.r.Max.Y
	for j := c.nw() - 1; j > i; j-- {
		v := c.w[j]
		r := v.r
		r.Min.Y = y2 - v.tagtop.Dy()
		if nl[j] != 0 {
			r.Min.Y -= 1 + nl[j]*v.body.fr.Font.Height
		}
		r.Min.Y -= a.border()
		ny[j] = r.Min.Y
		y2 = r.Min.Y
	}
	// compute new size of window
	{
		r := w.r
		r.Min.Y = y1
		r.Max.Y = y2
		h := w.body.fr.Font.Height
		if r.Dy() < w.tagtop.Dy()+1+h+a.border() {
			r.Max.Y = r.Min.Y + w.tagtop.Dy() + 1 + h + a.border()
		}
		// draw window
		r.Max.Y = a.winresize(w, r, c.safe, true)
		if i < c.nw()-1 {
			r.Min.Y = r.Max.Y
			r.Max.Y += a.border()
			a.screen.Draw(r, a.disp.Black, nil, draw.ZP)
			for j := i + 1; j < c.nw(); j++ {
				ny[j] -= y2 - r.Max.Y
			}
		}
		// pack everyone below
		y1 = r.Max.Y
		for j := i + 1; j < c.nw(); j++ {
			v := c.w[j]
			r := v.r
			r.Min.Y = y1
			r.Max.Y = y1 + v.tagtop.Dy()
			if nl[j] != 0 {
				r.Max.Y += 1 + nl[j]*v.body.fr.Font.Height
			}
			y1 = a.winresize(v, r, c.safe, j == c.nw()-1)
			if j < c.nw()-1 { // no border on last window
				r.Min.Y = y1
				r.Max.Y += a.border()
				a.screen.Draw(r, a.disp.Black, nil, draw.ZP)
				y1 = r.Max.Y
			}
		}
	}
	c.safe = true
	a.winmousebut(w)
}

// coladd adds window w (allocating one if nil) to column c near y (cols.c). When
// the column already holds windows, it splits the landing window to make room.
func (a *Acme) coladd(c *Column, w, clone *Window, y int) *Window {
	var v *Window
	r := c.r
	r.Min.Y = c.tag.fr.R.Max.Y + a.border()
	if y < r.Min.Y && c.nw() > 0 { // steal half of last window by default
		v = c.w[c.nw()-1]
		y = v.body.fr.R.Min.Y + v.body.fr.R.Dy()/2
	}
	// look for window we'll land on
	i := 0
	for ; i < c.nw(); i++ {
		v = c.w[i]
		if y < v.r.Max.Y {
			break
		}
	}
	buggered := false
	if c.nw() > 0 {
		if i < c.nw() {
			i++ // new window will go after v
		}
		// if landing window (v) is too small, grow it first.
		minht := v.tag.fr.Font.Height + a.border() + 1
		j := 0
		for !c.safe || v.body.fr.MaxLines <= 3 || v.body.all.Dy() <= minht {
			j++
			if j > 10 {
				buggered = true // too many windows in column
				break
			}
			a.colgrow(c, v, 1)
		}

		// figure out where to split v to make room for w

		// new window stops where next window begins
		var ymax int
		if i < c.nw() {
			ymax = c.w[i].r.Min.Y - a.border()
		} else {
			ymax = c.r.Max.Y
		}

		// new window must start after v's tag ends
		y = maxInt(y, v.tagtop.Max.Y+a.border())

		// new window must start early enough to end before ymax
		y = minInt(y, ymax-minht)

		// if y is too small, too many windows in column
		if y < v.tagtop.Max.Y+a.border() {
			buggered = true
		}

		// resize & redraw v
		r = v.r
		r.Max.Y = ymax
		a.screen.Draw(r, a.textcols[frame.BACK], nil, draw.ZP)
		r1 := r
		y = minInt(y, ymax-(v.tag.fr.Font.Height*v.taglines+v.body.fr.Font.Height+a.border()+1))
		r1.Max.Y = minInt(y, v.body.fr.R.Min.Y+v.body.fr.NumLines*v.body.fr.Font.Height)
		r1.Min.Y = a.winresize(v, r1, false, false)
		r1.Max.Y = r1.Min.Y + a.border()
		a.screen.Draw(r1, a.disp.Black, nil, draw.ZP)

		// leave r with w's coordinates
		r.Min.Y = r1.Max.Y
	}
	if w == nil {
		w = new(Window)
		w.col = c
		a.screen.Draw(r, a.textcols[frame.BACK], nil, draw.ZP)
		a.wininit(w, clone, r)
	} else {
		w.col = c
		a.winresize(w, r, false, true)
	}
	w.tag.col = c
	w.tag.row = c.row
	w.body.col = c
	w.body.row = c.row
	c.w = append(c.w, nil)
	copy(c.w[i+1:], c.w[i:])
	c.w[i] = w
	c.safe = true

	// if there were too many windows, redraw the whole column
	if buggered {
		a.colresize(c, c.r)
	}

	a.savemouse(w)
	// Near the button, but in the body. Don't warp the mouse to the new window
	// while a button is held (cols.c), so a New/Look run from a button chord
	// doesn't yank the pointer mid-gesture. (No button is ever down under -D, so
	// it still always warps there; a.mousectl is nil only in headless tests.)
	if a.mousectl == nil || a.mousectl.Buttons == 0 {
		a.moveto(w.tag.scrollr.Max.Add(draw.Pt(3, 3)))
	}
	a.barttext = &w.body // the new window is the scroll-key target (cols.c)
	return w
}

// colclose removes window w from column c, freeing it if dofree, and reflows
// the neighbours (cols.c).
func (a *Acme) colclose(c *Column, w *Window, dofree bool) {
	// w is locked (cols.c). If the column was left mid-layout, grow w first so
	// the reflow below starts from a consistent state.
	if !c.safe {
		// untested: c.safe is false only transiently inside colgrow, which
		// restores it before returning, so colclose is always entered safe.
		a.colgrow(c, w, 1)
	}
	i := -1
	for j, x := range c.w {
		if x == w {
			i = j
			break
		}
	}
	if i < 0 {
		// unreachable
		bug("can't find window")
	}
	r := w.r
	w.tag.col = nil
	w.body.col = nil
	w.col = nil
	didmouse := a.restoremouse(w)
	if dofree {
		a.windelete(w)
		a.winclose(w)
	}
	c.w = append(c.w[:i], c.w[i+1:]...)
	if c.nw() == 0 {
		a.screen.Draw(r, a.disp.White, nil, draw.ZP)
		return
	}
	up := false
	if i == c.nw() { // extend last window down
		w = c.w[i-1]
		r.Min.Y = w.r.Min.Y
		r.Max.Y = c.r.Max.Y
	} else { // extend next window up
		up = true
		w = c.w[i]
		r.Max.Y = w.r.Max.Y
	}
	a.screen.Draw(r, a.textcols[frame.BACK], nil, draw.ZP)
	if c.safe {
		if !didmouse && up {
			w.showdel = true
		}
		a.winresize(w, r, false, true)
		if !didmouse && up {
			a.movetodel(w)
		}
	}
}

func (a *Acme) colresize(c *Column, r draw.Rectangle) {
	a.clearmouse()
	r1 := r
	r1.Max.Y = r1.Min.Y + c.tag.fr.Font.Height
	a.textresize(&c.tag, r1, true)
	a.screen.Draw(c.tag.scrollr, a.colbutton, nil, a.colbutton.R.Min)
	r1.Min.Y = r1.Max.Y
	r1.Max.Y += a.border()
	a.screen.Draw(r1, a.disp.Black, nil, draw.ZP)
	r1.Max.Y = r.Max.Y
	newh := r.Dy() - c.nw()*(a.border()+a.font.Height)
	oldh := c.r.Dy() - c.nw()*(a.border()+a.font.Height)
	for i := 0; i < c.nw(); i++ {
		w := c.w[i]
		w.maxlines = 0
		if i == c.nw()-1 {
			r1.Max.Y = r.Max.Y
		} else {
			r1.Max.Y = r1.Min.Y
			if newh > 0 && oldh > 0 && w.r.Dy() > a.border()+a.font.Height {
				r1.Max.Y += (w.r.Dy()-a.border()-a.font.Height)*newh/oldh + a.border() + a.font.Height
			}
		}
		r1.Max.Y = maxInt(r1.Max.Y, r1.Min.Y+a.border()+a.font.Height)
		r2 := r1
		r2.Max.Y = r2.Min.Y + a.border()
		a.screen.Draw(r2, a.disp.Black, nil, draw.ZP)
		r1.Min.Y = r2.Max.Y
		r1.Min.Y = a.winresize(w, r1, false, i == c.nw()-1)
	}
	c.r = r
}

// colcmp orders windows by body file name (cols.c colcmp).
func colcmp(a, b *Window) int {
	r1 := a.body.file.name
	r2 := b.body.file.name
	n := len(r1)
	if len(r2) < n {
		n = len(r2)
	}
	for i := 0; i < n; i++ {
		if r1[i] != r2[i] {
			return int(r1[i]) - int(r2[i])
		}
	}
	return len(r1) - len(r2)
}

// colsort sorts a column's windows by file name and re-tiles them (cols.c).
func (a *Acme) colsort(c *Column) {
	if c.nw() == 0 {
		// untested: Sort always executes from a window in the column, so nw>=1
		return
	}
	a.clearmouse()
	wp := make([]*Window, c.nw())
	copy(wp, c.w)
	// C uses qsort (cols.c colsort), an unstable sort; use the unstable sort.Slice
	// to match. Equal file names would order arbitrarily, but names are unique.
	sort.Slice(wp, func(i, j int) bool { return colcmp(wp[i], wp[j]) < 0 })
	r := c.r
	r.Min.Y = c.tag.fr.R.Max.Y
	a.screen.Draw(r, a.textcols[frame.BACK], nil, draw.ZP)
	y := r.Min.Y
	for i, w := range wp {
		r.Min.Y = y
		if i == len(wp)-1 {
			r.Max.Y = c.r.Max.Y
		} else {
			r.Max.Y = r.Min.Y + w.r.Dy() + a.border()
		}
		r1 := r
		r1.Max.Y = r1.Min.Y + a.border()
		a.screen.Draw(r1, a.disp.Black, nil, draw.ZP)
		r.Min.Y = r1.Max.Y
		y = a.winresize(w, r, false, i == len(wp)-1)
	}
	c.w = wp
}

// colclean reports whether every window in c is clean (cols.c).
func (a *Acme) colclean(c *Column) bool {
	clean := true
	for _, w := range c.w {
		clean = a.winclean(w, true) && clean
	}
	return clean
}

// colcloseall closes a column and all its windows (cols.c).
func (a *Acme) colcloseall(c *Column) {
	if c == a.activecol {
		a.activecol = nil
	}
	a.textclose(&c.tag)
	for _, w := range c.w {
		a.winclose(w)
	}
	c.w = nil
	a.clearmouse()
}

// colwhich returns the Text (column tag, a window tag, or a window body) under
// point p in column c, or nil (cols.c colwhich).
func (a *Acme) colwhich(c *Column, p draw.Point) *Text {
	if !p.In(c.r) {
		// unreachable: rowwhich only calls colwhich after rowwhichcol has
		// confirmed p is inside c.r.
		return nil
	}
	if p.In(c.tag.all) {
		return &c.tag
	}
	for _, w := range c.w {
		if p.In(w.r) {
			if p.In(w.tagtop) || p.In(w.tag.all) {
				return &w.tag
			}
			// exclude the partial line at the bottom
			if p.X >= w.body.scrollr.Max.X && p.Y >= w.body.fr.R.Max.Y {
				return nil
			}
			return &w.body
		}
	}
	// untested: the windows tile the column below the tag, so a point in c.r is
	// always in the tag or a window -- no gap reaches here.
	return nil
}

// colmousebut warps the mouse to the center of c's tag scrollbar (cols.c).
func (a *Acme) colmousebut(c *Column) {
	r := c.tag.scrollr
	a.moveto(draw.Pt((r.Min.X+r.Max.X)/2, (r.Min.Y+r.Max.Y)/2))
}

// coldragwin handles a button-3 press on a window's tag: it tracks the mouse
// until release, then grows the window (tiny move), tosses it to another column
// (cross-column or rightward flick), shuffles it within the column, or resizes
// it against its upper neighbour (cols.c coldragwin). The mouse is the scripted
// synthetic Mousectl under -D.
func (a *Acme) coldragwin(c *Column, w *Window, but int) {
	a.clearmouse()
	a.disp.SwitchCursor2(&boxcursor, &boxcursor2)
	mc := a.mousectl
	b := mc.Buttons
	op := mc.Point
	for mc.Buttons == b {
		mc.Read()
		a.mousexy = mc.Point // C's mouse aliases mousectl->m; keep them in sync
	}
	a.disp.SwitchCursor(nil)
	if mc.Buttons != 0 {
		// a second button pressed mid-drag aborts it
		for mc.Buttons != 0 {
			mc.Read()
			a.mousexy = mc.Point
		}
		return
	}

	i := -1
	for j, ww := range c.w {
		if ww == w {
			i = j
			break
		}
	}
	if i < 0 {
		// unreachable: w is always a window of c
		bug("can't find window")
	}
	if w.tagexpand { // force recomputation of the window tag size
		w.taglines = 1
	}
	p := mc.Point
	if absInt(p.X-op.X) < 5 && absInt(p.Y-op.Y) < 5 {
		a.colgrow(c, w, but)
		a.winmousebut(w)
		return
	}
	// a flick to the right? toss it to the next column
	if absInt(p.Y-op.Y) < 10 && p.X > op.X+30 && a.rowwhichcol(c.row, p) == c {
		p.X = op.X + w.r.Dx()
	}
	nc := a.rowwhichcol(c.row, p)
	if nc != nil && nc != c {
		a.colclose(c, w, false)
		a.coladd(nc, w, nil, p.Y)
		a.winmousebut(w)
		return
	}
	if i == 0 && c.nw() == 1 {
		return // can't do it
	}
	if (i > 0 && p.Y < c.w[i-1].r.Min.Y) || (i < c.nw()-1 && p.Y > w.r.Max.Y) ||
		(i == 0 && p.Y > w.r.Max.Y) {
		// shuffle within the column
		a.colclose(c, w, false)
		a.coladd(c, w, nil, p.Y)
		a.winmousebut(w)
		return
	}
	if i == 0 {
		return
	}
	// resize against the upper neighbour v
	v := c.w[i-1]
	if p.Y < v.tagtop.Max.Y {
		p.Y = v.tagtop.Max.Y
	}
	if p.Y > w.r.Max.Y-w.tagtop.Dy()-a.border() {
		p.Y = w.r.Max.Y - w.tagtop.Dy() - a.border()
	}
	r := v.r
	r.Max.Y = p.Y
	if r.Max.Y > v.body.fr.R.Min.Y {
		r.Max.Y -= (r.Max.Y - v.body.fr.R.Min.Y) % v.body.fr.Font.Height
		if v.body.fr.R.Min.Y == v.body.fr.R.Max.Y {
			r.Max.Y++
		}
	}
	r.Min.Y = a.winresize(v, r, c.safe, false)
	r.Max.Y = r.Min.Y + a.border()
	a.screen.Draw(r, a.disp.Black, nil, draw.ZP)
	r.Min.Y = r.Max.Y
	if i == c.nw()-1 {
		r.Max.Y = c.r.Max.Y
	} else {
		r.Max.Y = c.w[i+1].r.Min.Y - a.border()
	}
	a.winresize(w, r, c.safe, true)
	c.safe = true
	a.winmousebut(w)
}
