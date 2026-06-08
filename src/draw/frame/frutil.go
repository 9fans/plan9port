package frame

import (
	"unicode/utf8"

	"9fans.net/go/draw"
)

// canfit returns the number of runes from b that
// will fit into the frame when drawn starting at pt.
func (f *Frame) canfit(pt draw.Point, b *box) int {
	left := f.R.Max.X - pt.X
	if b.nrune < 0 {
		if b.minwid <= left {
			return 1
		}
		return 0
	}
	if left >= b.wid {
		return b.nrune
	}
	p := b.bytes
	for nr := 0; nr < b.nrune; nr++ {
		_, size := utf8.DecodeRune(p)
		left -= f.Font.BytesWidth(p[:size])
		p = p[size:]
		if left < 0 {
			return nr
		}
	}
	drawerror(f.Display, "_frcanfit can't")
	return 0
}

// cklinewrap checks whether b when drawn at p will
// fit within f.R. If not, it updates p to the start of the next line.
func (f *Frame) cklinewrap(p *draw.Point, b *box) {
	w := b.wid
	if b.nrune < 0 {
		w = b.minwid
	}
	if w > f.R.Max.X-p.X {
		p.X = f.R.Min.X
		p.Y += f.Font.Height
	}
}

// cklinewrap0 is like cklinewrap but with a weaker criterion
// for wrapping: it only wraps if none of the text of b
// will fit.
func (f *Frame) cklinewrap0(p *draw.Point, b *box) {
	if f.canfit(*p, b) == 0 {
		p.X = f.R.Min.X
		p.Y += f.Font.Height
	}
}

func (f *Frame) advance(p *draw.Point, b *box) {
	if b.nrune < 0 && b.bc == '\n' {
		p.X = f.R.Min.X
		p.Y += f.Font.Height
	} else {
		p.X += b.wid
	}
}

func (f *Frame) newwid(pt draw.Point, b *box) int {
	b.wid = f.newwid0(pt, b)
	return b.wid
}

func (f *Frame) newwid0(pt draw.Point, b *box) int {
	c := f.R.Max.X
	x := pt.X
	if b.nrune >= 0 || b.bc != '\t' {
		return b.wid
	}
	if x+b.minwid > c {
		pt.X = f.R.Min.X
		x = pt.X
	}
	x += f.MaxTab
	x -= (x - f.R.Min.X) % f.MaxTab
	if x-pt.X < b.minwid || x > c {
		x = pt.X + b.minwid
	}
	return x - pt.X
}

func (f *Frame) clean(pt draw.Point, n0, n1 int) {
	// look for mergeable boxes

	c := f.R.Max.X
	nb := n0
	for ; nb < n1-1; nb++ {
		b := &f.box[nb]
		f.cklinewrap(&pt, b)
		for b.nrune >= 0 && nb < n1-1 && f.box[nb+1].nrune >= 0 && pt.X+b.wid+f.box[nb+1].wid < c {
			f.mergebox(nb)
			n1--
			b = &f.box[nb]
		}
		f.advance(&pt, &f.box[nb])
	}
	for ; nb < len(f.box); nb++ {
		b := &f.box[nb]
		f.cklinewrap(&pt, b)
		f.advance(&pt, &f.box[nb])
	}
	f.LastLineFull = pt.Y >= f.R.Max.Y
}
