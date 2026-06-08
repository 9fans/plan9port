package draw

const (
	menuMargin      = 4  /* outside to text */
	menuBorder      = 2  /* outside to selection boxes */
	menuBlackborder = 2  /* width of outlining border */
	menuVspacing    = 2  /* extra spacing between lines of text */
	menuMaxunscroll = 25 /* maximum #entries before scrolling turns on */
	menuNscroll     = 20 /* number entries in scrolling part */
	menuScrollwid   = 14 /* width of scroll bar */
	menuGap         = 4  /* between text and scroll bar */
)

// A Menu describes a menu of items.
//
// The items are specified either in the static slice Item
// or by a function Gen that can be called to generate the k'th item
// (starting with k = 0). Gen should return text, true on success
// and "", false when k is beyond the end of the menu.
// LastHit records the previously selected menu item.
type Menu struct {
	Item    []string
	Gen     func(k int, buf []byte) (text []byte, ok bool)
	LastHit int

	cache []byte
}

func (me *Menu) gen(k int) ([]byte, bool) {
	buf, ok := me.Gen(k, me.cache[:0])
	if buf != nil {
		me.cache = buf
	}
	return buf, ok
}

// TODO hide in display
var menu struct {
	txt   *Image
	back  *Image
	high  *Image
	bord  *Image
	text  *Image
	htext *Image
}

func menucolors(display *Display) {
	/* Main tone is greenish, with negative selection */
	menu.back = display.AllocImageMix(PaleGreen, White)
	menu.high, _ = display.AllocImage(Rect(0, 0, 1, 1), display.ScreenImage.Pix, true, DarkGreen) /* dark green */
	menu.bord, _ = display.AllocImage(Rect(0, 0, 1, 1), display.ScreenImage.Pix, true, MedGreen)  /* not as dark green */
	if menu.back == nil || menu.high == nil || menu.bord == nil {
		goto Error
	}
	menu.text = display.Black
	menu.htext = menu.back
	return

Error:
	menu.back.Free()
	menu.high.Free()
	menu.bord.Free()
	menu.back = display.White
	menu.high = display.Black
	menu.bord = display.Black
	menu.text = display.Black
	menu.htext = display.White
}

/*
 * r is a rectangle holding the text elements.
 * return the rectangle, including its black edge, holding element i.
 */
func menurect(display *Display, r Rectangle, i int) Rectangle {
	if i < 0 {
		return Rect(0, 0, 0, 0)
	}
	r.Min.Y += (display.Font.Height + menuVspacing) * i
	r.Max.Y = r.Min.Y + display.Font.Height + menuVspacing
	return r.Inset(menuBorder - menuMargin)
}

/*
 * r is a rectangle holding the text elements.
 * return the element number containing p.
 */
func menusel(display *Display, r Rectangle, p Point) int {
	r = r.Inset(menuMargin)
	if !p.In(r) {
		return -1
	}
	return (p.Y - r.Min.Y) / (display.Font.Height + menuVspacing)
}

func paintitem(m *Image, me *Menu, textr Rectangle, off, i int, highlight bool, save, restore *Image) {
	if i < 0 {
		return
	}
	display := m.Display
	font := display.Font
	r := menurect(display, textr, i)
	if restore != nil {
		m.Draw(r, restore, nil, restore.R.Min)
		return
	}
	if save != nil {
		save.Draw(save.R, m, nil, r.Min)
	}
	var item string
	var itemBytes []byte
	var width int
	if me.Item != nil {
		item = me.Item[i+off]
		width = font.StringWidth(item)
	} else {
		itemBytes, _ = me.gen(i + off)
		width = font.BytesWidth(itemBytes)
	}
	var pt Point
	pt.X = (textr.Min.X + textr.Max.X - width) / 2
	pt.Y = textr.Min.Y + i*(font.Height+menuVspacing)
	back, text := menu.back, menu.text
	if highlight {
		back, text = menu.high, menu.htext
	}
	m.Draw(r, back, nil, pt)
	if item != "" {
		m.String(pt, text, pt, font, item)
	} else {
		m.Bytes(pt, text, pt, font, itemBytes)
	}
}

/*
 * menur is a rectangle holding all the highlightable text elements.
 * track mouse while inside the box, return what's selected when button
 * is raised, -1 as soon as it leaves box.
 * invariant: nothing is highlighted on entry or exit.
 */
func menuscan(m *Image, me *Menu, but int, mc *Mousectl, textr Rectangle, off, lasti int, save *Image) int {
	paintitem(m, me, textr, off, lasti, true, save, nil)
	for mc.Read(); mc.Buttons&(1<<(but-1)) != 0; mc.Read() {
		i := menusel(m.Display, textr, mc.Point)
		if i != -1 && i == lasti {
			continue
		}
		paintitem(m, me, textr, off, lasti, false, nil, save)
		if i == -1 {
			return i
		}
		lasti = i
		paintitem(m, me, textr, off, lasti, true, save, nil)
	}
	return lasti
}

func menupaint(m *Image, me *Menu, textr Rectangle, off, nitemdrawn int) {
	m.Draw(textr.Inset(menuBorder-menuMargin), menu.back, nil, ZP)
	for i := 0; i < nitemdrawn; i++ {
		paintitem(m, me, textr, off, i, false, nil, nil)
	}
}

func menuscrollpaint(m *Image, scrollr Rectangle, off, nitem, nitemdrawn int) {
	m.Draw(scrollr, menu.back, nil, ZP)
	r := scrollr
	r.Min.Y = scrollr.Min.Y + (scrollr.Dy()*off)/nitem
	r.Max.Y = scrollr.Min.Y + (scrollr.Dy()*(off+nitemdrawn))/nitem
	if r.Max.Y < r.Min.Y+2 {
		r.Max.Y = r.Min.Y + 2
	}
	m.Border(r, 1, menu.bord, ZP)
	if menu.txt == nil {
		display := m.Display
		menu.txt, _ = display.AllocImage(Rect(0, 0, 1, 1), display.ScreenImage.Pix, true, DarkGreen) /* border color; BUG? */
	}
	if menu.txt != nil {
		m.Draw(r.Inset(1), menu.txt, nil, ZP)
	}
}

func MenuHit(but int, mc *Mousectl, me *Menu, scr *Screen) int {
	/*
		int nitemdrawn,  lasti, off, noff, wid;
		int scrolling;
		Rectangle r, menur,  textr, scrollr;
		Image *b, *save, *backup;
		Point pt;
		char *item;
	*/

	display := mc.Display
	screen := display.ScreenImage
	font := display.Font

	if menu.back == nil {
		menucolors(display)
	}
	sc := screen.Clipr
	screen.ReplClipr(false, screen.R)
	maxwid := 0
	nitem := 0
	for ; ; nitem++ {
		var w int
		if me.Item != nil {
			if nitem >= len(me.Item) {
				break
			}
			w = font.StringWidth(me.Item[nitem])
		} else {
			buf, ok := me.gen(nitem)
			if !ok {
				break
			}
			w = font.BytesWidth(buf)
		}
		if w > maxwid {
			maxwid = w
		}
	}

	if me.LastHit < 0 || me.LastHit >= nitem {
		me.LastHit = 0
	}

	screenitem := (screen.R.Dy() - 10) / (font.Height + menuVspacing)
	scrolling := false
	nitemdrawn := nitem
	wid := maxwid
	off := 0
	lasti := me.LastHit
	if nitem > menuMaxunscroll || nitem > screenitem {
		scrolling = true
		nitemdrawn = menuNscroll
		if nitemdrawn > screenitem {
			nitemdrawn = screenitem
		}
		wid = maxwid + menuGap + menuScrollwid
		off = me.LastHit - nitemdrawn/2
		if off < 0 {
			off = 0
		}
		if off > nitem-nitemdrawn {
			off = nitem - nitemdrawn
		}
		lasti = me.LastHit - off
	}
	r := Rect(0, 0, wid, nitemdrawn*(font.Height+menuVspacing)).Inset(-menuMargin)
	r = r.Sub(Pt(wid/2, lasti*(font.Height+menuVspacing)+font.Height/2))
	r = r.Add(mc.Point)
	pt := ZP
	if r.Max.X > screen.R.Max.X {
		pt.X = screen.R.Max.X - r.Max.X
	}
	if r.Max.Y > screen.R.Max.Y {
		pt.Y = screen.R.Max.Y - r.Max.Y
	}
	if r.Min.X < screen.R.Min.X {
		pt.X = screen.R.Min.X - r.Min.X
	}
	if r.Min.Y < screen.R.Min.Y {
		pt.Y = screen.R.Min.Y - r.Min.Y
	}
	menur := r.Add(pt)
	var textr Rectangle
	textr.Max.X = menur.Max.X - menuMargin
	textr.Min.X = textr.Max.X - maxwid
	textr.Min.Y = menur.Min.Y + menuMargin
	textr.Max.Y = textr.Min.Y + nitemdrawn*(font.Height+menuVspacing)
	var scrollr Rectangle
	if scrolling {
		scrollr = menur.Inset(menuBorder)
		scrollr.Max.X = scrollr.Min.X + menuScrollwid
	}

	var b *Image
	var backup *Image
	if scr != nil {
		b, _ = allocwindow(nil, scr, menur, RefBackup, White)
		if b == nil {
			b = screen
		}
		backup = nil
	} else {
		b = screen
		backup, _ = display.AllocImage(menur, screen.Pix, false, White)
		if backup != nil {
			backup.Draw(menur, screen, nil, menur.Min)
		}
	}
	b.Draw(menur, menu.back, nil, ZP)
	b.Border(menur, menuBlackborder, menu.bord, ZP)
	save, _ := display.AllocImage(menurect(display, textr, 0), screen.Pix, false, White)
	r = menurect(display, textr, lasti)
	display.MoveCursor(r.Min.Add(r.Max).Div(2))
	menupaint(b, me, textr, off, nitemdrawn)
	if scrolling {
		menuscrollpaint(b, scrollr, off, nitem, nitemdrawn)
	}
	for mc.Buttons&(1<<(but-1)) != 0 {
		lasti = menuscan(b, me, but, mc, textr, off, lasti, save)
		if lasti >= 0 {
			break
		}
		for !mc.In(textr) && (mc.Buttons&(1<<(but-1))) != 0 {
			if scrolling && mc.In(scrollr) {
				noff := ((mc.Y - scrollr.Min.Y) * nitem) / scrollr.Dy()
				noff -= nitemdrawn / 2
				if noff < 0 {
					noff = 0
				}
				if noff > nitem-nitemdrawn {
					noff = nitem - nitemdrawn
				}
				if noff != off {
					off = noff
					menupaint(b, me, textr, off, nitemdrawn)
					menuscrollpaint(b, scrollr, off, nitem, nitemdrawn)
				}
			}
			mc.Read()
		}
	}
	if b != screen {
		b.Free()
	}
	if backup != nil {
		screen.Draw(menur, backup, nil, menur.Min)
		backup.Free()
	}
	save.Free()
	screen.ReplClipr(false, sc)
	display.Flush()
	if lasti >= 0 {
		me.LastHit = lasti + off
		return me.LastHit
	}
	return -1
}
