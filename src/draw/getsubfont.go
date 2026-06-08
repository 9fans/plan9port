package draw

import (
	"bytes"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"strings"
)

func getsubfont(d *Display, name string) (*subfont, error) {
	scale, fname := parsefontscale(name)
	data, err := ioutil.ReadFile(fname)
	if err != nil && strings.HasPrefix(fname, "/mnt/font/") {
		data1, err1 := fontPipe(fname[len("/mnt/font/"):])
		if err1 == nil {
			data, err = data1, err1
		}
	}
	if err != nil {
		fmt.Fprintf(os.Stderr, "getsubfont: %v\n", err)
		return nil, err
	}
	f, err := d.readSubfont(name, bytes.NewReader(data), nil)
	if err != nil {
		fmt.Fprintf(os.Stderr, "getsubfont: can't read %s: %v\n", fname, err)
	}
	if scale > 1 {
		scalesubfont(f, scale)
	}
	return f, err
}

func scalesubfont(f *subfont, scale int) {
	r := f.Bits.R
	r2 := r
	r2.Min.X *= scale
	r2.Min.Y *= scale
	r2.Max.X *= scale
	r2.Max.Y *= scale

	srcn := BytesPerLine(r, f.Bits.Depth)
	src := make([]byte, srcn)
	dstn := BytesPerLine(r2, f.Bits.Depth)
	dst := make([]byte, dstn)
	i, err := allocImage(f.Bits.Display, nil, r2, f.Bits.Pix, false, Black, 0, 0)
	if err != nil {
		log.Fatalf("allocimage: %v", err)
	}
	for y := r.Min.Y; y < r.Max.Y; y++ {
		_, err := f.Bits.unload(Rect(r.Min.X, y, r.Max.X, y+1), src)
		if err != nil {
			log.Fatalf("unloadimage: %v", err)
		}
		for i := range dst {
			dst[i] = 0
		}
		pack := 8 / f.Bits.Depth
		mask := byte(1<<uint(f.Bits.Depth) - 1)
		for x := 0; x < r.Dx(); x++ {
			v := ((src[x/pack] << uint((x%pack)*f.Bits.Depth)) >> uint(8-f.Bits.Depth)) & mask
			for j := 0; j < scale; j++ {
				x2 := x*scale + j
				dst[x2/pack] |= v << uint(8-f.Bits.Depth) >> uint((x2%pack)*f.Bits.Depth)
			}
		}
		for j := 0; j < scale; j++ {
			i.load(Rect(r2.Min.X, y*scale+j, r2.Max.X, y*scale+j+1), dst)
		}
	}
	f.Bits.free()
	f.Bits = i
	f.Height *= scale
	f.Ascent *= scale

	for j := 0; j < f.N; j++ {
		p := &f.Info[j]
		p.X *= scale
		p.Top *= uint8(scale)
		p.Bottom *= uint8(scale)
		p.Left *= int8(scale)
		p.Width *= uint8(scale)
	}
}
