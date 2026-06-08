package draw

// ReplXY clips x to be in the half-open interval [min, max) y adding or
// subtracking a multiple of max - min.
// That is, assuming [min, max) specify the base of an infinite tiling
// of the integer line, ReplXY returns the value of the image of x that appears
// in the interval.
func ReplXY(min, max, x int) int {
	sx := (x - min) % (max - min)
	if sx < 0 {
		sx += max - min
	}
	return sx + min
}

// Repl clips the point p to be within the rectangle r by translating p
// horizontally by an integer multiple of the rectangle width
// and vertically by an integer multiple of the rectangle height.
// That is, it returns the point corresponding to the image of p that appears inside
// the base rectangle r, which represents a tiling of the plane.
func Repl(r Rectangle, p Point) Point {
	return Point{
		ReplXY(r.Min.X, r.Max.X, p.X),
		ReplXY(r.Min.Y, r.Max.Y, p.Y),
	}
}
