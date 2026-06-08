package draw

import "fmt"

// Cload replaces the specified rectangle in image i with the compressed data,
// returning the number of bytes copied from data.
// It is an error if data does not contain pixels for the entire rectangle.
//
// See the package documentation for details about the compressed data format.
// Each call to Cload must pass data starting at the beginning of a compressed
// data block, specifically the y coordinate and data length for the block.
func (dst *Image) Cload(r Rectangle, data []byte) (int, error) {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	i := dst
	if !r.In(i.R) {
		return 0, fmt.Errorf("cloadimage: bad rectangle")
	}

	miny := r.Min.Y
	m := 0
	ncblock := compblocksize(r, i.Depth)
	for miny != r.Max.Y {
		maxy := atoi(data[0*12:])
		nb := atoi(data[1*12:])
		if maxy <= miny || r.Max.Y < maxy {
			return 0, fmt.Errorf("creadimage: bad maxy %d", maxy)
		}
		data = data[2*12:]
		m += 2 * 12
		if nb <= 0 || ncblock < nb || nb > len(data) {
			return 0, fmt.Errorf("creadimage: bad count %d", nb)
		}
		// TODO: error check?
		a := i.Display.bufimage(21 + nb)
		a[0] = 'Y'
		bplong(a[1:], i.id)
		bplong(a[5:], uint32(r.Min.Y))
		bplong(a[9:], uint32(miny))
		bplong(a[13:], uint32(r.Max.Y))
		bplong(a[17:], uint32(maxy))
		copy(a[21:], data)
		miny = maxy
		data = data[nb:]
		m += nb
	}
	return m, nil
}
