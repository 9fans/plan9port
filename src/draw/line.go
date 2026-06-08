package draw

// An End specifies how to draw the end of a line.
type End int

// EndSquare terminates the line perpendicularly to the direction of the
// line; a thick line with EndSquare on both ends will be a rectangle.
// EndDisc terminates the line by drawing a disc of diameter 1+2*thick
// centered on the end point. EndArrow terminates the line with an
// arrowhead whose tip touches the endpoint.
// See the Arrow function for more control over arrow shapes.
const (
	// EndSquare terminates the line perpindicularly
	// to the direction of the line; a thick line with EndSquare
	// on both ends will be a rectangle.
	EndSquare End = 0

	// EndDisc terminates the line by drawing a disc of diameter 1+2*thick
	// centered on the end point.
	EndDisc End = 1

	// EndArrow terminates the line with an arrowhead whose tip
	// touches the endpoint.
	// Use the Arrow function for more control over the arrow shape.
	EndArrow End = 2
)

// Arrow permits explicit control of the shape of a line-ending arrow.
// If all three parameters are zero, it produces the default arrowhead.
// Otherwise, a sets the distance along line from the end of the regular line
// to the tip, b sets the distance along line from the barb to the tip,
// and c sets the distance perpendicular to the line from the edge of the line
// to the tip of the barb, all in pixels.
func Arrow(a, b, c int) End {
	return EndArrow | End(a<<5|b<<14|c<<23)
}

// Line draws in dst a line of width 1+2*thick pixels joining p0 and p1.
// The line is drawn using pixels from the src image aligned so sp in the
// source corresponds to p0 in the destination. The line touches both p0
// and p1.  End0 and end1 specify how the ends of the line are drawn;
// see the documentation for EndSquare and the Arrow function.
//
// Line and the other geometrical operators are equivalent to calls to
// Draw using a mask produced by the geometric procedure.
func (dst *Image) Line(p0, p1 Point, end0, end1 End, thick int, src *Image, sp Point) {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	dst.lineOp(p0, p1, end0, end1, thick, src, sp, SoverD)
}

// LineOp draws a line in the source color from p0 to p1, of thickness
// 1+2*radius, with the specified ends. The source is aligned so sp corresponds
// to p0. See the Plan 9 documentation for more information.
func (dst *Image) LineOp(p0, p1 Point, end0, end1 End, radius int, src *Image, sp Point, op Op) {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	dst.lineOp(p0, p1, end0, end1, radius, src, sp, op)
}

func (dst *Image) lineOp(p0, p1 Point, end0, end1 End, radius int, src *Image, sp Point, op Op) {
	setdrawop(dst.Display, op)
	a := dst.Display.bufimage(1 + 4 + 2*4 + 2*4 + 4 + 4 + 4 + 4 + 2*4)
	a[0] = 'L'
	bplong(a[1:], uint32(dst.id))
	bplong(a[5:], uint32(p0.X))
	bplong(a[9:], uint32(p0.Y))
	bplong(a[13:], uint32(p1.X))
	bplong(a[17:], uint32(p1.Y))
	bplong(a[21:], uint32(end0))
	bplong(a[25:], uint32(end1))
	bplong(a[29:], uint32(radius))
	bplong(a[33:], uint32(src.id))
	bplong(a[37:], uint32(sp.X))
	bplong(a[41:], uint32(sp.Y))
}
