package draw

// A Cursor describes a single cursor.
//
// The arrays White and Black are arranged in rows, two bytes per row,
// left to right in big-endian order, to give 16 rows of 16 bits each.
// A cursor is displayed on the screen by adding Point to the current
// mouse position, then using White as a mask to draw white at
// the pixels where White is 1, and then drawing black at the pixels
// where Black is 1.
type Cursor struct {
	Point
	White [2 * 16]uint8
	Black [2 * 16]uint8
}

// A Cursor2 describes a single high-DPI cursor,
// with twice the pixels in each direction as a Cursor
// (32 rows of 32 bits each).
type Cursor2 struct {
	Point
	White [4 * 32]uint8
	Black [4 * 32]uint8
}

var expand = [16]uint8{
	0x00, 0x03, 0x0c, 0x0f,
	0x30, 0x33, 0x3c, 0x3f,
	0xc0, 0xc3, 0xcc, 0xcf,
	0xf0, 0xf3, 0xfc, 0xff,
}

// ScaleCursor returns a high-DPI version of c.
func ScaleCursor(c Cursor) Cursor2 {
	var c2 Cursor2
	c2.X = 2 * c.X
	c2.Y = 2 * c.Y
	for y := 0; y < 16; y++ {
		c2.White[8*y+4] = expand[c.White[2*y]>>4]
		c2.White[8*y] = c2.White[8*y+4]
		c2.Black[8*y+4] = expand[c.Black[2*y]>>4]
		c2.Black[8*y] = c2.Black[8*y+4]
		c2.White[8*y+5] = expand[c.White[2*y]&15]
		c2.White[8*y+1] = c2.White[8*y+5]
		c2.Black[8*y+5] = expand[c.Black[2*y]&15]
		c2.Black[8*y+1] = c2.Black[8*y+5]
		c2.White[8*y+6] = expand[c.White[2*y+1]>>4]
		c2.White[8*y+2] = c2.White[8*y+6]
		c2.Black[8*y+6] = expand[c.Black[2*y+1]>>4]
		c2.Black[8*y+2] = c2.Black[8*y+6]
		c2.White[8*y+7] = expand[c.White[2*y+1]&15]
		c2.White[8*y+3] = c2.White[8*y+7]
		c2.Black[8*y+7] = expand[c.Black[2*y+1]&15]
		c2.Black[8*y+3] = c2.Black[8*y+7]
	}
	return c2
}
