// Package drawtest provides helpers for writing tests of graphical programs.
// The basic approach is to run against a devdraw that writes every flushed
// display image to a new graphics file and then test those images against
// expected outputs recorded from a known good version of the program.
package drawtest

import (
	"bytes"
	"fmt"
	"image"
	"image/color"
	"image/gif"
	"image/png"
	"iter"
	"maps"
	"os"
	"path/filepath"
	"slices"
	"sort"
	"strconv"
	"strings"
	"testing"
)

// Frames loads output images from the files name.0, name.1, ...,
// and returns them in numeric order.
func Frames(t *testing.T, name string) []image.Image {
	t.Helper()
	matches, _ := filepath.Glob(name + ".*")
	var nums []int
	for _, file := range matches {
		num, err := strconv.Atoi(file[len(name)+1:])
		if err != nil {
			continue
		}
		nums = append(nums, num)
	}
	if len(nums) == 0 {
		t.Errorf("no output image files found")
		return nil
	}
	sort.Ints(nums)
	if nums[len(nums)-1] != len(nums)-1 {
		t.Errorf("sparse output numbering: %v", nums)
		return nil
	}
	// devdraw writes only the rectangle each drawing operation changed (see
	// test_output); each file is that diff, positioned at its rectangle. We
	// return the diffs as-is rather than compositing them into full frames: the
	// comparison and the golden GIF both work directly on the diffs (see Check
	// and encodeGIF), which keeps reconstruction -- a full-window copy per frame
	// -- out of the hot path. The diffs still composite visually when the GIF is
	// viewed, because GIF frames overlay with disposal "none".
	frames := make([]image.Image, len(nums))
	for i, num := range nums {
		frames[i] = readImage(t, fmt.Sprint(name, ".", num))
	}
	return frames
}

// readImage reads a Plan 9 image file -- the compressed format devdraw writes
// (image(6); the decoder is libmemdraw/cload.c) -- into an image.RGBA. It
// preserves the image's rectangle, so a frame whose top-left corner is not the
// origin keeps its position (which lets devdraw one day write out just the
// dirtied subimage of each frame). It assumes 8-bit r/g/b/a/k/x channels in any
// order; the channel descriptor in the header gives the byte layout.
func readImage(t *testing.T, file string) *image.RGBA {
	t.Helper()
	data, err := os.ReadFile(file)
	if err != nil {
		t.Fatal(err)
	}
	compressed := bytes.HasPrefix(data, []byte("compressed\n"))
	if compressed {
		data = data[len("compressed\n"):]
	}
	if len(data) < 5*12 {
		t.Fatalf("%s: short image header", file)
	}
	hdr, data := data[:5*12], data[5*12:]
	field := func(i int) int {
		n, err := strconv.Atoi(strings.TrimSpace(string(hdr[i*12 : i*12+12])))
		if err != nil {
			t.Fatalf("%s: bad header field %d: %v", file, i, err)
		}
		return n
	}
	chanStr := strings.TrimSpace(string(hdr[:12]))
	r := image.Rect(field(1), field(2), field(3), field(4))

	// Map each output component to a byte offset within a pixel. The channel
	// descriptor lists channels most-significant first, and pixels are stored
	// little-endian, so the channel at descriptor index i is byte nchan-1-i.
	nchan := len(chanStr) / 2
	const (
		cR = iota
		cG
		cB
		cA
		cK
	)
	off := [5]int{-1, -1, -1, -1, -1}
	for i := range nchan {
		if i*2+1 >= len(chanStr) || chanStr[i*2+1] != '8' {
			t.Fatalf("%s: channel descriptor %q is not all 8-bit", file, chanStr)
		}
		b := nchan - 1 - i
		switch chanStr[i*2] {
		case 'r':
			off[cR] = b
		case 'g':
			off[cG] = b
		case 'b':
			off[cB] = b
		case 'a':
			off[cA] = b
		case 'k':
			off[cK] = b
		case 'x':
			// ignored channel
		default:
			t.Fatalf("%s: unsupported channel descriptor %q", file, chanStr)
		}
	}

	bpl := r.Dx() * nchan
	buf := make([]byte, bpl*r.Dy())
	if compressed {
		for y := r.Min.Y; y < r.Max.Y; {
			if len(data) < 2*12 {
				t.Fatalf("%s: short block header", file)
			}
			maxy, _ := strconv.Atoi(strings.TrimSpace(string(data[:12])))
			nb, _ := strconv.Atoi(strings.TrimSpace(string(data[12:24])))
			data = data[2*12:]
			if maxy <= y || maxy > r.Max.Y || nb <= 0 || nb > len(data) {
				t.Fatalf("%s: bad block header (maxy %d nb %d)", file, maxy, nb)
			}
			if err := cload(buf[(y-r.Min.Y)*bpl:(maxy-r.Min.Y)*bpl], data[:nb]); err != nil {
				t.Fatalf("%s: %v", file, err)
			}
			data, y = data[nb:], maxy
		}
	} else if copy(buf, data) < len(buf) {
		t.Fatalf("%s: short pixel data", file)
	}

	m := image.NewRGBA(r)
	for i := 0; i*nchan < len(buf); i++ {
		p := buf[i*nchan:]
		comp := func(c int) byte {
			if off[c] < 0 {
				return 0
			}
			return p[off[c]]
		}
		o := m.Pix[i*4 : i*4+4 : i*4+4]
		if off[cK] >= 0 { // grey
			o[0], o[1], o[2] = p[off[cK]], p[off[cK]], p[off[cK]]
		} else {
			o[0], o[1], o[2] = comp(cR), comp(cG), comp(cB)
		}
		if off[cA] >= 0 {
			o[3] = p[off[cA]]
		} else {
			o[3] = 0xFF // no alpha channel: opaque
		}
	}
	return m
}

// cload decompresses one block of Plan 9 image data (a copy of cloadmemimage in
// libmemdraw/cload.c) into dst, which it fills exactly. The scheme is LZ77-like
// over a 1024-byte window: a byte >=128 introduces that-many-minus-127 literal
// bytes; otherwise it and the next byte give a back-reference (offset and
// length) into the window of previously emitted bytes.
func cload(dst, data []byte) error {
	const nmem, nmatch = 1024, 3
	var mem [nmem]byte
	memp, u, d := 0, 0, 0
	for d < len(dst) {
		if u >= len(data) {
			return fmt.Errorf("compressed image: data underrun")
		}
		c := int(data[u])
		u++
		if c >= 128 {
			for cnt := c - 128 + 1; cnt != 0; cnt-- {
				if u >= len(data) || d >= len(dst) {
					return fmt.Errorf("compressed image: bad literal run")
				}
				dst[d] = data[u]
				mem[memp] = data[u]
				u, d, memp = u+1, d+1, memp+1
				if memp == nmem {
					memp = 0
				}
			}
			continue
		}
		if u >= len(data) {
			return fmt.Errorf("compressed image: data underrun")
		}
		offs := int(data[u]) + ((c & 3) << 8) + 1
		u++
		omemp := memp - offs
		if omemp < 0 {
			omemp += nmem
		}
		for cnt := (c >> 2) + nmatch; cnt != 0; cnt-- {
			if d >= len(dst) {
				return fmt.Errorf("compressed image: back-reference overrun")
			}
			dst[d] = mem[omemp]
			mem[memp] = mem[omemp]
			d, memp, omemp = d+1, memp+1, omemp+1
			if omemp == nmem {
				omemp = 0
			}
			if memp == nmem {
				memp = 0
			}
		}
	}
	return nil
}

// writePNG writes m to file as a PNG, for inspecting a differing frame.
func writePNG(t *testing.T, file string, m image.Image) {
	t.Helper()
	var buf bytes.Buffer
	if err := png.Encode(&buf, m); err != nil {
		t.Errorf("encoding %s: %v", file, err)
		return
	}
	if err := os.WriteFile(file, buf.Bytes(), 0666); err != nil {
		t.Errorf("writing %s: %v", file, err)
	}
}


// Check checks the frames loaded by [Frames] against expected frames stored in
// the golden file, as a single animated GIF. The frames are the per-operation
// diffs (see Frames); each is compared and stored at its own rectangle, so no
// compositing is needed on either side. If update is true, it writes the GIF
// instead of comparing against it.
func Check(t *testing.T, frames []image.Image, golden string, update bool) {
	t.Helper()
	if update {
		if err := os.MkdirAll(filepath.Dir(golden), 0777); err != nil {
			t.Error(err)
			return
		}
		if err := os.WriteFile(golden, encodeGIF(t, frames), 0666); err != nil {
			t.Error(err)
			return
		}
		return
	}
	data, err := os.ReadFile(golden)
	if err != nil {
		t.Fatalf("reading golden %s (regenerate with -update): %v", golden, err)
	}
	want := decodeGIF(t, data)
	for i, frame := range frames {
		if i >= len(want) {
			t.Errorf("extra frames: have %d, want %d", len(frames), len(want))
			return
		}
		if diff := imageDiff(frame, want[i]); diff.Dx() > 0 {
			t.Errorf("frame %d differs from %v in bounding box %v", i, golden, diff)
			have := filepath.Join(filepath.Dir(golden), "have."+filepath.Base(golden))
			if err := os.WriteFile(have, encodeGIF(t, frames), 0666); err != nil {
				t.Fatal(err)
			}
			t.Errorf("new frames in %v", have)
			// Also dump the single differing frame -- the one we have and the one
			// we wanted -- as PNGs, so the actual pixel difference can be seen
			// directly instead of having to scrub the whole animation.
			base := golden[:len(golden)-len(".gif")]
			writePNG(t, fmt.Sprintf("%s.frame%d.have.png", base, i), frame)
			writePNG(t, fmt.Sprintf("%s.frame%d.want.png", base, i), want[i])
			t.Errorf("wrote differing frame %d to %s.frame%d.{have,want}.png", i, base, i)
			return
		}
	}
	if len(want) > len(frames) {
		t.Errorf("missing frames: have %d, want %d", len(frames), len(want))
	}
}

// imageDiff reports the bounding box of differing pixels in the images.
// If the images are identical, it returns a 0x0 bounding box.
func imageDiff(p, q image.Image) image.Rectangle {
	const inf = 1_000_000_000
	xmin, xmax, ymin, ymax := inf, -inf, inf, -inf
	pb := p.Bounds()
	qb := q.Bounds()
	for x := min(pb.Min.X, qb.Min.X); x < max(pb.Max.X, qb.Max.X); x++ {
		for y := min(pb.Min.Y, qb.Min.Y); y < max(pb.Max.Y, qb.Max.Y); y++ {
			if image.Pt(x, y).In(pb) && image.Pt(x, y).In(qb) && rgbaOf(p.At(x, y)) == rgbaOf(q.At(x, y)) {
				continue
			}
			xmin, xmax = min(xmin, x), max(xmax, x)
			ymin, ymax = min(ymin, y), max(ymax, y)
		}
	}
	if xmin > xmax {
		return image.Rect(0, 0, 0, 0)
	}
	return image.Rect(xmin, ymin, xmax, ymax)
}

// decodeGIF decodes an animated GIF and composites its frames back into full images.
func decodeGIF(t *testing.T, data []byte) []image.Image {
	t.Helper()
	g, err := gif.DecodeAll(bytes.NewReader(data))
	if err != nil {
		t.Fatalf("decodeGIF: %v", err)
	}
	// Return each frame as it was stored -- the diff at its rectangle -- without
	// compositing. Check compares the diffs directly (see Frames and encodeGIF);
	// the GIF still composites visually when viewed because the frames overlay
	// with disposal "none".
	frames := make([]image.Image, len(g.Image))
	for i, gframe := range g.Image {
		frames[i] = gframe // *image.Paletted, bounds = the diff's rectangle
	}
	return frames
}

// encodeGIF encodes the diff frames as an animated GIF, one GIF frame per diff,
// stored at its rectangle exactly as given. No transparency is computed: a diff
// is already the change from the previous frame, and GIF's "none" disposal makes
// the frames overlay into the full window when viewed. It fails if the frames
// use more than 256 colors.
func encodeGIF(t *testing.T, frames []image.Image) []byte {
	t.Helper()

	if len(frames) == 0 {
		t.Fatalf("encodeGIF: no frames")
	}

	// Build the palette from every color across the diffs, and the logical
	// screen from the union of their rectangles (the largest the window was).
	colors := map[rgba]bool{}
	var canvas image.Rectangle
	for _, m := range frames {
		canvas = canvas.Union(m.Bounds())
		for x, y := range points(m.Bounds()) {
			colors[rgbaOf(m.At(x, y))] = true
		}
	}
	if len(colors) > 256 {
		t.Fatalf("encodeGIF: too many colors: %v", len(colors))
	}
	// The logical screen is described by canvas.Max alone (GIF has no negative
	// origin), which is only correct if the diffs reach the top-left corner. The
	// test screen always starts at (0,0), so the first full-window draw pins the
	// union there; a non-zero Min would mean we never drew the corner -- a bug.
	if canvas.Min != (image.Point{}) {
		t.Fatalf("encodeGIF: canvas does not start at (0,0): %v", canvas)
	}
	pal := make(color.Palette, 0, len(colors))
	for _, c := range slices.Sorted(maps.Keys(colors)) {
		pal = append(pal, c)
	}
	index := make(map[rgba]int)
	for i, c := range pal {
		index[c.(rgba)] = i
	}

	g := &gif.GIF{Config: image.Config{ColorModel: pal, Width: canvas.Max.X, Height: canvas.Max.Y}}
	for _, frame := range frames {
		p := image.NewPaletted(frame.Bounds(), pal)
		for x, y := range points(frame.Bounds()) {
			p.SetColorIndex(x, y, uint8(index[rgbaOf(frame.At(x, y))]))
		}
		g.Image = append(g.Image, p)
		g.Delay = append(g.Delay, 100) // 1s/frame when viewed
		g.Disposal = append(g.Disposal, gif.DisposalNone)
	}
	var buf bytes.Buffer
	if err := gif.EncodeAll(&buf, g); err != nil {
		t.Fatalf("encodeGIF: %v", err)
	}
	return buf.Bytes()
}

// points returns an iterator over points in r, in row-major order.
func points(r image.Rectangle) iter.Seq2[int, int] {
	return func(yield func(x, y int) bool) {
		for y := r.Min.Y; y <= r.Max.Y; y++ {
			for x := r.Min.X; x <= r.Max.X; x++ {
				if !yield(x, y) {
					return
				}
			}
		}
	}
}

// An rgba implements color.Color as a 64-bit RGBA value (16 bits each).
type rgba uint64

// rgbaOf returns c as an rgba.
func rgbaOf(c color.Color) rgba {
	r, g, b, a := c.RGBA()
	return rgba(r)<<48 | rgba(g)<<32 | rgba(b)<<16 | rgba(a)
}

func (c rgba) RGBA() (r, g, b, a uint32) {
	return uint32((c >> 48) & 0xFFFF),
		uint32((c >> 32) & 0xFFFF),
		uint32((c >> 16) & 0xFFFF),
		uint32(c & 0xFFFF)
}
