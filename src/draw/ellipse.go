package draw

func doellipse(cmd byte, dst *Image, c Point, xr, yr, thick int, src *Image, sp Point, alpha uint32, phi int, op Op) {
	setdrawop(dst.Display, op)
	a := dst.Display.bufimage(1 + 4 + 4 + 2*4 + 4 + 4 + 4 + 2*4 + 2*4)
	a[0] = cmd
	bplong(a[1:], dst.id)
	bplong(a[5:], src.id)
	bplong(a[9:], uint32(c.X))
	bplong(a[13:], uint32(c.Y))
	bplong(a[17:], uint32(xr))
	bplong(a[21:], uint32(yr))
	bplong(a[25:], uint32(thick))
	bplong(a[29:], uint32(sp.X))
	bplong(a[33:], uint32(sp.Y))
	bplong(a[37:], alpha)
	bplong(a[41:], uint32(phi))
}

// Ellipse draws in dst an ellipse centered on c with horizontal and vertical
// semiaxes a and b. The source is aligned so sp in src corresponds to c in dst.
// The ellipse is drawn with thickness 1+2*thick.
func (dst *Image) Ellipse(c Point, a, b, thick int, src *Image, sp Point) {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	doellipse('e', dst, c, a, b, thick, src, sp, 0, 0, SoverD)
}

// EllipseOp is like Ellipse but specifies an explicit Porter-Duff operator.
func (dst *Image) EllipseOp(c Point, a, b, thick int, src *Image, sp Point, op Op) {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	doellipse('e', dst, c, a, b, thick, src, sp, 0, 0, op)
}

// FillEllipse is like Ellipse but fills the ellipse rather than outlining it.
func (dst *Image) FillEllipse(c Point, a, b int, src *Image, sp Point) {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	doellipse('E', dst, c, a, b, 0, src, sp, 0, 0, SoverD)
}

// FillEllipseOp is like FillEllipse but specifies an explicit Porter-Duff operator.
func (dst *Image) FillEllipseOp(c Point, a, b int, src *Image, sp Point, op Op) {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	doellipse('E', dst, c, a, b, 0, src, sp, 0, 0, op)
}

// Arc is like Ellipse but draws only that portion of the ellipse starting at angle alpha
// and extending through an angle of phi. The angles are measured in degrees
// counterclockwise from the positive x axis.
func (dst *Image) Arc(c Point, a, b, thick int, src *Image, sp Point, alpha, phi int) {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	doellipse('e', dst, c, a, b, thick, src, sp, uint32(alpha)|1<<31, phi, SoverD)
}

// ArcOp is like Arc but specifies an explicit Porter-Duff operator.
func (dst *Image) ArcOp(c Point, a, b, thick int, src *Image, sp Point, alpha, phi int, op Op) {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	doellipse('e', dst, c, a, b, thick, src, sp, uint32(alpha)|1<<31, phi, op)
}

// FillArc is like Arc but fills the sector with the source color.
func (dst *Image) FillArc(c Point, a, b int, src *Image, sp Point, alpha, phi int) {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	doellipse('E', dst, c, a, b, 0, src, sp, uint32(alpha)|1<<31, phi, SoverD)
}

// FillArcOp is like FillArc but specifies an explicit Porter-Duff operator.
func (dst *Image) FillArcOp(c Point, a, b int, src *Image, sp Point, alpha, phi int, op Op) {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	doellipse('E', dst, c, a, b, 0, src, sp, uint32(alpha)|1<<31, phi, op)
}
