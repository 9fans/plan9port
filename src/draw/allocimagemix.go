package draw

// AllocImageMix is used to allocate background colors.
// It returns a 1×1 replicated image whose pixel is the result of
// mixing the two colors in a one to three ratio.
// On 8-bit color-mapped displays, it returns a 2×2 replicated image
// with one pixel colored the color one and the other three
// with three.  (This simulates a wider range of tones than can
// be represented by a single pixel value on a color-mapped
// display.)
func (d *Display) AllocImageMix(color1, color3 Color) *Image {
	d.mu.Lock()
	defer d.mu.Unlock()
	if d.ScreenImage.Depth <= 8 { // create a 2x2 texture
		t, _ := d.allocImage(Rect(0, 0, 1, 1), d.ScreenImage.Pix, false, color1)
		b, _ := d.allocImage(Rect(0, 0, 2, 2), d.ScreenImage.Pix, true, color3)
		b.draw(Rect(0, 0, 1, 1), t, nil, ZP)
		t.free()
		return b
	}

	// use a solid color, blended using alpha
	if d.qmask == nil {
		d.qmask, _ = d.allocImage(Rect(0, 0, 1, 1), GREY8, true, 0x3F3F3FFF)
	}
	t, _ := d.allocImage(Rect(0, 0, 1, 1), d.ScreenImage.Pix, true, color1)
	b, _ := d.allocImage(Rect(0, 0, 1, 1), d.ScreenImage.Pix, true, color3)
	b.draw(b.R, t, d.qmask, ZP)
	t.free()
	return b
}
