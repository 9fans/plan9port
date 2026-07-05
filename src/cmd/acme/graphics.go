package main

import (
	"os"

	"9fans.net/plan9port/draw"
	"9fans.net/plan9port/draw/frame"
)

// boxcursor is the box-shaped cursor shown while dragging a window or column
// (acme.c). boxcursor2 is the high-DPI variant; the test runs at 1x, where only
// boxcursor is used, so the 2x form is derived by scaling rather than
// transcribed (it is identical to C's at the resolutions the port renders).
var boxcursor = draw.Cursor{
	Point: draw.Point{X: -7, Y: -7},
	White: [32]uint8{
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F,
		0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	Black: [32]uint8{
		0x00, 0x00, 0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE,
		0x70, 0x0E, 0x70, 0x0E, 0x70, 0x0E, 0x70, 0x0E,
		0x70, 0x0E, 0x70, 0x0E, 0x70, 0x0E, 0x70, 0x0E,
		0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE, 0x00, 0x00},
}

var boxcursor2 = draw.ScaleCursor(boxcursor)

// Scaled UI dimensions (dat.h scalesize(display,N)).
func (a *Acme) scrollwid() int { return a.disp.ScaleSize(12) }
func (a *Acme) scrollgap() int { return a.disp.ScaleSize(4) }
func (a *Acme) border() int    { return a.disp.ScaleSize(2) }

// dat.h also defines Margin = scalesize(display,4), but it is unused in the
// C source, so it is not ported here.

// initdraw opens the display, font, and colors (acme.c threadmain + iconinit).
func (a *Acme) initdraw(label, winsize string) error {
	d, err := draw.Init(nil, "", label, winsize, os.Environ())
	if err != nil {
		// untested: draw.Init failure needs an unopenable display
		return err
	}
	a.disp = d
	a.screen = d.ScreenImage
	a.font = d.Font
	a.reffont.f = a.font
	a.reffonts[0] = &a.reffont
	a.reffonts[1] = &a.reffont
	a.fontnames[0] = a.font.Name
	a.fontnames[1] = a.font.Name
	a.fontcache = []*Reffont{&a.reffont}
	if a.maxtab == 0 {
		a.maxtab = 4
	}
	a.iconinit()
	return nil
}

func (a *Acme) allocColor(c draw.Color) *draw.Image {
	im, _ := a.disp.AllocImage(draw.Rect(0, 0, 1, 1), a.screen.Pix, true, c)
	return im
}

// iconinit sets up the tag/body colors and the button images (acme.c iconinit).
func (a *Acme) iconinit() {
	d := a.disp
	if a.tagcols[frame.BACK] == nil {
		// Blue
		a.tagcols[frame.BACK] = d.AllocImageMix(draw.PaleBlueGreen, draw.White)
		a.tagcols[frame.HIGH] = a.allocColor(draw.PaleGreyGreen)
		a.tagcols[frame.BORD] = a.allocColor(draw.PurpleBlue)
		a.tagcols[frame.TEXT] = d.Black
		a.tagcols[frame.HTEXT] = d.Black
		// Yellow
		a.textcols[frame.BACK] = d.AllocImageMix(draw.PaleYellow, draw.White)
		a.textcols[frame.HIGH] = a.allocColor(draw.DarkYellow)
		a.textcols[frame.BORD] = a.allocColor(draw.YellowGreen)
		a.textcols[frame.TEXT] = d.Black
		a.textcols[frame.HTEXT] = d.Black
	}
	r := draw.Rect(0, 0, a.scrollwid(), a.font.Height+1)
	if a.button != nil && eqrect(r, a.button.R) {
		// Same geometry: nothing to reallocate. Reached when the `resize` verb
		// re-runs iconinit but -D keeps the font, so scrollwid/height are
		// unchanged (acme.c iconinit).
		return
	}
	a.button = a.allocImageRect(r, draw.NoFill)
	a.button.Draw(r, a.tagcols[frame.BACK], nil, r.Min)
	a.button.Border(r, a.disp.ScaleSize(2), a.tagcols[frame.BORD], draw.ZP)
	a.modbutton = a.allocImageRect(r, draw.NoFill)
	a.modbutton.Draw(r, a.tagcols[frame.BACK], nil, r.Min)
	bb := a.disp.ScaleSize(2)
	a.modbutton.Border(r, bb, a.tagcols[frame.BORD], draw.ZP)
	ir := r
	ir.Min.X += bb
	ir.Min.Y += bb
	ir.Max.X -= bb
	ir.Max.Y -= bb
	a.modbutton.Draw(ir, a.allocColor(draw.MedBlue), nil, draw.ZP)
	a.colbutton = a.allocImageRect(r, draw.PurpleBlue)
	a.but2col = a.allocColorRepl(0xAA0000FF)
	a.but3col = a.allocColorRepl(0x006600FF)
}

func (a *Acme) allocImageRect(r draw.Rectangle, c draw.Color) *draw.Image {
	im, _ := a.disp.AllocImage(r, a.screen.Pix, false, c)
	return im
}

func (a *Acme) allocColorRepl(c draw.Color) *draw.Image {
	im, _ := a.disp.AllocImage(draw.Rect(0, 0, 1, 1), a.screen.Pix, true, c)
	return im
}

func eqrect(r, s draw.Rectangle) bool {
	return r.Min.X == s.Min.X && r.Min.Y == s.Min.Y && r.Max.X == s.Max.X && r.Max.Y == s.Max.Y
}
