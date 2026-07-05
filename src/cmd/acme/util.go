package main

import (
	"errors"
	"strings"
	"syscall"
	"unicode/utf8"

	"9fans.net/plan9port/draw"
)

// errstr renders err the way plan9port's %r verb does: the bare system error
// string (strerror(errno)), capitalized, without Go's wrapping "open path: "
// prefix or its lowercased first letter. Warning text that embeds an OS error
// then matches the C oracle byte-for-byte (e.g. "No such file or directory").
func errstr(err error) string {
	var e syscall.Errno
	if errors.As(err, &e) {
		s := e.Error()
		if s != "" {
			s = strings.ToUpper(s[:1]) + s[1:]
		}
		return s
	}
	// untested: every -D OS-error warning wraps a syscall.Errno
	return err.Error()
}

// moveto warps the mouse cursor to pt, mirroring libdraw's moveto (mouse.c): it
// moves the devdraw cursor and records the new position in a.mousexy, the -D
// analogue of mousectl->m.xy. The recorded position drives savemouse/
// restoremouse and the mouse-in-tag/body nudges in winresize.
func (a *Acme) moveto(pt draw.Point) {
	if a.disp != nil {
		a.disp.MoveCursor(pt)
	}
	a.mousexy = pt
}

// savemouse records the current mouse position so a later restoremouse can put
// the cursor back, remembering which window asked (util.c).
func (a *Acme) savemouse(w *Window) {
	a.prevmouse = a.mousexy
	a.mousew = w
}

// restoremouse warps the cursor back to the saved position if w is the window
// that saved it, reporting whether it moved (util.c).
func (a *Acme) restoremouse(w *Window) bool {
	did := false
	if a.mousew != nil && a.mousew == w {
		a.moveto(a.prevmouse)
		did = true
	}
	a.mousew = nil
	return did
}

// clearmouse forgets any saved mouse position (util.c).
func (a *Acme) clearmouse() {
	a.mousew = nil
}

// cvttorunes converts the first n bytes of p to runes, appending them to *r
// and setting *nulls if any NUL bytes were elided (util.c cvttorunes). Because
// the Go Buffer is not block-paged, we decode the whole input at once and need
// not track a partial trailing rune.
func cvttorunes(p []byte, n int, r *[]rune, nulls *bool) {
	for i := 0; i < n; {
		c := p[i]
		if c < utf8.RuneSelf {
			i++
			if c != 0 {
				*r = append(*r, rune(c))
			} else if nulls != nil {
				*nulls = true
			}
			continue
		}
		ru, w := utf8.DecodeRune(p[i:n])
		i += w
		*r = append(*r, ru)
	}
}
