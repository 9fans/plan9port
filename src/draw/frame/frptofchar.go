package frame

import (
	"unicode/utf8"

	"9fans.net/go/draw"
)

func (f *Frame) ptofcharptb(p int, pt draw.Point, bn int) draw.Point {
	for ; bn < len(f.box); bn++ {
		b := &f.box[bn]
		f.cklinewrap(&pt, b)
		l := b.NRUNE()
		if p < l {
			if b.nrune > 0 {
				s := b.bytes
				for ; p > 0; p-- {
					if len(s) == 0 {
						drawerror(f.Display, "frptofchar")
					}
					_, size := utf8.DecodeRune(s)
					pt.X += f.Font.BytesWidth(s[:size])
					s = s[size:]
					if pt.X > f.R.Max.X {
						drawerror(f.Display, "frptofchar")
					}
				}
			}
			break
		}
		p -= l
		f.advance(&pt, b)
	}
	return pt
}

// PointOf returns
// the location of the upper left corner of the p'th rune,
// starting from 0, in the Frame f. If f holds fewer than p
// runes, frptofchar returns the location of the upper right
// corner of the last character in f.
func (f *Frame) PointOf(p int) draw.Point {
	return f.ptofcharptb(p, f.R.Min, 0)
}

func (f *Frame) ptofcharnb(p, nb int) draw.Point {
	// doesn't do final f.advance to next line
	box := f.box
	f.box = f.box[:nb]
	pt := f.ptofcharptb(p, f.R.Min, 0)
	f.box = box
	return pt
}

func (f *Frame) grid(p draw.Point) draw.Point {
	p.Y -= f.R.Min.Y
	p.Y -= p.Y % f.Font.Height
	p.Y += f.R.Min.Y
	if p.X > f.R.Max.X {
		p.X = f.R.Max.X
	}
	return p
}

// CharOf is the
// inverse of PointOf: it returns the index of the closest rune whose
// image's upper left corner is up and to the left of pt.
func (f *Frame) CharOf(pt draw.Point) int {
	pt = f.grid(pt)
	qt := f.R.Min
	p := 0
	bn := 0
	for ; bn < len(f.box) && qt.Y < pt.Y; bn++ {
		b := &f.box[bn]
		f.cklinewrap(&qt, b)
		if qt.Y >= pt.Y {
			break
		}
		f.advance(&qt, b)
		p += b.NRUNE()
	}
	for ; bn < len(f.box) && qt.X <= pt.X; bn++ {
		b := &f.box[bn]
		f.cklinewrap(&qt, b)
		if qt.Y > pt.Y {
			break
		}
		if qt.X+b.wid > pt.X {
			if b.nrune < 0 {
				f.advance(&qt, b)
			} else {
				s := b.bytes
				for {
					if len(s) == 0 {
						drawerror(f.Display, "end of string in frcharofpt")
					}
					_, size := utf8.DecodeRune(s)
					qt.X += f.Font.BytesWidth(s[:size])
					s = s[size:]
					if qt.X > pt.X {
						break
					}
					p++
				}
			}
		} else {
			p += b.NRUNE()
			f.advance(&qt, b)
		}
	}
	return p
}
