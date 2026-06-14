package main

import (
	"slices"
	"time"

	"9fans.net/plan9port/draw"
	"9fans.net/plan9port/draw/frame"
)

// Vis is a layer's visibility.
type Vis int

const (
	None Vis = iota
	Some
	All
)

const Clicktime = 1000 * time.Millisecond // one second

type Flayer struct {
	f       frame.Frame
	origin  int // offset of first char in flayer
	p0, p1  int
	click   int // time of selection click, in ms
	textfn  func(l *Flayer, n int) []rune
	user0   int   //TODO: rename
	user1   *Text //TODO: rename text
	entire  draw.Rectangle
	scroll  draw.Rectangle
	lastsr  draw.Rectangle // geometry of scrollbar when last drawn
	visible Vis
}

func (t *Term) flstart(r draw.Rectangle) {
	t.lDrect = r
	display := t.display
	screen := t.screen

	// TODO: Why is frame.BORD 2x2?

	// Main text is yellowish.
	t.maincols[frame.BACK] = display.AllocImageMix(draw.PaleYellow, draw.White)
	t.maincols[frame.HIGH], _ = display.AllocImage(draw.Rect(0, 0, 1, 1), screen.Pix, true, draw.DarkYellow)
	t.maincols[frame.BORD], _ = display.AllocImage(draw.Rect(0, 0, 2, 2), screen.Pix, true, draw.YellowGreen)
	t.maincols[frame.TEXT] = display.Black
	t.maincols[frame.HTEXT] = display.Black

	// Command text is blueish.
	t.cmdcols[frame.BACK] = display.AllocImageMix(draw.PaleBlueGreen, draw.White)
	t.cmdcols[frame.HIGH], _ = display.AllocImage(draw.Rect(0, 0, 1, 1), screen.Pix, true, draw.PaleGreyGreen)
	t.cmdcols[frame.BORD], _ = display.AllocImage(draw.Rect(0, 0, 2, 2), screen.Pix, true, draw.PurpleBlue)
	t.cmdcols[frame.TEXT] = display.Black
	t.cmdcols[frame.HTEXT] = display.Black
}

func (t *Term) flnew(l *Flayer, fn func(*Flayer, int) []rune, u0 int, u1 *Text) {
	l.textfn = fn
	l.user0 = u0
	l.user1 = u1
	l.lastsr = draw.ZR
	t.llinsert(l)
}

func (t *Term) flrect(l *Flayer, r draw.Rectangle) draw.Rectangle {
	draw.RectClip(&r, t.lDrect)
	l.entire = r
	l.scroll = r.Inset(flmargin(l))
	r.Min.X += flmargin(l) + flscrollwid(l) + (flgap(l) - flmargin(l))
	l.scroll.Max.X = r.Min.X
	return r
}

func (t *Term) flinit(l *Flayer, r draw.Rectangle, ft *draw.Font, cols []*draw.Image) {
	t.lldelete(l)
	t.llinsert(l)
	l.visible = All
	l.origin = 0
	l.p0 = 0
	l.p1 = 0
	l.f.Display = t.display // for flmargin
	l.f.Init(t.flrect(l, r).Inset(flmargin(l)), ft, t.screen, cols)
	l.f.MaxTab = t.maxtab * ft.StringWidth("0")
	t.newvisibilities(true)
	t.screen.Draw(l.entire, l.f.Cols[frame.BACK], nil, draw.ZP)
	t.scrdraw(l, 0)
	t.flborder(l, false)
}

func (t *Term) flclose(l *Flayer) {
	if l.visible == All {
		t.screen.Draw(l.entire, t.display.White, nil, draw.ZP)
	} else if l.visible == Some {
		if l.f.B == nil {
			l.f.B, _ = t.display.AllocImage(l.entire, t.screen.Pix, false, draw.NoFill)
		}
		if l.f.B != nil {
			l.f.B.Draw(l.entire, t.display.White, nil, draw.ZP)
			t.flrefresh(l, l.entire, 0)
		}
	}
	l.f.Clear(true)
	t.lldelete(l)
	if l.f.B != nil && l.visible != All {
		l.f.B.Free()
	}
	l.f.B = nil
	l.textfn = nil
	t.newvisibilities(true)
}

func (t *Term) flborder(l *Flayer, wide bool) {
	if t.flprepare(l) {
		l.f.B.Border(l.entire, flmargin(l), l.f.Cols[frame.BACK], draw.ZP)
		l.f.B.Border(l.entire, cond(wide, flmargin(l), 1), l.f.Cols[frame.BORD], draw.ZP)
		if l.visible == Some {
			t.flrefresh(l, l.entire, 0)
		}
	}
}

func (t *Term) flwhich(p draw.Point) *Flayer {
	if p.X == 0 && p.Y == 0 {
		if len(t.llist) > 0 {
			return t.llist[0]
		}
		// untested: flwhich(0,0) returns nil only for an empty layer list, but the
		// command window occupies llist[0] from startup onward, so the list is never
		// empty while the event loop dispatches mouse points.
		return nil
	}
	for _, l := range t.llist {
		if p.In(l.entire) {
			return l
		}
	}
	return nil
}

func (t *Term) flupfront(l *Flayer) {
	v := l.visible
	t.lldelete(l)
	t.llinsert(l)
	if v != All {
		t.newvisibilities(false)
	}
}

// newvisibilities recomputes every layer's visibility. If redraw is false we
// know it's a flupfront and needn't redraw anyone becoming partially covered.
func (t *Term) newvisibilities(redraw bool) {
	for _, l := range t.llist {
		l.lastsr = draw.ZR // make sure scroll bar gets redrawn
		ov := l.visible
		l.visible = t.visibility(l)
		V := func(x, y Vis) [2]Vis { return [2]Vis{x, y} }
		switch V(ov, l.visible) {
		case V(Some, None):
			// covering window placed to exact pixel boundaries, not scriptable blind.
			if l.f.B != nil {
				l.f.B.Free()
			}
			fallthrough
		case V(All, None), V(All, Some):
			l.f.B = nil
			l.f.Clear(false)

		case V(Some, Some), V(None, Some):
			// needs the precise cover/uncover sequence described above.
			if ov == None || (l.f.B == nil && redraw) {
				t.flprepare(l)
			}
			if l.f.B != nil && redraw {
				t.flrefresh(l, l.entire, 0)
				l.f.B.Free()
				l.f.B = nil
				l.f.Clear(false)
			}
		case V(None, None), V(All, All):
			// nothing

		case V(Some, All):
			if l.f.B != nil {
				t.screen.Draw(l.entire, l.f.B, nil, l.entire.Min)
				l.f.B.Free()
				l.f.B = t.screen
				break
			}
			fallthrough
		case V(None, All):
			t.flprepare(l)
		}
		if ov == None && l.visible != None {
			t.flnewlyvisible(l)
		}
	}
}

func (t *Term) llinsert(l *Flayer) {
	t.llist = slices.Insert(t.llist, 0, l)
}

func (t *Term) lldelete(l *Flayer) {
	i := slices.Index(t.llist, l)
	if i < 0 {
		// unreachable
		panic("lldelete")
	}
	t.llist = slices.Delete(t.llist, i, i+1)
}

func (t *Term) flinsert(l *Flayer, sp []rune, p0 int) {
	if t.flprepare(l) {
		l.f.Insert(sp, p0-l.origin)
		t.scrdraw(l, t.scrtotal(l))
		if l.visible == Some {
			t.flrefresh(l, l.entire, 0)
		}
	}
}

func (t *Term) fldelete(l *Flayer, p0, p1 int) {
	if t.flprepare(l) {
		p0 -= l.origin
		p1 -= l.origin
		l.f.Delete(p0, p1)
		t.scrdraw(l, t.scrtotal(l))
		if l.visible == Some {
			// to edit it brings it fully to the front (All) first.
			t.flrefresh(l, l.entire, 0)
		}
	}
}

func (t *Term) flselect(l *Flayer) bool {
	if l.visible != All {
		// untested: flselect runs only on the current window (which==l), and `which`
		// is always llist[0], which visibility() always reports All; the only way to
		// make a covered window current without raising it is the Hcurrent flupfront
		// branch (mesg.go), itself proven not to occur, so flselect's l is always All.
		t.flupfront(l)
	}
	l.f.Select(t.mouse)
	ret := false
	if l.f.P0 == l.f.P1 {
		//TODO: Rethink Msec type.
		if time.Duration(int(t.mouse.Msec)-l.click)*time.Millisecond < Clicktime && l.f.P0+l.origin == l.p0 {
			ret = true
			l.click = 0
		} else {
			l.click = int(t.mouse.Msec)
		}
	} else {
		l.click = 0
	}
	l.p0 = l.f.P0 + l.origin
	l.p1 = l.f.P1 + l.origin
	return ret
}

func (t *Term) flsetselect(l *Flayer, p0, p1 int) {
	l.click = 0
	if l.visible == None || !t.flprepare(l) {
		// window completely covered by another, not placeable blind.
		l.p0 = p0
		l.p1 = p1
		return
	}
	l.p0 = p0
	l.p1 = p1
	fp0, fp1, ticked := flfp0p1(l)
	if fp0 == l.f.P0 && fp1 == l.f.P1 {
		if l.f.Ticked != ticked {
			l.f.Tick(l.f.PointOf(fp0), ticked)
		}
		return
	}

	if fp1 <= l.f.P0 || fp0 >= l.f.P1 || l.f.P0 == l.f.P1 || fp0 == fp1 {
		// no overlap or trivial repainting
		l.f.Drawsel(l.f.PointOf(l.f.P0), l.f.P0, l.f.P1, false)
		if fp0 != fp1 || ticked {
			l.f.Drawsel(l.f.PointOf(fp0), fp0, fp1, true)
		}
		//TODO: use else?
		goto Refresh
	}
	// the current selection and the desired selection overlap and are both non-empty
	if fp0 < l.f.P0 {
		// extend selection backwards
		l.f.Drawsel(l.f.PointOf(fp0), fp0, l.f.P0, true)
	} else if fp0 > l.f.P0 {
		// trim first part of selection
		l.f.Drawsel(l.f.PointOf(l.f.P0), l.f.P0, fp0, false)
	}
	if fp1 > l.f.P1 {
		// extend selection forwards
		l.f.Drawsel(l.f.PointOf(l.f.P1), l.f.P1, fp1, true)
	} else if fp1 < l.f.P1 {
		// trim last part of selection
		l.f.Drawsel(l.f.PointOf(fp1), fp1, l.f.P1, false)
	}

Refresh:
	l.f.P0 = fp0
	l.f.P1 = fp1
	if l.visible == Some {
		t.flrefresh(l, l.entire, 0)
	}
}

func flfp0p1(l *Flayer) (pp0, pp1 int, ticked bool) {
	p0 := l.p0 - l.origin
	p1 := l.p1 - l.origin
	ticked = p0 == p1
	if p0 < 0 {
		ticked = false
		p0 = 0
	}
	if p1 < 0 {
		p1 = 0
	}
	if p0 > l.f.NumChars {
		// only within the region it asked the terminal to show.
		p0 = l.f.NumChars
	}
	if p1 > l.f.NumChars {
		ticked = false
		p1 = l.f.NumChars
	}
	return p0, p1, ticked
}

func rscale(r draw.Rectangle, old, new draw.Point) draw.Rectangle {
	r.Min.X = r.Min.X * new.X / old.X
	r.Min.Y = r.Min.Y * new.Y / old.Y
	r.Max.X = r.Max.X * new.X / old.X
	r.Max.Y = r.Max.Y * new.Y / old.Y
	return r
}

func (t *Term) flresize(dr draw.Rectangle) {
	olDrect := t.lDrect
	t.lDrect = dr
	// no moving on rio; always repaint
	t.screen.Draw(t.lDrect, t.display.White, nil, draw.ZP)
	for _, l := range t.llist {
		l.lastsr = draw.ZR
		f := &l.f
		r := l.entire.Sub(olDrect.Min)
		r = rscale(r, olDrect.Max.Sub(olDrect.Min), dr.Max.Sub(dr.Min))
		r = r.Add(dr.Min)
		if l.visible == Some && f.B != nil {
			f.B.Free()
			f.Clear(false)
		}
		f.B = nil
		if l.visible != None {
			f.Clear(false)
		}
		if !draw.RectClip(&r, dr) {
			// unreachable
			panic("flresize")
		}
		if r.Max.X-r.Min.X < 100 {
			r.Min.X = dr.Min.X
		}
		if r.Max.X-r.Min.X < 100 {
			r.Max.X = dr.Max.X
		}
		if r.Max.Y-r.Min.Y < 2*flmargin(l)+f.Font.Height {
			r.Min.Y = dr.Min.Y
		}
		if r.Max.Y-r.Min.Y < 2*flmargin(l)+f.Font.Height {
			r.Max.Y = dr.Max.Y
		}
		l.visible = None
		f.SetRects(t.flrect(l, r).Inset(flmargin(l)), f.B)
		if f.B != nil {
			// untested: f.B was set to nil just above and SetRects is passed that
			// nil, so f.B is always nil here -- dead code mirroring C's flresize,
			// which likewise guards scrdraw with the just-cleared f->b.
			t.scrdraw(l, t.scrtotal(l))
		}
	}
	t.newvisibilities(true)
}

func (t *Term) flprepare(l *Flayer) bool {
	if l.visible == None {
		// covered by another, not placeable through the blind mouse script.
		return false
	}
	f := &l.f
	if f.B == nil {
		if l.visible == All {
			f.B = t.screen
		} else if f.B, _ = t.display.AllocImage(l.entire, t.screen.Pix, false, 0); f.B == nil {
			// untested: defensive -- AllocImage for the backing store fails only
			// under image-memory exhaustion, which the test backend never induces.
			return false
		}
		f.B.Draw(l.entire, f.Cols[frame.BACK], nil, draw.ZP)
		f.B.Border(l.entire, cond(l == t.llist[0], flmargin(l), 1), f.Cols[frame.BORD], draw.ZP)
		n := f.NumChars
		f.Init(f.Entire, f.Font, f.B, nil)
		f.MaxTab = t.maxtab * f.Font.StringWidth("0")
		r := l.textfn(l, n)
		f.Insert(r, 0)
		f.Drawsel(f.PointOf(f.P0), f.P0, f.P1, false)
		var ticked bool
		f.P0, f.P1, ticked = flfp0p1(l)
		if f.P0 != f.P1 || ticked {
			f.Drawsel(f.PointOf(f.P0), f.P0, f.P1, true)
		}
		l.lastsr = draw.ZR
		t.scrdraw(l, t.scrtotal(l))
	}
	return true
}

func (t *Term) visibility(l *Flayer) Vis {
	t.somevis = false
	t.someinvis = false
	t.justvis = true
	t.flrefresh(l, l.entire, 0)
	t.justvis = false
	if !t.somevis {
		// mouse script does not place one window exactly over another.
		return None
	}
	if !t.someinvis {
		return All
	}
	return Some
}

func (t *Term) flrefresh(l *Flayer, r draw.Rectangle, i int) {
	//TODO: rewrite as loop and check for running off end
Top:
	top := t.llist[i]
	i++
	if top == l {
		if !t.justvis {
			t.screen.Draw(r, l.f.B, nil, r.Min)
		}
		t.somevis = true
	} else {
		if !draw.RectXRect(top.entire, r) {
			goto Top // avoid stacking unnecessarily
		}
		if top.entire.Min.X > r.Min.X {
			s := r
			s.Max.X = top.entire.Min.X
			t.flrefresh(l, s, i)
			r.Min.X = top.entire.Min.X
		}
		if top.entire.Min.Y > r.Min.Y {
			s := r
			s.Max.Y = top.entire.Min.Y
			t.flrefresh(l, s, i)
			r.Min.Y = top.entire.Min.Y
		}
		if top.entire.Max.X < r.Max.X {
			s := r
			s.Min.X = top.entire.Max.X
			t.flrefresh(l, s, i)
			r.Max.X = top.entire.Max.X
		}
		if top.entire.Max.Y < r.Max.Y {
			s := r
			s.Min.Y = top.entire.Max.Y
			t.flrefresh(l, s, i)
			r.Max.Y = top.entire.Max.Y
		}
		// remaining piece of r is blocked by top; forget about it
		t.someinvis = true
	}
}

func flscale(l *Flayer, n int) int {
	if l == nil {
		// untested: flscale's callers (flmargin/flscrollwid) always pass a real
		// layer; the nil guard is defensive.
		return n
	}
	return l.f.Display.ScaleSize(n)
}

func flmargin(l *Flayer) int    { return flscale(l, 4) }
func flscrollwid(l *Flayer) int { return flscale(l, 12) }
func flgap(l *Flayer) int       { return flscale(l, 4) }
