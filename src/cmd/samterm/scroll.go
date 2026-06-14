package main

import (
	"9fans.net/plan9port/draw"
	"9fans.net/plan9port/draw/frame"
)

func (t *Term) scrtemps() {
	if t.scrtmp != nil {
		return
	}
	_, h, ok := t.screensize()
	if !ok {
		h = 2048
	}
	pix := t.screen.Pix
	t.scrtmp, _ = t.display.AllocImage(draw.Rect(0, 0, 32, h), pix, false, 0)
	t.scrback, _ = t.display.AllocImage(draw.Rect(0, 0, 32, h), pix, false, 0)
	if t.scrtmp == nil || t.scrback == nil {
		// unreachable
		panic("scrtemps")
	}
}

func scrpos(r draw.Rectangle, p0, p1, tot int) draw.Rectangle {
	q := r
	h := q.Max.Y - q.Min.Y
	if tot == 0 {
		return q
	}
	if tot > 1024*1024 {
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

func (t *Term) scrmark(l *Flayer, r draw.Rectangle) {
	r.Max.X--
	if draw.RectClip(&r, l.scroll) {
		l.f.B.Draw(r, l.f.Cols[frame.HIGH], nil, draw.ZP)
	}
}

func (t *Term) scrunmark(l *Flayer, r draw.Rectangle) {
	if draw.RectClip(&r, l.scroll) {
		l.f.B.Draw(r, t.scrback, nil, draw.Pt(0, r.Min.Y-l.scroll.Min.Y))
	}
}

func (t *Term) scrdraw(l *Flayer, tot int) {
	t.scrtemps()
	if l.f.B == nil {
		// unreachable
		panic("scrdraw")
	}
	r := l.scroll
	r1 := r
	var b *draw.Image
	if l.visible == All {
		b = t.scrtmp
		r1.Min.X = 0
		r1.Max.X = r.Dx()
	} else {
		b = l.f.B
	}
	r2 := scrpos(r1, l.origin, l.origin+l.f.NumChars, tot)
	if !r2.Eq(l.lastsr) {
		l.lastsr = r2
		b.Draw(r1, l.f.Cols[frame.BORD], nil, draw.ZP)
		b.Draw(r2, l.f.Cols[frame.BACK], nil, r2.Min)
		r2 = r1
		r2.Min.X = r2.Max.X - 1
		b.Draw(r2, l.f.Cols[frame.BORD], nil, draw.ZP)
		if b != l.f.B {
			l.f.B.Draw(r, b, nil, r1.Min)
		}
	}
}

func (t *Term) scroll(l *Flayer, but int) {
	in := false
	tot := t.scrtotal(l)
	s := l.scroll
	x := s.Min.X + flscrollwid(l)/2
	scr := scrpos(l.scroll, l.origin, l.origin+l.f.NumChars, tot)
	r := scr
	y := scr.Min.Y
	my := t.mouse.Y
	t.scrback.Draw(draw.Rect(0, 0, l.scroll.Dx(), l.scroll.Dy()), l.f.B, nil, l.scroll.Min)
	for {
		oin := in
		in = abs(x-t.mouse.X) <= flscrollwid(l)/2
		if oin && !in {
			t.scrunmark(l, r)
		}
		if in {
			t.scrmark(l, r)
			oy := y
			my = t.mouse.Y
			if my < s.Min.Y {
				my = s.Min.Y
			}
			if my >= s.Max.Y {
				my = s.Max.Y
			}
			if !t.mouse.Eq(draw.Pt(x, my)) {
				t.display.MoveCursor(draw.Pt(x, my))
			}
			switch but {
			case 1:
				p0 := l.origin - l.f.CharOf(draw.Pt(s.Max.X, my))
				rt := scrpos(l.scroll, p0, p0+l.f.NumChars, tot)
				y = rt.Min.Y
			case 2:
				y = my
				if y > s.Max.Y-2 {
					y = s.Max.Y - 2
				}
			case 3:
				p0 := l.origin + l.f.CharOf(draw.Pt(s.Max.X, my))
				rt := scrpos(l.scroll, p0, p0+l.f.NumChars, tot)
				y = rt.Min.Y
			}
			if y != oy {
				t.scrunmark(l, r)
				r = scr.Add(draw.Pt(0, y-scr.Min.Y))
				t.scrmark(l, r)
			}
		}
		if t.button(but) == 0 {
			break
		}
	}
	if in {
		h := s.Max.Y - s.Min.Y
		t.scrunmark(l, r)
		p0 := 0
		switch but {
		case 1:
			p0 = (my-s.Min.Y)/l.f.Font.Height + 1
		case 2:
			if tot > 1024*1024 {
				p0 = ((tot >> 10) * (y - s.Min.Y) / h) << 10
			} else {
				p0 = tot * (y - s.Min.Y) / h
			}
		case 3:
			p0 = l.origin + l.f.CharOf(draw.Pt(s.Max.X, my))
			if p0 > tot {
				// untested: CharOf returns at most NumChars and origin is at most
				// tot-NumChars, so origin+CharOf never exceeds tot -- a defensive
				// clamp mirroring C.
				p0 = tot
			}
		}
		t.scrorigin(l, but, p0)
	}
}

func abs(n int) int {
	if n < 0 {
		return -n
	}
	return n
}
