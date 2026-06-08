package draw

import "fmt"

// Unload reads a rectangle of pixels from image i into data,
// returning the number of bytes copied to data.
// It is an error if data does not contain pixels for the entire rectangle.
//
// See the Load method's documentation for details about the data format.
func (i *Image) Unload(r Rectangle, data []byte) (n int, err error) {
	i.Display.mu.Lock()
	defer i.Display.mu.Unlock()
	return i.unload(r, data)
}

func (src *Image) unload(r Rectangle, data []byte) (n int, err error) {
	i := src
	if !r.In(i.R) {
		return 0, fmt.Errorf("image.Unload: bad rectangle")
	}
	bpl := BytesPerLine(r, i.Depth)
	if len(data) < bpl*r.Dy() {
		return 0, fmt.Errorf("image.Unload: buffer too small")
	}

	d := i.Display
	d.flush(false) // make sure next flush is only us
	ntot := 0
	for r.Min.Y < r.Max.Y {
		a := d.bufimage(1 + 4 + 4*4)
		dy := 8000 / bpl
		if dy <= 0 {
			return 0, fmt.Errorf("unloadimage: image too wide")
		}
		if dy > r.Dy() {
			dy = r.Dy()
		}
		a[0] = 'r'
		bplong(a[1:], uint32(i.id))
		bplong(a[5:], uint32(r.Min.X))
		bplong(a[9:], uint32(r.Min.Y))
		bplong(a[13:], uint32(r.Max.X))
		bplong(a[17:], uint32(r.Min.Y+dy))
		if err := d.flush(false); err != nil {
			return ntot, err
		}
		n, err := d.conn.ReadDraw(data[ntot:])
		ntot += n
		if err != nil {
			return ntot, err
		}
		r.Min.Y += dy
	}
	return ntot, nil
}
