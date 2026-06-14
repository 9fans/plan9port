package main

import (
	"slices"
	"unicode/utf8"

	"9fans.net/plan9port/draw"
	"9fans.net/plan9port/internal/sam"
)

const (
	Cut = iota
	Paste
	Snarf
	Plumb
	Look
	Exch
	Search
	NMENU2  = Search
	Send    = Search
	NMENU2C = Search + 1
)

const (
	New = iota
	Zerox
	Resize
	Close
	Write
	NMENU3
)

func (t *Term) menuinit() {
	t.menu2str = []string{"cut", "paste", "snarf", "plumb", "look", "<rio>", ""}
	t.menu3str = []string{"new", "zerox", "resize", "close", "write"}
	t.menu2.Gen = t.genmenu2
	t.menu2c.Gen = t.genmenu2c
	t.menu3.Gen = t.genmenu3
}

func (t *Term) menu2hit() {
	tx := t.which.user1
	w := t.windex(tx, t.which)
	if t.hversion == 0 || t.plumbfd < 0 {
		t.menu2str[Plumb] = "(plumb)"
	}
	menu := &t.menu2
	if tx == &t.cmd {
		menu = &t.menu2c
	}
	m := draw.MenuHit(2, t.mouse, menu, nil)
	if t.hostlock != 0 || tx.lock != 0 {
		// untested: the window becomes locked while the button-2 menu is up. The
		// deterministic harness waitframe-syncs after every scripted event, so the
		// host's Hunlock always lands before the next event -- the lock never
		// straddles a menu. Same reason genmenu2/genmenu2c/genmenu3 never render a
		// locked (parenthesized) item below.
		return
	}
	switch m {
	case Cut:
		t.cut(tx, w, true, true)
	case Paste:
		t.paste(tx, w)
	case Snarf:
		t.snarf(tx, w)
	case Plumb:
		if t.hversion > 0 {
			t.outTsll(sam.Tplumb, tx.tag, t.which.p0, t.which.p1)
		}
	case Exch:
		t.snarf(tx, w)
		t.outT0(sam.Tstartsnarf)
		t.setlock()
	case Look:
		t.outTsll(sam.Tlook, tx.tag, t.which.p0, t.which.p1)
		t.setlock()
	case Search: // == Send in cmd window
		t.outcmd()
		if tx == &t.cmd {
			t.outTsll(sam.Tsend, 0, t.which.p0, t.which.p1)
		} else {
			t.outT0(sam.Tsearch)
		}
		t.setlock()
	}
}

func (t *Term) menu3hit() {
	t.mw = -1
	m := draw.MenuHit(3, t.mouse, &t.menu3, nil)
	switch m {
	case -1:
		// nothing

	case New:
		if t.hostlock == 0 {
			t.sweeptext(true, 0)
		}

	case Zerox, Resize:
		if t.hostlock == 0 {
			t.display.SwitchCursor(&bullseye)
			t.buttons(Down)
			cursor := true
			if t.mouse.Buttons&4 != 0 {
				if l := t.flwhich(t.mouse.Point); l != nil {
					if r, ok := t.getr(); ok {
						t.duplicate(l, r, l.f.Font, boolToInt(m == Resize))
						cursor = false
					}
				}
			}
			if cursor {
				t.setcursor(t.cursor)
			}
			t.buttons(Up)
		}

	case Close:
		if t.hostlock == 0 {
			t.display.SwitchCursor(&bullseye)
			t.buttons(Down)
			if l := t.flwhich(t.mouse.Point); (t.mouse.Buttons&4) != 0 && l != nil && t.hostlock == 0 {
				tx := l.user1
				if tx.nwin > 1 {
					t.close(l)
				} else if tx != &t.cmd {
					t.outTs(sam.Tclose, tx.tag)
					t.setlock()
				}
			}
			t.setcursor(t.cursor)
			t.buttons(Up)
		}

	case Write:
		if t.hostlock == 0 {
			t.display.SwitchCursor(&bullseye)
			t.buttons(Down)
			if l := t.flwhich(t.mouse.Point); (t.mouse.Buttons&4) != 0 && l != nil {
				t.outTs(sam.Twrite, l.user1.tag)
				t.setlock()
			} else {
				t.setcursor(t.cursor)
			}
			t.buttons(Up)
		}

	default:
		if tx := t.text[m-NMENU3]; tx != nil {
			i := tx.front
			if tx.nwin == 0 || tx.l[i].textfn == nil {
				// untested: close nils text[m] the moment nwin hits 0, so a
				// reachable menu entry always has a window (nwin>0); the
				// textfn==nil half-built-window case is a transient the
				// frame-synced harness never catches mid-build.
				return // not ready yet; try again later
			}
			if tx.nwin > 1 && t.which == &tx.l[i] {
				for {
					if i++; i == NL {
						i = 0
					}
					if i == tx.front || tx.l[i].textfn != nil {
						break
					}
				}
			}
			t.current(&tx.l[i])
		} else if t.hostlock == 0 {
			t.sweeptext(false, t.tag[m-NMENU3])
		}
	}
}

func (t *Term) sweeptext(isnew bool, tag int) *Text {
	if r, ok := t.getr(); ok {
		tx := new(Text)
		t.current(nil)
		t.flnew(&tx.l[0], t.gettext, 0, tx)
		t.flinit(&tx.l[0], r, t.font, t.maincols[:]) // bnl
		tx.nwin = 1
		tx.rasp.init()
		if isnew {
			t.startnewfile(sam.Tstartnewfile, tx)
		} else {
			tx.rasp.init()
			tx.tag = tag
			t.startfile(tx)
		}
		return tx
	}
	return nil
}

func (t *Term) whichmenu(tg int) int {
	for i, tag := range t.tag {
		if tag == tg {
			return i
		}
	}
	return -1
}

func (t *Term) whichtext(tg int) *Text {
	i := t.whichmenu(tg)
	if i < 0 {
		// unreachable
		panic("whichtext")
	}
	return t.text[i]
}

func (t *Term) menuins(n int, s []byte, text *Text, mod byte, tag int) {
	name := append([]byte{mod}, s...)
	t.name = slices.Insert(t.name, n, name)
	t.text = slices.Insert(t.text, n, text)
	t.tag = slices.Insert(t.tag, n, tag)
	t.menu3.LastHit = n + NMENU3
}

func (t *Term) menudel(n int) {
	if n >= len(t.text) || t.text[n] != nil {
		// unreachable
		panic("menudel")
	}
	t.name = slices.Delete(t.name, n, n+1)
	t.text = slices.Delete(t.text, n, n+1)
	t.tag = slices.Delete(t.tag, n, n+1)
}

func (t *Term) setpat(s string) {
	if len(s) > 15 {
		s = s[:15]
	}
	t.menu2str[Search] = "/" + s
}

func paren(s string) string {
	// untested: only reached for a menu item rendered while its window is locked;
	// see menu2hit for why the deterministic harness never holds a lock across a
	// menu.
	return "(" + s + ")"
}

func (t *Term) genmenu2(n int, buf []byte) ([]byte, bool) {
	tx := t.which.user1
	extra := cond(t.menu2str[Search] != "", 1, 0)
	if n >= NMENU2+extra {
		return nil, false
	}
	p := t.menu2str[n]
	if (t.hostlock == 0 && tx.lock == 0) || n == Search || n == Look {
		return append(buf, p...), true
	}
	// untested: locked file-menu item (see menu2hit)
	return append(buf, paren(p)...), true
}

func (t *Term) genmenu2c(n int, buf []byte) ([]byte, bool) {
	tx := t.which.user1
	if n >= NMENU2C {
		return nil, false
	}
	var p string
	if n == Send {
		p = "send"
	} else {
		p = t.menu2str[n]
	}
	if t.hostlock == 0 && tx.lock == 0 {
		return append(buf, p...), true
	}
	// untested: locked cmd-menu item (see menu2hit)
	return append(buf, paren(p)...), true
}

func (t *Term) genmenu3(n int, buf []byte) ([]byte, bool) {
	if n >= NMENU3+len(t.text) {
		return nil, false
	}
	if n < NMENU3 {
		p := t.menu3str[n]
		if t.hostlock != 0 {
			// untested: locked menu3 item (see menu2hit)
			p = paren(p)
		}
		return append(buf, p...), true
	}
	n -= NMENU3
	if n == 0 { // unless we've been fooled, this is cmd
		return append(buf, t.name[n][1:]...), true
	}
	if t.mw == -1 {
		t.mw = len("~~sam~~")
		for _, name := range t.name[1:] {
			w := utf8.RuneCount(name[1:]) + 4 // include "'+. "
			t.mw = max(t.mw, w)
		}
	}
	const NBUF = 64
	if t.mw > NBUF {
		t.mw = NBUF
	}
	tx := t.text[n]
	mark0 := t.name[n][0]
	mark1 := byte('-')
	mark2 := byte(' ')
	if tx != nil {
		if tx.nwin == 1 {
			mark1 = '+'
		} else if tx.nwin > 1 {
			mark1 = '*'
		}
		if t.work != nil && tx == t.work.user1 {
			mark2 = '.'
			if t.modified {
				mark0 = '\''
			}
		}
	}
	name := string(t.name[n][1:])
	body := name
	if utf8.RuneCountInString(name) > NBUF-4-2 {
		// truncate with "..." in the middle. The two halves are asymmetric,
		// matching C genmenu3: its char counter starts at 4 (the "X- " prefix) for
		// the head but does not count the "..." for the tail, so the head keeps
		// NBUF/2-4 runes and the tail keeps NBUF/2-1.
		runes := []rune(name)
		body = string(runes[:NBUF/2-4]) + "..." + string(runes[len(runes)-(NBUF/2-1):])
	}

	buf = append(buf, mark0, mark1, mark2, ' ')
	buf = append(buf, body...)
	// pad to mw columns
	nr := utf8.RuneCount(buf)
	for nr < t.mw && len(buf) < NBUF-1 {
		nr++
		buf = append(buf, ' ')
	}
	return buf, true
}

// setcursor switches the displayed cursor.
// A nil c restores the default arrow.
func (t *Term) setcursor(c *draw.Cursor) {
	t.display.SwitchCursor(c)
}
