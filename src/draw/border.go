package draw

// BorderOp is like Border but specifies an explicit Porter-Duff operator.
func (dst *Image) BorderOp(r Rectangle, n int, color *Image, sp Point, op Op) {
	if n < 0 {
		r = r.Inset(n)
		sp = sp.Add(Pt(n, n))
		n = -n
	}
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	draw(dst, Rect(r.Min.X, r.Min.Y, r.Max.X, r.Min.Y+n),
		color, sp, nil, sp, op)
	pt := Pt(sp.X, sp.Y+r.Dy()-n)
	draw(dst, Rect(r.Min.X, r.Max.Y-n, r.Max.X, r.Max.Y),
		color, pt, nil, pt, op)
	pt = Pt(sp.X, sp.Y+n)
	draw(dst, Rect(r.Min.X, r.Min.Y+n, r.Min.X+n, r.Max.Y-n),
		color, pt, nil, pt, op)
	pt = Pt(sp.X+r.Dx()-n, sp.Y+n)
	draw(dst, Rect(r.Max.X-n, r.Min.Y+n, r.Max.X, r.Max.Y-n),
		color, pt, nil, pt, op)
}

// Border draws an outline of rectangle r in the specified color.
// The outline has width w; if w is positive, the border goes inside the
// rectangle; if negative, outside.
// The source is aligned so that sp corresponds to r.Min.
func (dst *Image) Border(r Rectangle, w int, color *Image, sp Point) {
	dst.BorderOp(r, w, color, sp, SoverD)
}
