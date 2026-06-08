package frame

import (
	"9fans.net/go/draw"
)

func region(a, b int) int {
	if a < b {
		return -1
	}
	if a == b {
		return 0
	}
	return +1
}

// Select tracks the mouse to select a contiguous string of
// text in the Frame.  When called, a mouse button is typically
// down.  Select will return when the button state has
// changed (some buttons may still be down) and will set f.P0
// and f.P1 to the selected range of text.
func (f *Frame) Select(mc *draw.Mousectl) {
	// when called, button 1 is down

	mp := mc.Point
	b := mc.Buttons
	f.modified = false
	f.Drawsel(f.PointOf(f.P0), f.P0, f.P1, false)
	p0 := f.CharOf(mp)
	p1 := p0
	f.P0 = p0
	f.P1 = p1
	pt0 := f.PointOf(p0)
	pt1 := f.PointOf(p1)
	f.Drawsel(pt0, p0, p1, true)
	reg := 0
	for mc.Buttons == b {
		scrolled := false
		if f.Scroll != nil {
			if mp.Y < f.R.Min.Y {
				f.Scroll(f, -(f.R.Min.Y-mp.Y)/f.Font.Height-1)
				p0 = f.P1
				p1 = f.P0
				scrolled = true
			} else if mp.Y > f.R.Max.Y {
				f.Scroll(f, (mp.Y-f.R.Max.Y)/f.Font.Height+1)
				p0 = f.P0
				p1 = f.P1
				scrolled = true
			}
			if scrolled {
				if reg != region(p1, p0) {
					p0, p1 = p1, p0 // undo the swap that will happen below
				}
				pt0 = f.PointOf(p0)
				pt1 = f.PointOf(p1)
				reg = region(p1, p0)
			}
		}
		if q := f.CharOf(mp); p1 != q {
			if reg != region(q, p0) { // crossed starting point; reset
				if reg > 0 {
					f.Drawsel(pt0, p0, p1, false)
				} else if reg < 0 {
					f.Drawsel(pt1, p1, p0, false)
				}
				p1 = p0
				pt1 = pt0
				reg = region(q, p0)
				if reg == 0 {
					f.Drawsel(pt0, p0, p1, true)
				}
			}
			qt := f.PointOf(q)
			if reg > 0 {
				if q > p1 {
					f.Drawsel(pt1, p1, q, true)
				} else {
					f.Drawsel(qt, q, p1, false)
				}
			} else if reg < 0 {
				if q > p1 {
					f.Drawsel(pt1, p1, q, false)
				} else {
					f.Drawsel(qt, q, p1, true)
				}
			}
			p1 = q
			pt1 = qt
		}
		f.modified = false
		if p0 < p1 {
			f.P0 = p0
			f.P1 = p1
		} else {
			f.P0 = p1
			f.P1 = p0
		}
		if scrolled {
			f.Scroll(f, 0)
		}
		f.Display.Flush()
		if !scrolled {
			mc.Read()
		}
		mp = mc.Point
	}
}

// SelectPaint uses a solid color, col, to paint a region of the frame
// defined by the points p0 and p1.
func (f *Frame) SelectPaint(p0, p1 draw.Point, col *draw.Image) {
	q0 := p0
	q1 := p1
	q0.Y += f.Font.Height
	q1.Y += f.Font.Height
	n := (p1.Y - p0.Y) / f.Font.Height
	if f.B == nil {
		drawerror(f.Display, "frselectpaint b==0")
	}
	if p0.Y == f.R.Max.Y {
		return
	}
	if n == 0 {
		f.B.Draw(draw.Rectangle{Min: p0, Max: q1}, col, nil, draw.ZP)
	} else {
		if p0.X >= f.R.Max.X {
			p0.X = f.R.Max.X - 1
		}
		f.B.Draw(draw.Rect(p0.X, p0.Y, f.R.Max.X, q0.Y), col, nil, draw.ZP)
		if n > 1 {
			f.B.Draw(draw.Rect(f.R.Min.X, q0.Y, f.R.Max.X, p1.Y), col, nil, draw.ZP)
		}
		f.B.Draw(draw.Rect(f.R.Min.X, p1.Y, q1.X, q1.Y), col, nil, draw.ZP)
	}
}
