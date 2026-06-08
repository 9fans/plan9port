package draw

func addcoord(p []byte, oldx, newx int) int {
	dx := newx - oldx
	if uint(dx - -0x40) <= 0x7F {
		p[0] = byte(dx & 0x7F)
		return 1
	}
	p[0] = 0x80 | byte(newx&0x7F)
	p[1] = byte(newx >> 7)
	p[2] = byte(newx >> 15)
	return 3
}

func dopoly(cmd byte, dst *Image, pp []Point, end0, end1 End, radius int, src *Image, sp Point, op Op) {
	if len(pp) == 0 {
		return
	}

	setdrawop(dst.Display, op)
	m := 1 + 4 + 2 + 4 + 4 + 4 + 4 + 2*4 + len(pp)*2*3 // too much
	a := dst.Display.bufimage(m)                       // too much
	a[0] = cmd
	bplong(a[1:], uint32(dst.id))
	bpshort(a[5:], uint16(len(pp)-1))
	bplong(a[7:], uint32(end0))
	bplong(a[11:], uint32(end1))
	bplong(a[15:], uint32(radius))
	bplong(a[19:], uint32(src.id))
	bplong(a[23:], uint32(sp.X))
	bplong(a[27:], uint32(sp.Y))
	o := 31
	ox, oy := 0, 0
	for _, p := range pp {
		o += addcoord(a[o:], ox, p.X)
		o += addcoord(a[o:], oy, p.Y)
		ox, oy = p.X, p.Y
	}
	d := dst.Display
	d.buf = d.buf[:len(d.buf)-m+o]
}

// Poly draws a general open polygon; it is conceptually equivalent to a series of
// calls to Line joining adjacent points in the array of points p.
// The ends end0 and end1 of the polygon are specified as in the Line method;
// see the EndSquare and Arrow documentation.
// Interior lines are terminated with EndDisc to make smooth joins.
// The source is aligned so that sp corresponds to p[0].
func (dst *Image) Poly(p []Point, end0, end1 End, radius int, src *Image, sp Point) {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	dopoly('p', dst, p, end0, end1, radius, src, sp, SoverD)
}

// PolyOp is like Poly but specifies an explicit Porter-Duff operator.
func (dst *Image) PolyOp(p []Point, end0, end1 End, radius int, src *Image, sp Point, op Op) {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	dopoly('p', dst, p, end0, end1, radius, src, sp, op)
}

// FillPoly is like Poly but fills in the resulting polygon rather than outlining it.
// The source is aligned so sp corresponds to p[0].
// The winding rule parameter wind resolves ambiguities about what to fill if the
// polygon is self-intersecting.  If wind is ^0, a pixel is inside the polygon
// if the polygon's winding number about the point is non-zero.
// If wind is 1, a pixel is inside if the winding number is odd.
// Complementary values (0 or ^1) cause outside pixels to be filled.
// The meaning of other values is undefined.
// The polygon is closed with a line if necessary.
func (dst *Image) FillPoly(p []Point, wind int, src *Image, sp Point) {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	dopoly('P', dst, p, End(wind), 0, 0, src, sp, SoverD)
}

// FillPolyOp is like FillPoly but specifies an explicit Porter-Duff operator.
func (dst *Image) FillPolyOp(p []Point, wind int, src *Image, sp Point, op Op) {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	dopoly('P', dst, p, End(wind), 0, 0, src, sp, op)
}
