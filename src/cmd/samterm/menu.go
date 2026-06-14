package main

import (
	"strings"
	"unicode/utf8"

	"9fans.net/plan9port/draw"
)

//FIX: drop m prefix and use C names
const (
	mCut = iota
	mPaste
	mSnarf
	mPlumb
	mLook
	mExch
	mSearch
	NMENU2  = mSearch
	mSend   = mSearch
	NMENU2C = mSearch + 1
)

//FIX: drop m prefix and use C names
const (
	mNew = iota
	mZerox
	mResize
	mClose
	mWrite
	NMENU3
)

func (t *Term) menuinit() {
	t.menu2str = []string{"cut", "paste", "snarf", "plumb", "look", "<rio>", ""}
	t.menu3str = []string{"new", "zerox", "resize", "close", "write"}
	//FIX: rewrite genmenu2, genmenu2c, genmenu3 so this can be t.menu2.Gen = t.genmenu2 etc.
	t.menu2.Gen = func(k int, buf []byte) ([]byte, bool) { return t.genmenu2(k) }
	t.menu2c.Gen = func(k int, buf []byte) ([]byte, bool) { return t.genmenu2c(k) }
	t.menu3.Gen = func(k int, buf []byte) ([]byte, bool) { return t.genmenu3(k) }
}

func (t *Term) menu2hit() {
	tx := t.which.user1
	w := t.windex(tx, t.which)
	if t.hversion == 0 || t.plumbfd < 0 {
		t.menu2str[mPlumb] = "(plumb)"
	}
	menu := &t.menu2
	if tx == &t.cmd {
		menu = &t.menu2c
	}
	m := draw.MenuHit(2, t.mousectl, menu, nil)
	if t.hostlock != 0 || tx.lock != 0 {
		// untested: the window becomes locked while the button-2 menu is up. The
		// deterministic harness waitframe-syncs after every scripted event, so the
		// host's Hunlock always lands before the next event -- the lock never
		// straddles a menu. Same reason genmenu2/genmenu2c/genmenu3 never render a
		// locked (parenthesized) item below.
		return
	}
	switch m {
	case mCut:
		t.cut(tx, w, 1, 1)
	case mPaste:
		t.paste(tx, w)
	case mSnarf:
		t.snarf(tx, w)
	case mPlumb:
		if t.hversion > 0 {
			t.outTsll(Tplumb, tx.tag, t.which.p0, t.which.p1)
		}
	case mExch:
		t.snarf(tx, w)
		t.outT0(Tstartsnarf)
		t.setlock()
	case mLook:
		t.outTsll(Tlook, tx.tag, t.which.p0, t.which.p1)
		t.setlock()
	case mSearch: // == Send in cmd window
		t.outcmd()
		if tx == &t.cmd {
			t.outTsll(Tsend, 0, t.which.p0, t.which.p1)
		} else {
			t.outT0(Tsearch)
		}
		t.setlock()
	}
}

func (t *Term) menu3hit() {
	t.mw = -1
	m := draw.MenuHit(3, t.mousectl, &t.menu3, nil)
	//FIX: switch m
	switch {
	case m == -1:
		// nothing

	case m == mNew:
		if t.hostlock == 0 {
			t.sweeptext(1, 0)
		}

	case m == mZerox || m == mResize:
		if t.hostlock == 0 {
			t.disp.SwitchCursor(&t.bullseye)
			t.buttons(Down)
			cursor := true
			if t.mousep().Buttons&4 != 0 {
				var r draw.Rectangle
				if l := t.flwhich(t.mousep().Point); l != nil && t.getr(&r) {
					t.duplicate(l, r, l.f.Font, boolToInt(m == mResize))
					cursor = false
				}
			}
			if cursor {
				t.setcursor(t.cursor)
			}
			t.buttons(Up)
		}

	case m == mClose:
		if t.hostlock == 0 {
			t.disp.SwitchCursor(&t.bullseye)
			t.buttons(Down)
			if l := t.flwhich(t.mousep().Point); (t.mousep().Buttons&4) != 0 && l != nil && t.hostlock == 0 {
				tx := l.user1
				if tx.nwin > 1 {
					t.closeup(l)
				} else if tx != &t.cmd {
					t.outTs(Tclose, tx.tag)
					t.setlock()
				}
			}
			t.setcursor(t.cursor)
			t.buttons(Up)
		}

	case m == mWrite:
		if t.hostlock == 0 {
			t.disp.SwitchCursor(&t.bullseye)
			t.buttons(Down)
			if l := t.flwhich(t.mousep().Point); (t.mousep().Buttons&4) != 0 && l != nil {
				t.outTs(Twrite, l.user1.tag)
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
				// untested: closeup nils text[m] the moment nwin hits 0, so a
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
			t.sweeptext(0, t.tag[m-NMENU3])
		}
	}
}

//FIX: make isnew bool
func (t *Term) sweeptext(isnew, tag int) *Text {
	var r draw.Rectangle
	//FIX: make getr return r, ok
	if t.getr(&r) {
		tx := new(Text)
		t.current(nil)
		t.flnew(&tx.l[0], t.gettext, 0, tx)
		t.flinit(&tx.l[0], r, t.font, t.maincols[:]) // bnl
		tx.nwin = 1
		tx.rasp.init()
		if isnew != 0 {
			t.startnewfile(Tstartnewfile, tx)
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
	for i := 0; i < t.nname; i++ {
		if t.tag[i] == tg {
			return i
		}
	}
	return -1
}

func (t *Term) whichtext(tg int) *Text {
	//FIX: use range
	for i := 0; i < t.nname; i++ {
		if t.tag[i] == tg {
			return t.text[i]
		}
	}
	// unreachable
	panic("whichtext")
}

//FIX: rename m to mod byte
func (t *Term) menuins(n int, s []byte, tx *Text, m, tg int) {
	//FIX: use slices.Insert
	t.name = append(t.name, nil)
	t.text = append(t.text, nil)
	t.tag = append(t.tag, 0)
	copy(t.name[n+1:], t.name[n:])
	copy(t.text[n+1:], t.text[n:])
	copy(t.tag[n+1:], t.tag[n:])

	t.text[n] = tx
	t.tag[n] = tg
	//FIX: use append
	nm := make([]byte, len(s)+1)
	nm[0] = byte(m)
	copy(nm[1:], s)
	t.name[n] = nm
	t.nname++
	t.menu3.LastHit = n + NMENU3
}

func (t *Term) menudel(n int) {
	if t.nname == 0 || n >= t.nname || t.text[n] != nil {
		// unreachable
		panic("menudel")
	}
	//FIX: use slices.Delete
	t.nname--
	t.name = append(t.name[:n], t.name[n+1:]...)
	t.text = append(t.text[:n], t.text[n+1:]...)
	t.tag = append(t.tag[:n], t.tag[n+1:]...)
}

func (t *Term) setpat(s string) {
	if len(s) > 15 {
		s = s[:15]
	}
	t.menu2str[mSearch] = "/" + s
}

func paren(s string) string {
	// untested: only reached for a menu item rendered while its window is locked;
	// see menu2hit for why the deterministic harness never holds a lock across a
	// menu.
	return "(" + s + ")"
}

func (t *Term) genmenu2(n int) ([]byte, bool) {
	tx := t.which.user1
	//FIX: use cond
	extra := 0
	if t.menu2str[mSearch] != "" {
		extra = 1
	}
	if n >= NMENU2+extra {
		return nil, false
	}
	p := t.menu2str[n]
	if (t.hostlock == 0 && tx.lock == 0) || n == mSearch || n == mLook {
		return []byte(p), true
	}
	// untested: locked file-menu item (see menu2hit)
	return []byte(paren(p)), true
}

func (t *Term) genmenu2c(n int) ([]byte, bool) {
	tx := t.which.user1
	if n >= NMENU2C {
		return nil, false
	}
	var p string
	if n == mSend {
		p = "send"
	} else {
		p = t.menu2str[n]
	}
	if t.hostlock == 0 && tx.lock == 0 {
		return []byte(p), true
	}
	// untested: locked cmd-menu item (see menu2hit)
	return []byte(paren(p)), true
}

func (t *Term) genmenu3(n int) ([]byte, bool) {
	if n >= NMENU3+t.nname {
		return nil, false
	}
	if n < NMENU3 {
		p := t.menu3str[n]
		if t.hostlock != 0 {
			// untested: locked menu3 item (see menu2hit)
			p = paren(p)
		}
		return []byte(p), true
	}
	n -= NMENU3
	if n == 0 { // unless we've been fooled, this is cmd
		return append([]byte(nil), t.name[n][1:]...), true
	}
	if t.mw == -1 {
		t.mw = len("~~sam~~")
		//FIX: use max
		for i := 1; i < t.nname; i++ {
			w := utf8.RuneCount(t.name[i][1:]) + 4 // include "'+. "
			if w > t.mw {
				t.mw = w
			}
		}
	}
	const NBUF = 64
	if t.mw > NBUF {
		t.mw = NBUF
	}
	tx := t.text[n]
	var b strings.Builder
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
			if t.modified != 0 {
				mark0 = '\''
			}
		}
	}
	//FIX: should t.name[n] be type string already?
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
	//FIX: seems like we don't really need strings.Builder here.
	b.WriteByte(mark0)
	b.WriteByte(mark1)
	b.WriteByte(mark2)
	b.WriteByte(' ')
	b.WriteString(body)
	// pad to mw columns
	//FIX: predeclare a string of 64 spaces and just slice it to the right length
	for utf8.RuneCountInString(b.String()) < t.mw && b.Len() < NBUF-1 {
		b.WriteByte(' ')
	}
	return []byte(b.String()), true
}

// setcursor switches the displayed cursor.
// A nil c restores the default arrow.
func (t *Term) setcursor(c *draw.Cursor) {
	t.disp.SwitchCursor(c)
}
