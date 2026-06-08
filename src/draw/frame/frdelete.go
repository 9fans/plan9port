package frame

import "9fans.net/go/draw"

// Delete deletes from f the text between p0 and p1;
// p1 points at the first rune beyond the deletion.
func (f *Frame) Delete(p0, p1 int) int {
	if p0 >= f.NumChars || p0 == p1 || f.B == nil {
		return 0
	}
	if p1 > f.NumChars {
		p1 = f.NumChars
	}
	n0 := f.findbox(0, 0, p0)
	if n0 >= len(f.box) {
		drawerror(f.Display, "off end in frdelete")
	}
	n1 := f.findbox(n0, p0, p1)
	pt0 := f.ptofcharnb(p0, n0)
	pt1 := f.PointOf(p1)
	if f.P0 == f.P1 {
		f.Tick(f.PointOf(f.P0), false)
	}
	nn0 := n0
	ppt0 := pt0
	f.modified = true

	// Invariants:
	//   - pt0 points to beginning, pt1 points to end
	//   - n0 is box containing beginning of stuff being deleted
	//   - n1, b are box containing beginning of stuff to be kept after deletion
	//   - cn1 is char position of n1
	//   - f.P0 and f.P1 are not adjusted until after all deletion is done
	cn1 := p1
	for pt1.X != pt0.X && n1 < len(f.box) {
		b := &f.box[n1]
		f.cklinewrap0(&pt0, b)
		f.cklinewrap(&pt1, b)
		n := f.canfit(pt0, b)
		if n == 0 {
			drawerror(f.Display, "_frcanfit==0")
		}
		r := draw.Rectangle{pt0, pt0}
		r.Max.Y += f.Font.Height
		if b.nrune > 0 {
			w0 := b.wid
			if n != b.nrune {
				f.splitbox(n1, n)
				b = &f.box[n1]
			}
			r.Max.X += b.wid
			f.B.Draw(r, f.B, nil, pt1)
			cn1 += b.nrune

			// blank remainder of line
			r.Min.X = r.Max.X
			r.Max.X += w0 - b.wid
			if r.Max.X > f.R.Max.X {
				r.Max.X = f.R.Max.X
			}
			f.B.Draw(r, f.Cols[BACK], nil, r.Min)
		} else {
			r.Max.X += f.newwid0(pt0, b)
			if r.Max.X > f.R.Max.X {
				r.Max.X = f.R.Max.X
			}
			col := f.Cols[BACK]
			if f.P0 <= cn1 && cn1 < f.P1 {
				col = f.Cols[HIGH]
			}
			f.B.Draw(r, col, nil, pt0)
			cn1++
		}
		f.advance(&pt1, b)
		pt0.X += f.newwid(pt0, b)
		f.box[n0] = f.box[n1]
		n0++
		n1++
	}
	if n1 == len(f.box) && pt0.X != pt1.X { // deleting last thing in window; must clean up
		f.SelectPaint(pt0, pt1, f.Cols[BACK])
	}
	if pt1.Y != pt0.Y {
		pt2 := f.ptofcharptb(32767, pt1, n1) // TODO 32767
		if pt2.Y > f.R.Max.Y {
			drawerror(f.Display, "frptofchar in frdelete")
		}
		if n1 < len(f.box) {
			q0 := pt0.Y + f.Font.Height
			q1 := pt1.Y + f.Font.Height
			q2 := pt2.Y + f.Font.Height
			if q2 > f.R.Max.Y {
				q2 = f.R.Max.Y
			}
			f.B.Draw(draw.Rect(pt0.X, pt0.Y, pt0.X+(f.R.Max.X-pt1.X), q0), f.B, nil, pt1)
			f.B.Draw(draw.Rect(f.R.Min.X, q0, f.R.Max.X, q0+(q2-q1)), f.B, nil, draw.Pt(f.R.Min.X, q1))
			f.SelectPaint(draw.Pt(pt2.X, pt2.Y-(pt1.Y-pt0.Y)), pt2, f.Cols[BACK])
		} else {
			f.SelectPaint(pt0, pt2, f.Cols[BACK])
		}
	}
	f.delbox(n0, n1-1)
	if nn0 > 0 && f.box[nn0-1].nrune >= 0 && ppt0.X-f.box[nn0-1].wid >= f.R.Min.X {
		nn0--
		ppt0.X -= f.box[nn0].wid
	}
	if n0 < len(f.box)-1 {
		n0++
	}
	f.clean(ppt0, nn0, n0)
	if f.P1 > p1 {
		f.P1 -= p1 - p0
	} else if f.P1 > p0 {
		f.P1 = p0
	}
	if f.P0 > p1 {
		f.P0 -= p1 - p0
	} else if f.P0 > p0 {
		f.P0 = p0
	}
	f.NumChars -= p1 - p0
	if f.P0 == f.P1 {
		f.Tick(f.PointOf(f.P0), true)
	}
	pt0 = f.PointOf(f.NumChars)
	n := f.NumLines
	f.NumLines = (pt0.Y - f.R.Min.Y) / f.Font.Height
	if pt0.X > f.R.Min.X {
		f.NumLines++
	}
	return n - f.NumLines
}
