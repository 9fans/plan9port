package draw

import "image"

// A Point is an X, Y coordinate pair, a location in an Image such as the display.
// The coordinate system has X increasing to the right and Y increasing down.
type Point = image.Point

// A Rectangle is a rectangular area in an image.
// By definition, Min.X ≤ Max.X and Min.Y ≤ Max.Y.
// By convention, the right (Max.X) and bottom (Max.Y)
// edges are excluded from the represented rectangle,
// so abutting rectangles have no points in common.
// Thus, max contains the coordinates of the first point beyond the rectangle.
// If Min.X > Max.X or Min.Y > Max.Y, the rectangle contains no points.
type Rectangle = image.Rectangle

// Pt is shorthand for Point{X: x, Y: y}.
func Pt(x, y int) Point {
	return Point{X: x, Y: y}
}

// Rect is shorthand for Rectangle{Min: Pt(x0, y0), Max: Pt(x1, y1)}.
// Unlike image.Rect, Rect does not swap x1 ↔ x2 or y1 ↔ y2
// to put them in canonical order.
// In this package, a Rectangle with x1 > x2 or y1 > y2
// is an empty rectangle.
func Rect(x1, y1, x2, y2 int) Rectangle {
	return Rectangle{Pt(x1, y1), Pt(x2, y2)}
}

// Rpt is shorthand for Rectangle{min, max}.
func Rpt(min, max Point) Rectangle {
	return Rectangle{Min: min, Max: max}
}

// ZP is the zero Point.
var ZP Point

// ZR is the zero Rectangle.
var ZR Rectangle
