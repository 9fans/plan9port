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
	"image/draw"
	"image/gif"
	"image/png"
	"iter"
	"maps"
	"os"
	"os/exec"
	"path/filepath"
	"slices"
	"sort"
	"strconv"
	"sync"
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
	var frames []image.Image
	for _, num := range nums {
		x := topng(t, fmt.Sprint(name, ".", num)) // DEBUG
		os.WriteFile(fmt.Sprintf("/tmp/debug.%06d.png", num), x, 0666)
		frames = append(frames, decodePNG(t, x))
	}
	return frames
}

var lookTopng = sync.OnceValues(func() (string, error) {
	return exec.LookPath("topng")
})

// topng converts one Plan 9 image file to PNG bytes.
func topng(t *testing.T, file string) []byte {
	t.Helper()
	exe, err := lookTopng()
	if err != nil {
		t.Fatal(err)
	}
	out, err := exec.Command(exe, file).Output()
	if err != nil {
		t.Fatalf("topng %s: %v", file, err)
	}
	return out
}

// decodePNG decodes PNG bytes to an image.Image.
func decodePNG(t *testing.T, data []byte) image.Image {
	t.Helper()
	m, err := png.Decode(bytes.NewReader(data))
	if err != nil {
		t.Fatal(err)
	}
	return m
}

// Check checks the frames loaded by [Frames] against
// expected frames stored in the golden file, as a single animated GIF.
// If update is true, it writes the GIF instead of comparing against the GIF.
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
		t.Fatalf("reading golden (regenerate with -update): %v", golden, err)
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
	b := g.Image[0].Bounds()
	canvas := image.NewRGBA(b)
	var frames []image.Image
	for _, gframe := range g.Image {
		draw.Draw(canvas, gframe.Bounds(), gframe, gframe.Bounds().Min, draw.Over)
		frame := image.NewRGBA(b)
		draw.Draw(frame, b, canvas, b.Min, draw.Src)
		frames = append(frames, frame)
	}
	return frames
}

// encodeGIF encodes the frames as an animated GIF.
// It fails if the frames use more than 255 colors.
// Each frame is stored with transparent pixels
// where it matches the previous frame, to reduce storage space.
func encodeGIF(t *testing.T, frames []image.Image) []byte {
	t.Helper()

	if len(frames) == 0 {
		t.Fatalf("encodeGIF: no frames")
	}

	// Build GIF palette and index.
	// Transparent rgba(0) will end up at palette slot 0.
	colors := map[rgba]bool{0: true}
	for _, m := range frames {
		for x, y := range points(m.Bounds()) {
			colors[rgbaOf(m.At(x, y))] = true
		}
	}
	if len(colors) > 256 {
		t.Fatalf("encodeGIF: too many colors: %v", len(colors))
	}
	pal := make(color.Palette, len(colors))
	for i, c := range slices.Sorted(maps.Keys(colors)) {
		pal[i] = c
	}
	index := make(map[rgba]int)
	for i, c := range pal {
		index[c.(rgba)] = i
	}

	// Convert frames to paletted.
	g := new(gif.GIF)
	var prev image.Image
	for _, frame := range frames {
		p := image.NewPaletted(frame.Bounds(), pal) // zero-filled => all transparent
		for x, y := range points(frame.Bounds()) {
			c := rgbaOf(frame.At(x, y))
			if prev != nil && image.Pt(x, y).In(prev.Bounds()) && rgbaOf(prev.At(x, y)) == c {
				c = 0 // transparent
			}
			p.SetColorIndex(x, y, uint8(index[c]))
		}
		g.Image = append(g.Image, p)
		g.Delay = append(g.Delay, 100) // 1s/frame when viewed
		g.Disposal = append(g.Disposal, gif.DisposalNone)
		prev = p
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
