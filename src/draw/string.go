package draw

// String draws the string in the specified font, placing the upper left corner at p.
// It draws the text using src, with sp aligned to p, using operation SoverD onto dst.
// String returns a Point that is the position of the next character that would be drawn
// if the string were longer.
//
// For characters with undefined or zero-width images in the font,
// the character at font position 0 (NUL) is drawn instead.
func (dst *Image) String(p Point, src *Image, sp Point, f *Font, s string) Point {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	return _string(dst, p, src, sp, f, s, nil, nil, dst.Clipr, nil, ZP, SoverD)
}

// StringOp is like String but specifies an explicit Porter-Duff operator.
func (dst *Image) StringOp(p Point, src *Image, sp Point, f *Font, s string, op Op) Point {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	return _string(dst, p, src, sp, f, s, nil, nil, dst.Clipr, nil, ZP, op)
}

// StringBg is like String but draws the background bg behind the characters,
// with bgp aligned to p, before drawing the text.
func (dst *Image) StringBg(p Point, src *Image, sp Point, f *Font, s string, bg *Image, bgp Point) Point {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	return _string(dst, p, src, sp, f, s, nil, nil, dst.Clipr, bg, bgp, SoverD)
}

// StringBgOp is like StringBg but specifies an explicit Porter-Duff operator.
func (dst *Image) StringBgOp(p Point, src *Image, sp Point, f *Font, s string, bg *Image, bgp Point, op Op) Point {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	return _string(dst, p, src, sp, f, s, nil, nil, dst.Clipr, bg, bgp, op)
}

// Runes is like String but accepts a rune slice instead of a string.
func (dst *Image) Runes(p Point, src *Image, sp Point, f *Font, r []rune) Point {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	return _string(dst, p, src, sp, f, "", nil, r, dst.Clipr, nil, ZP, SoverD)
}

// RunesOp is like StringOp but accepts a rune slice instead of a string.
func (dst *Image) RunesOp(p Point, src *Image, sp Point, f *Font, r []rune, op Op) Point {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	return _string(dst, p, src, sp, f, "", nil, r, dst.Clipr, nil, ZP, op)
}

// RunesBg is like StringBg but accepts a rune slice instead of a string.
func (dst *Image) RunesBg(p Point, src *Image, sp Point, f *Font, r []rune, bg *Image, bgp Point) Point {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	return _string(dst, p, src, sp, f, "", nil, r, dst.Clipr, bg, bgp, SoverD)
}

// RunesBgOp is like StringBgOp but accepts a rune slice instead of a string.
func (dst *Image) RunesBgOp(p Point, src *Image, sp Point, f *Font, r []rune, bg *Image, bgp Point, op Op) Point {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	return _string(dst, p, src, sp, f, "", nil, r, dst.Clipr, bg, bgp, op)
}

// Bytes is like String but accepts a UTF-8-encoded byte slice instead of a string.
func (dst *Image) Bytes(p Point, src *Image, sp Point, f *Font, b []byte) Point {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	return _string(dst, p, src, sp, f, "", b, nil, dst.Clipr, nil, ZP, SoverD)
}

// BytesOp is like StringOp but accepts a UTF-8-encoded byte slice instead of a string.
func (dst *Image) BytesOp(p Point, src *Image, sp Point, f *Font, b []byte, op Op) Point {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	return _string(dst, p, src, sp, f, "", b, nil, dst.Clipr, nil, ZP, op)
}

// BytesBg is like StringBg but accepts a UTF-8-encoded byte slice instead of a string.
func (dst *Image) BytesBg(p Point, src *Image, sp Point, f *Font, b []byte, bg *Image, bgp Point) Point {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	return _string(dst, p, src, sp, f, "", b, nil, dst.Clipr, bg, bgp, SoverD)
}

// BytesBgOp is like StringBgOp but accepts a UTF-8-encoded byte slice instead of a string.
func (dst *Image) BytesBgOp(p Point, src *Image, sp Point, f *Font, b []byte, bg *Image, bgp Point, op Op) Point {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	return _string(dst, p, src, sp, f, "", b, nil, dst.Clipr, bg, bgp, op)
}

func _string(dst *Image, p Point, src *Image, sp Point, f *Font, s string, b []byte, r []rune, clipr Rectangle, bg *Image, bgp Point, op Op) Point {
	var in input
	in.init(s, b, r)
	const Max = 100
	cbuf := make([]uint16, Max)
	var sf *subfont
	for !in.done {
		max := Max
		n, wid, subfontname := cachechars(f, &in, cbuf, max)
		if n > 0 {
			setdrawop(dst.Display, op)
			m := 47 + 2*n
			if bg != nil {
				m += 4 + 2*4
			}
			b := dst.Display.bufimage(m)
			if bg != nil {
				b[0] = 'x'
			} else {
				b[0] = 's'
			}
			bplong(b[1:], uint32(dst.id))
			bplong(b[5:], uint32(src.id))
			bplong(b[9:], uint32(f.cacheimage.id))
			bplong(b[13:], uint32(p.X))
			bplong(b[17:], uint32(p.Y+f.Ascent))
			bplong(b[21:], uint32(clipr.Min.X))
			bplong(b[25:], uint32(clipr.Min.Y))
			bplong(b[29:], uint32(clipr.Max.X))
			bplong(b[33:], uint32(clipr.Max.Y))
			bplong(b[37:], uint32(sp.X))
			bplong(b[41:], uint32(sp.Y))
			bpshort(b[45:], uint16(n))
			b = b[47:]
			if bg != nil {
				bplong(b, uint32(bg.id))
				bplong(b[4:], uint32(bgp.X))
				bplong(b[8:], uint32(bgp.Y))
				b = b[12:]
			}
			for i, c := range cbuf[:n] {
				bpshort(b[2*i:], c)
			}
			p.X += wid
			bgp.X += wid
			agefont(f)
		}
		if subfontname != "" {
			sf.free()
			var err error
			sf, err = getsubfont(f.Display, subfontname)
			if err != nil {
				if f.Display != nil && f != f.Display.Font {
					f = f.Display.Font
					continue
				}
				break
			}
			/*
			 * must not free sf until cachechars has found it in the cache
			 * and picked up its own reference.
			 */
		}
	}
	return p
}
