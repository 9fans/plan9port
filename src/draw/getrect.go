package draw

import (
	"log"
)

var (
	grTmp [4]*Image
	grRed *Image
)

var sweep = Cursor{
	Point{-7, -7},
	[2 * 16]uint8{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE0, 0x07,
		0xE0, 0x07, 0xE0, 0x07, 0xE3, 0xF7, 0xE3, 0xF7,
		0xE3, 0xE7, 0xE3, 0xF7, 0xE3, 0xFF, 0xE3, 0x7F,
		0xE0, 0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	[2 * 16]uint8{0x00, 0x00, 0x7F, 0xFE, 0x40, 0x02, 0x40, 0x02,
		0x40, 0x02, 0x40, 0x02, 0x40, 0x02, 0x41, 0xE2,
		0x41, 0xC2, 0x41, 0xE2, 0x41, 0x72, 0x40, 0x38,
		0x40, 0x1C, 0x40, 0x0E, 0x7F, 0xE6, 0x00, 0x00},
}

const BorderWidth = 4

func brects(r Rectangle) [4]Rectangle {
	const W = BorderWidth
	if r.Dx() < 2*W {
		r.Max.X = r.Min.X + 2*W
	}
	if r.Dy() < 2*W {
		r.Max.Y = r.Min.Y + 2*W
	}
	var rp [4]Rectangle
	rp[0] = Rect(r.Min.X, r.Min.Y, r.Max.X, r.Min.Y+W)
	rp[1] = Rect(r.Min.X, r.Max.Y-W, r.Max.X, r.Max.Y)
	rp[2] = Rect(r.Min.X, r.Min.Y+W, r.Min.X+W, r.Max.Y-W)
	rp[3] = Rect(r.Max.X-W, r.Min.Y+W, r.Max.X, r.Max.Y-W)
	return rp
}

func SweepRect(but int, mc *Mousectl) Rectangle {
	but = 1 << (but - 1)
	mc.Display.SwitchCursor(&sweep)
	for mc.Buttons != 0 {
		mc.Read()
	}
	for mc.Buttons&but == 0 {
		mc.Read()
		if mc.Buttons&(7^but) != 0 {
			for mc.Buttons != 0 {
				mc.Read()
			}
			return ZR
		}
	}
	r := Rectangle{Min: mc.Point, Max: mc.Point}
	var rc Rectangle
	for {
		rc = r.Canon()
		drawgetrect(mc.Display, rc, true)
		mc.Read()
		drawgetrect(mc.Display, rc, false)
		r.Max = mc.Point
		if mc.Buttons != but {
			break
		}
	}

	mc.Display.SwitchCursor(nil)
	if mc.Buttons&(7^but) != 0 {
		rc = ZR
		for mc.Buttons != 0 {
			mc.Read()
		}
	}
	return rc
}

func freegrtmp() {
	grTmp[0].Free()
	grTmp[1].Free()
	grTmp[2].Free()
	grTmp[3].Free()
	grRed.Free()
	grTmp = [4]*Image{}
	grRed = nil
}

func max(a, b int) int {
	if a > b {
		return a
	}
	return b
}

func drawgetrect(display *Display, rc Rectangle, up bool) {
	screen := display.ScreenImage
	/*
	 * BUG: if for some reason we have two of these going on at once
	 * when we must grow the tmp buffers, we lose data.  Also if tmp
	 * is unallocated and we ask to restore the screen, it would be nice
	 * to complain, but we silently make a mess.
	 */
	if up && grTmp[0] != nil {
		if grTmp[0].R.Dx() < rc.Dx() || grTmp[2].R.Dy() < rc.Dy() {
			freegrtmp()
		}
	}
	if grTmp[0] == nil {
		const W = BorderWidth
		r := Rect(0, 0, max(screen.R.Dx(), rc.Dx()), W)
		grTmp[0], _ = display.AllocImage(r, screen.Pix, false, White)
		grTmp[1], _ = display.AllocImage(r, screen.Pix, false, White)
		r = Rect(0, 0, W, max(screen.R.Dy(), rc.Dy()))
		grTmp[2], _ = display.AllocImage(r, screen.Pix, false, White)
		grTmp[3], _ = display.AllocImage(r, screen.Pix, false, White)
		grRed, _ = display.AllocImage(Rect(0, 0, 1, 1), screen.Pix, true, Red)
		if grTmp[0] == nil || grTmp[1] == nil || grTmp[2] == nil || grTmp[3] == nil || grRed == nil {
			freegrtmp()
			log.Fatalf("getrect: allocimage failed")
		}
	}
	rects := brects(rc)
	if !up {
		for i := 0; i < 4; i++ {
			screen.Draw(rects[i], grTmp[i], nil, ZP)
		}
		return
	}
	for i := 0; i < 4; i++ {
		grTmp[i].Draw(Rect(0, 0, rects[i].Dx(), rects[i].Dy()), screen, nil, rects[i].Min)
		screen.Draw(rects[i], grRed, nil, ZP)
	}
}
