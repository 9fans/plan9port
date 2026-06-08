package draw // import "9fans.net/go/draw"

// An Op represents a Porter-Duff compositing operator.
//
// See T. Porter, T. Duff. “Compositing Digital Images”,
// Computer Graphics (Proc. SIGGRAPH), 18:3, pp. 253-259, 1984.
type Op int

const (
	Clear Op = 0

	SinD  Op = 8
	DinS  Op = 4
	SoutD Op = 2
	DoutS Op = 1

	S      = SinD | SoutD
	SoverD = SinD | SoutD | DoutS
	SatopD = SinD | DoutS
	SxorD  = SoutD | DoutS

	D      = DinS | DoutS
	DoverS = DinS | DoutS | SoutD
	DatopS = DinS | SoutD
	DxorS  = DoutS | SoutD // == SxorD

	Ncomp = 12
)

func setdrawop(d *Display, op Op) {
	if op != SoverD {
		a := d.bufimage(2)
		a[0] = 'O'
		a[1] = byte(op)
	}
}

func draw(dst *Image, r Rectangle, src *Image, p0 Point, mask *Image, p1 Point, op Op) {
	setdrawop(dst.Display, op)

	a := dst.Display.bufimage(1 + 4 + 4 + 4 + 4*4 + 2*4 + 2*4)
	if src == nil {
		src = dst.Display.Black
	}
	if mask == nil {
		mask = dst.Display.Opaque
	}
	a[0] = 'd'
	bplong(a[1:], dst.id)
	bplong(a[5:], src.id)
	bplong(a[9:], mask.id)
	bplong(a[13:], uint32(r.Min.X))
	bplong(a[17:], uint32(r.Min.Y))
	bplong(a[21:], uint32(r.Max.X))
	bplong(a[25:], uint32(r.Max.Y))
	bplong(a[29:], uint32(p0.X))
	bplong(a[33:], uint32(p0.Y))
	bplong(a[37:], uint32(p1.X))
	bplong(a[41:], uint32(p1.Y))
}

func (dst *Image) draw(r Rectangle, src, mask *Image, p1 Point) {
	draw(dst, r, src, p1, mask, p1, SoverD)
}

// Draw is the standard drawing function. Only those pixels within the
// intersection of dst.R and dst.Clipr will be affected; draw ignores
// dst.Repl. The operation proceeds as follows (this is a description of
// the behavior, not the implementation):
//
// 1. If Repl is set in src or mask, replicate their contents to fill
// their clip rectangles.
//
// 2. Translate src and mask so p is aligned with r.Min.
//
// 3. Set r to the intersection of r and dst.R.
//
// 4. Intersect r with src.Clipr. If src.Repl is false, also intersect r
// with src.R.
//
// 5. Intersect r with mask.Clipr. If mask.Repl is false, also intersect
// r with mask.R
//
// 6. For each location in r, combine the dst pixel with the src pixel
// using the alpha value corresponding to the mask pixel. If the mask has
// an explicit alpha channel, the alpha value corresponding to the mask
// pixel is simply that pixel's alpha chan- nel. Otherwise, the alpha
// value is the NTSC greyscale equivalent of the color value, with white
// meaning opaque and black transparent. In terms of the Porter-Duff
// compositing algebra, draw replaces the dst pixels with (src in mask)
// over dst. (In the extended form provided by DrawOp, “over” is replaced
// by op).
//
// The various pixel channel formats involved need not be identical.
// If the channels involved are smaller than 8-bits, they will be
// promoted before the calculation by replicating the extant bits; after
// the calculation, they will be truncated to their proper sizes.
//
// Mask may be nil, in which case a fully opaque mask is assumed.
func (dst *Image) Draw(r Rectangle, src, mask *Image, p Point) {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	draw(dst, r, src, p, mask, p, SoverD)
}

// DrawOp is like Draw but specifies a Porter-Duff operator op to use in place of “S over D”.
// That is, dst.Draw(r, src, mask, p) is the same as dst.DrawOp(r, src, mask, p, SoverD).
func (dst *Image) DrawOp(r Rectangle, src, mask *Image, p Point, op Op) {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	draw(dst, r, src, p, mask, p, op)
}

// GenDraw is like Draw except that it aligns the source and mask differently:
// src is aligned so p0 corresponds to r.Min, while mask is aligned so p1 corresponds to r.Min.
// GenDraw differs from Draw only when both of src or mask are non-trivial.
// For most purposes, Draw is sufficient.
func (dst *Image) GenDraw(r Rectangle, src *Image, p0 Point, mask *Image, p1 Point) {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	draw(dst, r, src, p0, mask, p1, SoverD)
}

// GenDrawOp is like GenDraw but specifies a Porter-Duff operator op to use in place of “S over D”.
func GenDrawOp(dst *Image, r Rectangle, src *Image, p0 Point, mask *Image, p1 Point, op Op) {
	dst.Display.mu.Lock()
	defer dst.Display.mu.Unlock()
	draw(dst, r, src, p0, mask, p1, op)
}
