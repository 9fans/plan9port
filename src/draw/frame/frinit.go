package frame

import (
	"9fans.net/go/draw"
)

// Init prepares the frame f so characters drawn in it
// will appear in the font ft.
// It then calls SetRects and InitTick to initialize the geometry for the frame.
// The image b is where the frame is to be drawn;
// the rectangle r defines the limit of the portion of the
// image the text will occupy.
// The image pointer may be null,
// allowing the other routines to be called to maintain the
// associated data structure in, for example, an obscured window.
// The array of images cols sets the colors in which text and
// borders will be drawn.  The background of the frame will be
// drawn in cols[BACK]; the background of highlighted text in
// cols[HIGH]; borders and scroll bar in cols[BORD]; regular
// text in cols[TEXT]; and highlighted text in cols[HTEXT].
func (f *Frame) Init(r draw.Rectangle, ft *draw.Font, b *draw.Image, cols []*draw.Image) {
	f.Font = ft
	f.Display = b.Display
	f.MaxTab = 8 * ft.StringWidth("0")
	f.box = nil
	f.NumChars = 0
	f.NumLines = 0
	f.P0 = 0
	f.P1 = 0
	f.LastLineFull = false
	copy(f.Cols[:], cols)
	f.SetRects(r, b)
	if f.tick == nil && f.Cols[BACK] != nil {
		f.InitTick()
	}
}

// InitTick initializes the frame's tick images.
// It is called during the Init method and must be called again
// each time the frame's font changes.
func (f *Frame) InitTick() {
	if f.Cols[BACK] == nil || f.Display == nil {
		return
	}
	f.tickscale = f.Display.ScaleSize(1)
	b := f.Display.ScreenImage
	if b == nil {
		drawerror(f.Display, "missing screenimage")
	}
	ft := f.Font
	if f.tick != nil {
		f.tick.Free()
	}
	f.tick, _ = f.Display.AllocImage(draw.Rect(0, 0, f.tickscale*_FRTICKW, ft.Height), b.Pix, false, draw.White)
	if f.tick == nil {
		return
	}
	if f.tickback != nil {
		f.tickback.Free()
	}
	f.tickback, _ = f.Display.AllocImage(f.tick.R, b.Pix, false, draw.White)
	if f.tickback == nil {
		f.tick.Free()
		f.tick = nil
		return
	}

	// background color
	f.tick.Draw(f.tick.R, f.Cols[BACK], nil, draw.ZP)
	// vertical line
	f.tick.Draw(draw.Rect(f.tickscale*(_FRTICKW/2), 0, f.tickscale*(_FRTICKW/2+1), ft.Height), f.Display.Black, nil, draw.ZP)
	// box on each end
	f.tick.Draw(draw.Rect(0, 0, f.tickscale*_FRTICKW, f.tickscale*_FRTICKW), f.Cols[TEXT], nil, draw.ZP)
	f.tick.Draw(draw.Rect(0, ft.Height-f.tickscale*_FRTICKW, f.tickscale*_FRTICKW, ft.Height), f.Cols[TEXT], nil, draw.ZP)
}

func (f *Frame) SetRects(r draw.Rectangle, b *draw.Image) {
	f.B = b
	f.Entire = r
	f.R = r
	f.R.Max.Y -= (r.Max.Y - r.Min.Y) % f.Font.Height
	f.MaxLines = (r.Max.Y - r.Min.Y) / f.Font.Height
}

// Clear frees the internal structures associated with f,
// permitting another Init or SetRects on the Frame.
// It does not clear the associated display.
// If f is to be deallocated, the associated Font and Image (as passed to Init)
// must be freed separately.
// The newFont argument should be true if the frame is to be
// redrawn with a different font; otherwise the frame will
// maintain some data structures associated with the font.
//
// To resize a frame, use Clear and Init and then Insert
// to recreate the display. If a frame is being moved
// but not resized, that is, if the shape of its containing
// rectangle is unchanged, it is sufficient to use Draw on the
// underlying image to copy the containing rectangle
// from the old to the new location and then call SetRects
// to establish the new geometry.
// (It is unnecessary to call frinittick unless the font size
// has changed.)  No redrawing is necessary.
func (f *Frame) Clear(newFont bool) {
	if len(f.box) > 0 {
		f.delbox(0, len(f.box)-1)
	}
	f.box = nil
	f.Ticked = false
	if newFont {
		f.tick.Free()
		f.tickback.Free()
		f.tick = nil
		f.tickback = nil
	}
}
