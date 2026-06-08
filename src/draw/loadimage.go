package draw

import "fmt"

// Load replaces the specified rectangle in image dst with the data,
// returning the number of bytes copied from data.
// It is an error if data is too small to supply pixels for the entire rectangle.
//
// In data, the pixels are presented one horizontal line at a time,
// starting with the top-left pixel of r. Each scan line starts with a new byte
// in the array, leaving the last byte of the previous line partially empty
// if necessary when i.Depth < 8. Pixels are packed as tightly as possible
// within a line, regardless of the rectangle being extracted.
// Bytes are filled from most to least significant bit order,
// as the x coordinate increases, aligned so that x = r.Min would appear as
// the leftmost pixel of its byte.
// Thus, for depth 1, the pixel at x offset 165 within the rectangle
// will be in a data byte at bit-position 0x04 regardless of the overall
// rectangle: 165 mod 8 equals 5, and 0x80 >> 5 equals 0x04.
func (dst *Image) Load(r Rectangle, data []byte) (n int, err error) {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	return dst.load(r, data)
}

func (dst *Image) load(r Rectangle, data []byte) (int, error) {
	i := dst
	chunk := i.Display.bufsize - 64
	if !r.In(i.R) {
		return 0, fmt.Errorf("loadimage: bad rectangle")
	}
	bpl := BytesPerLine(r, i.Depth)
	n := bpl * r.Dy()
	if n > len(data) {
		return 0, fmt.Errorf("loadimage: insufficient data")
	}
	ndata := 0
	for r.Max.Y > r.Min.Y {
		dy := r.Max.Y - r.Min.Y
		if dy*bpl > chunk {
			dy = chunk / bpl
		}
		if dy <= 0 {
			return 0, fmt.Errorf("loadimage: image too wide for buffer")
		}
		n := dy * bpl
		a := i.Display.bufimage(21 + n)
		a[0] = 'y'
		bplong(a[1:], uint32(i.id))
		bplong(a[5:], uint32(r.Min.X))
		bplong(a[9:], uint32(r.Min.Y))
		bplong(a[13:], uint32(r.Max.X))
		bplong(a[17:], uint32(r.Min.Y+dy))
		copy(a[21:], data)
		ndata += n
		data = data[n:]
		r.Min.Y += dy
	}
	if err := i.Display.flush(false); err != nil {
		return ndata, err
	}
	return ndata, nil
}
