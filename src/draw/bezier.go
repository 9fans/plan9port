package draw

func appendpt(l *[]Point, p Point) {
	*l = append(*l, p)
}

func normsq(p Point) int {
	return p.X*p.X + p.Y*p.Y
}

func psdist(p, a, b Point) int {
	p = p.Sub(a)
	b = b.Sub(a)
	num := p.X*b.X + p.Y*b.Y
	if num <= 0 {
		return normsq(p)
	}
	den := normsq(b)
	if num >= den {
		return normsq(b.Sub(p))
	}
	return normsq(b.Mul(num).Div(den).Sub(p))
}

/*
 * Convert cubic Bezier curve control points to polyline
 * vertices.  Leaves the last vertex off, so you can continue
 * with another curve.
 */
func bpts1(l *[]Point, p0, p1, p2, p3 Point, scale int) {
	tp0 := p0.Div(scale)
	tp1 := p1.Div(scale)
	tp2 := p2.Div(scale)
	tp3 := p3.Div(scale)
	if psdist(tp1, tp0, tp3) <= 1 && psdist(tp2, tp0, tp3) <= 1 {
		appendpt(l, tp0)
		appendpt(l, tp1)
		appendpt(l, tp2)
	} else {
		/*
		 * if scale factor is getting too big for comfort,
		 * rescale now & concede the rounding error
		 */
		if scale > 1<<12 {
			p0 = tp0
			p1 = tp1
			p2 = tp2
			p3 = tp3
			scale = 1
		}
		p01 := p0.Add(p1)
		p12 := p1.Add(p2)
		p23 := p2.Add(p3)
		p012 := p01.Add(p12)
		p123 := p12.Add(p23)
		p0123 := p012.Add(p123)
		bpts1(l, p0.Mul(8), p01.Mul(4), p012.Mul(2), p0123, scale*8)
		bpts1(l, p0123, p123.Mul(2), p23.Mul(4), p3.Mul(8), scale*8)
	}
}

func bpts(l *[]Point, p0 Point, p1 Point, p2 Point, p3 Point) {
	bpts1(l, p0, p1, p2, p3, 1)
}

func bezierpts(p0 Point, p1 Point, p2 Point, p3 Point) []Point {
	var l []Point
	bpts(&l, p0, p1, p2, p3)
	appendpt(&l, p3)
	return l
}

func _bezsplinepts(l *[]Point, pt []Point) {
	if len(pt) < 3 {
		return
	}
	ep := pt[len(pt)-3:]
	periodic := pt[0] == ep[2]
	var a, b, c, d Point
	if periodic {
		a = ep[1].Add(pt[0]).Div(2)
		b = ep[1].Add(pt[0].Mul(5)).Div(6)
		c = pt[0].Mul(5).Add(pt[1]).Div(6)
		d = pt[0].Add(pt[1]).Div(2)
		bpts(l, a, b, c, d)
	}
	for p := pt; len(p) >= len(ep); p = p[1:] {
		if len(p) == len(pt) && !periodic {
			a = p[0]
			b = p[0].Add(p[1].Mul(2)).Div(3)
		} else {
			a = p[0].Add(p[1]).Div(2)
			b = p[0].Add(p[1].Mul(5)).Div(6)
		}
		if len(p) == len(ep) && !periodic {
			c = p[1].Mul(2).Add(p[2]).Div(3)
			d = p[2]
		} else {
			c = p[1].Mul(5).Add(p[2]).Div(6)
			d = p[1].Add(p[2]).Div(2)
		}
		bpts(l, a, b, c, d)
	}
	appendpt(l, d)
}

func bezsplinepts(pt []Point) []Point {
	var l []Point
	_bezsplinepts(&l, pt)
	return l
}

// Bezier draws the cubic Bezier curve defined by Points a, b, c, and d.
// The end styles are determined by end0 and end1; the thickness
// of the curve is 1+2*thick.
// The source is aligned so sp in src corresponds to a in dst.
func (dst *Image) Bezier(a, b, c, d Point, end0, end1 End, radius int, src *Image, sp Point) {
	dst.BezierOp(a, b, c, d, end0, end1, radius, src, sp, SoverD)
}

// BezierOp is like Bezier but specifies an explicit Porter-Duff operator.
func (dst *Image) BezierOp(a, b, c, d Point, end0, end1 End, radius int, src *Image, sp Point, op Op) {
	l := bezierpts(a, b, c, d)
	if len(l) > 0 {
		dst.PolyOp(l, end0, end1, radius, src, sp.Sub(a).Add(l[0]), op)
	}
}

// BSpline takes the same arguments as Poly but draws a quadratic B-spline
// rather than a polygon.  If the first and last points in p are equal,
// the spline has periodic end conditions.
func (dst *Image) BSpline(pt []Point, end0, end1 End, radius int, src *Image, sp Point) {
	dst.BSplineOp(pt, end0, end1, radius, src, sp, SoverD)
}

// BSplineOp is like BSpline but specifies an explicit Porter-Duff operator.
func (dst *Image) BSplineOp(pt []Point, end0, end1 End, radius int, src *Image, sp Point, op Op) {
	var l []Point
	_bezsplinepts(&l, pt)
	if len(l) > 0 {
		dst.PolyOp(l, end0, end1, radius, src, sp.Sub(pt[0]).Add(l[0]), op)
	}
}

// FillBezier is like FillPoly but fills the cubic Bezier curve defined by a, b, c, d.
func (dst *Image) FillBezier(a, b, c, d Point, wind int, src *Image, sp Point) {
	dst.FillBezierOp(a, b, c, d, wind, src, sp, SoverD)
}

// FillBezierOp is like FillBezier but specifies an explicit Porter-Duff operator.
func (dst *Image) FillBezierOp(a, b, c, d Point, wind int, src *Image, sp Point, op Op) {
	l := bezierpts(a, b, c, d)
	if len(l) > 0 {
		dst.FillPolyOp(l, wind, src, sp.Sub(a).Add(l[0]), op)
	}
}

// FillBSpline is like FillPoly but fills the quadratic B-spline defined by p,
// not the polygon defined by p.
// The spline is closed with a line if necessary.
func (dst *Image) FillBSpline(pt []Point, wind int, src *Image, sp Point) {
	dst.FillBSplineOp(pt, wind, src, sp, SoverD)
}

// FillBSplineOp is like FillBSpline but specifies an explicit Porter-Duff operator.
func (dst *Image) FillBSplineOp(pt []Point, wind int, src *Image, sp Point, op Op) {
	var l []Point
	_bezsplinepts(&l, pt)
	if len(l) > 0 {
		dst.FillPolyOp(l, wind, src, sp.Sub(pt[0]).Add(l[0]), op)
	}
}
