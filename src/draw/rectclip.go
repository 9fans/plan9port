package draw

// RectClip attempts to clip *rp to be within b.
// If any of *rp overlaps b, RectClip modifies *rp to denote
// the overlapping portion and returns true.
// Otherwise, when *rp and b do not overlap,
// RectClip leaves *rp unmodified and returns false.
func RectClip(rp *Rectangle, b Rectangle) bool {
	if !RectXRect(*rp, b) {
		return false
	}

	if rp.Min.X < b.Min.X {
		rp.Min.X = b.Min.X
	}
	if rp.Min.Y < b.Min.Y {
		rp.Min.Y = b.Min.Y
	}
	if rp.Max.X > b.Max.X {
		rp.Max.X = b.Max.X
	}
	if rp.Max.Y > b.Max.Y {
		rp.Max.Y = b.Max.Y
	}
	return true
}

// RectXRect reports whether r and s cross, meaning they share any point
// or r is a zero-width or zero-height rectangle inside s.
// Note that the zero-sized cases make RectXRect(r, s) different from r.Overlaps(s).
func RectXRect(r, s Rectangle) bool {
	return r.Min.X < s.Max.X && s.Min.X < r.Max.X && r.Min.Y < s.Max.Y && s.Min.Y < r.Max.Y
}

// RectInRect reports whether r is entirely contained in s.
// RectInRect(r, s) differs from r.In(s)
// in its handling of zero-width or zero-height rectangles.
func RectInRect(r, s Rectangle) bool {
	return s.Min.X <= r.Min.X && r.Max.X <= s.Max.X && s.Min.Y <= r.Min.Y && r.Max.Y <= s.Max.Y
}

// CombineRect overwrites *r1 with the smallest rectangle
// enclosing both *r1 and r2.
// CombineRect(r1, r2) differs from *r1 = r1.Union(r2)
// in its handling of zero-width or zero-height rectangles.
func CombineRect(r1 *Rectangle, r2 Rectangle) {
	if r1.Min.X > r2.Min.X {
		r1.Min.X = r2.Min.X
	}
	if r1.Min.Y > r2.Min.Y {
		r1.Min.Y = r2.Min.Y
	}
	if r1.Max.X < r2.Max.X {
		r1.Max.X = r2.Max.X
	}
	if r1.Max.Y < r2.Max.Y {
		r1.Max.Y = r2.Max.Y
	}
}
