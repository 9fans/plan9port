package frame

import (
	"9fans.net/go/draw"
)

// bxscan splits text into boxes inside tmpf starting at *ppt,
// and reports the position at its end.
//
// It updates ppt to the position of the start of the text.
func (f *Frame) bxscan(tmpf *Frame, text []rune, ppt *draw.Point) draw.Point {
	tmpf.R = f.R
	tmpf.B = f.B
	tmpf.Font = f.Font
	tmpf.MaxTab = f.MaxTab
	tmpf.box = nil
	tmpf.NumChars = 0
	tmpf.Cols = f.Cols
	tmpf.MaxLines = f.MaxLines
	nl := 0
	for len(text) > 0 && nl <= tmpf.MaxLines {
		tmpf.box = append(tmpf.box, box{})
		b := &tmpf.box[len(tmpf.box)-1]
		c := text[0]
		if c == '\t' || c == '\n' {
			b.bc = c
			b.wid = 5000
			if c == '\t' {
				b.minwid = tmpf.Font.StringWidth(" ")
			}
			b.nrune = -1
			if c == '\n' {
				nl++
			}
			tmpf.NumChars++
			text = text[1:]
		} else {
			nr := 0
			w := 0
			for nr < len(text) {
				c := text[nr]
				if c == '\t' || c == '\n' || nr > 256 { // 256 is arbitrary cutoff to keep boxes small
					break
				}
				w += tmpf.Font.RunesWidth(text[nr : nr+1])
				nr++
			}
			b.bytes, text = []byte(string(text[:nr])), text[nr:]
			b.wid = w
			b.nrune = nr
			tmpf.NumChars += nr
		}
	}
	// Adjust for line wrap before calling draw so that
	// the starting position agrees with that of draw.
	// TODO we could call tmpf.cklinewrap0 instead,
	// which would arguably be clearer.
	f.cklinewrap0(ppt, &tmpf.box[0])
	return tmpf.draw(*ppt)
}

// chop removes any boxes in f that extend beyond
// its bounds, starting the calculation at box index bn
// that's at rune index p. It updates f.NumChars and f.NumLines.
//
// This is only called when we know that the frame needs to be
// truncated.
func (f *Frame) chop(pt draw.Point, p, bn int) {
	for ; ; bn++ {
		if bn >= len(f.box) {
			drawerror(f.Display, "endofframe")
		}
		b := &f.box[bn]
		f.cklinewrap(&pt, b)
		if pt.Y >= f.R.Max.Y {
			break
		}
		p += b.NRUNE()
		f.advance(&pt, b)
	}
	f.NumChars = p
	f.NumLines = f.MaxLines
	if bn < len(f.box) { // BUG
		f.delbox(bn, len(f.box)-1)
	}
}

// Insert inserts text into f starting at rune index p0.
// Tabs and newlines are handled by the library,
// but all other characters, including control characters, are just displayed.
// For example, backspaces are printed; to erase a character, use frdelete.
func (f *Frame) Insert(text []rune, p int) {
	p0 := p
	if p0 > f.NumChars || len(text) == 0 || f.B == nil {
		return
	}
	// Find the index of the box that starts at the insertion point,
	// splitting the box that's there if needed.
	n0 := f.findbox(0, 0, p0)
	// cn0 tracks the rune index of box that's being processed.
	cn0 := p0
	// nn0 remembers the index of the starting box (we update
	// n0 as we scan through during the calculations).
	nn0 := n0

	// Find the point at the end of the box just before the insertion point.
	pt0 := f.ptofcharnb(p0, n0)
	// ppt0 remembers the starting point.
	ppt0 := pt0
	opt0 := pt0

	// Scan the text into a temporary frame starting at the point
	// we know that it will be inserted. This tells us where the
	// text will end up.
	var tmpf Frame
	pt1 := f.bxscan(&tmpf, text, &ppt0)
	ppt1 := pt1
	if n0 < len(f.box) {
		b := &f.box[n0]
		f.cklinewrap(&pt0, b) // for frdrawsel
		f.cklinewrap0(&ppt1, b)
	}
	f.modified = true

	// ppt0 and ppt1 are start and end of insertion
	// as they will appear when insertion is complete.
	// pt0 is current location of insertion position (p0).
	// pt1 is terminal point (without line wrap) of insertion.
	if f.P0 == f.P1 {
		f.Tick(f.PointOf(f.P0), false)
	}
	// Find point where old and new x's line up.
	// Invariants:
	//   - pt0 is where the next box (b, n0) is now
	//   - pt1 is where it will be after the insertion
	// If pt1 goes off the rectangle, we can toss everything from there on.
	var pts []draw.Point
	for pt1.X != pt0.X && pt1.Y != f.R.Max.Y && n0 < len(f.box) {
		b := &f.box[n0]
		f.cklinewrap(&pt0, b)
		f.cklinewrap0(&pt1, b)
		if b.nrune > 0 {
			n := f.canfit(pt1, b)
			if n == 0 {
				drawerror(f.Display, "canfit==0")
			}
			if n != b.nrune {
				f.splitbox(n0, n)
				b = &f.box[n0]
			}
		}
		// has a text box overflowed off the frame?
		if pt1.Y == f.R.Max.Y {
			break
		}
		pts = append(pts, pt0, pt1)
		f.advance(&pt0, b)
		pt1.X += f.newwid(pt1, b)
		cn0 += b.NRUNE()
		n0++
	}
	if pt1.Y > f.R.Max.Y {
		drawerror(f.Display, "frinsert pt1 too far")
	}
	if pt1.Y == f.R.Max.Y && n0 < len(f.box) {
		f.NumChars -= f.strlen(n0)
		f.delbox(n0, len(f.box)-1)
	}
	if n0 == len(f.box) {
		f.NumLines = (pt1.Y - f.R.Min.Y) / f.Font.Height
		if pt1.X > f.R.Min.X {
			f.NumLines++
		}
	} else if pt1.Y != pt0.Y {
		y := f.R.Max.Y
		q0 := pt0.Y + f.Font.Height
		q1 := pt1.Y + f.Font.Height
		f.NumLines += (q1 - q0) / f.Font.Height
		if f.NumLines > f.MaxLines {
			f.chop(ppt1, p0, nn0)
		}
		if pt1.Y < y {
			r := f.R
			r.Min.Y = q1
			r.Max.Y = y
			if q1 < y {
				f.B.Draw(r, f.B, nil, draw.Pt(f.R.Min.X, q0))
			}
			r.Min = pt1
			r.Max.X = pt1.X + (f.R.Max.X - pt0.X)
			r.Max.Y = q1
			f.B.Draw(r, f.B, nil, pt0)
		}
	}

	// Move the old stuff down to make room.
	// The draws above moved everything down after the point where x's lined up.
	// The loop below will move the stuff between the insertion and the draws.
	y := pt1.Y
	if y != f.R.Max.Y {
		y = 0
	}
	for bn := n0 - 1; len(pts) > 0; bn, pts = bn-1, pts[:len(pts)-2] {
		b := &f.box[bn]
		pt := pts[len(pts)-1]
		if b.nrune > 0 {
			r := draw.Rectangle{Min: pt, Max: pt}
			r.Max.X += b.wid
			r.Max.Y += f.Font.Height
			f.B.Draw(r, f.B, nil, pts[len(pts)-2])
			// clear bit hanging off right
			if len(pts) == 2 && pt.Y > pt0.Y {
				// first new char is bigger than first char we're displacing,
				// causing line wrap. ugly special case.
				r := draw.Rectangle{Min: opt0, Max: opt0}
				r.Max.X = f.R.Max.X
				r.Max.Y += f.Font.Height
				var col *draw.Image
				if f.P0 <= cn0 && cn0 < f.P1 { // b+1 is inside selection
					col = f.Cols[HIGH]
				} else {
					col = f.Cols[BACK]
				}
				f.B.Draw(r, col, nil, r.Min)
			} else if pt.Y < y {
				r := draw.Rectangle{Min: pt, Max: pt}
				r.Min.X += b.wid
				r.Max.X = f.R.Max.X
				r.Max.Y += f.Font.Height
				var col *draw.Image
				if f.P0 <= cn0 && cn0 < f.P1 { // b+1 is inside selection
					col = f.Cols[HIGH]
				} else {
					col = f.Cols[BACK]
				}
				f.B.Draw(r, col, nil, r.Min)
			}
			y = pt.Y
			cn0 -= b.nrune
		} else {
			r := draw.Rectangle{Min: pt, Max: pt}
			r.Max.X += b.wid
			r.Max.Y += f.Font.Height
			if r.Max.X >= f.R.Max.X {
				r.Max.X = f.R.Max.X
			}
			cn0--
			var col *draw.Image
			if f.P0 <= cn0 && cn0 < f.P1 { // b is inside selection
				col = f.Cols[HIGH]
			} else {
				col = f.Cols[BACK]
			}
			f.B.Draw(r, col, nil, r.Min)
			y = 0
			if pt.X == f.R.Min.X {
				y = pt.Y
			}
		}
	}
	// insertion can extend the selection, so the condition here is different.
	var col, tcol *draw.Image
	if f.P0 < p0 && p0 <= f.P1 {
		col = f.Cols[HIGH]
		tcol = f.Cols[HTEXT]
	} else {
		col = f.Cols[BACK]
		tcol = f.Cols[TEXT]
	}
	f.SelectPaint(ppt0, ppt1, col)
	tmpf.drawtext(ppt0, tcol, col)
	f.addbox(nn0, len(tmpf.box))
	copy(f.box[nn0:], tmpf.box)
	if nn0 > 0 && f.box[nn0-1].nrune >= 0 && ppt0.X-f.box[nn0-1].wid >= f.R.Min.X {
		// There's some text just before the insertion point. Make sure we clean up from there.
		// TODO when can the start of that box ever be outside the frame rectangle bounds?
		nn0--
		ppt0.X -= f.box[nn0].wid
	}
	n0 += len(tmpf.box)
	if n0 < len(f.box)-1 {
		n0++
	}
	f.clean(ppt0, nn0, n0)
	f.NumChars += tmpf.NumChars
	if f.P0 >= p0 {
		f.P0 += tmpf.NumChars
	}
	if f.P0 > f.NumChars {
		f.P0 = f.NumChars
	}
	if f.P1 >= p0 {
		f.P1 += tmpf.NumChars
	}
	if f.P1 > f.NumChars {
		f.P1 = f.NumChars
	}
	if f.P0 == f.P1 {
		f.Tick(f.PointOf(f.P0), true)
	}
}
