package main

import (
	"9fans.net/plan9port/draw"
)

// Mouse routing (acme.c mousethread) and the scrollbar drag loop (scrl.c).
//
// The live mousethread (acme.go) selects over the resize/mouse/plumb/warning
// channels and, on each mouse event, runs the dispatch below under row.lk. Under
// -D the scripted driver (driver.go) feeds events through a.mousectl and calls
// mousedispatch once per press, exercising the same routing without the loop.

// (winlock/winunlock now live in wind.go, faithful to wind.c.)

// mousedispatch routes one mouse event m (the MMouse case of acme.c's
// mousethread): hit-test the row, log a focus change, commit the previously
// focused window, then send the press to the scrollbar (textscroll),
// scroll-wheel (texttype), tag drag (rowdragcol/coldragwin), or body
// select/execute/look handlers. The handlers read the rest of the scripted
// sweep from a.mousectl.
func (a *Acme) mousedispatch(m draw.Mouse) {
	const shift = 5
	t := a.rowwhich(&a.row, m.Point)

	if (t != a.mousetext && t != nil && t.w != nil) &&
		(a.mousetext == nil || a.mousetext.w == nil || t.w.id != a.mousetext.w.id) {
		a.xfidlog(t.w, "focus")
	}

	if t != a.mousetext && a.mousetext != nil && a.mousetext.w != nil {
		a.winlock(a.mousetext.w, 'M')
		a.mousetext.eq0 = ^0
		a.wincommit(a.mousetext.w, a.mousetext)
		a.winunlock(a.mousetext.w)
	}
	a.mousetext = t
	if t == nil {
		// untested: every -D mouse verb presses inside an existing text, so the
		// hit-test never misses (the live editor reaches this over empty space).
		// untested: live -r swap / scroll-debounce timer path not reached by -D
		return
	}
	w := t.w
	if m.Buttons == 0 {
		// untested: the verbs only dispatch a button-down press; the release
		// (buttons==0) is consumed by the handler, not entered here.
		return
	}
	but := 0
	switch m.Buttons {
	case 1:
		but = 1
	case 2:
		but = 2
	case 4:
		but = 3
	}
	// barttext is the text the scroll keys (and bartflag keyboard routing) act
	// on; the live keyboardthread/rowtype consult it (acme.c mousethread).
	a.barttext = t
	if t.what == Body && m.Point.In(t.scrollr) {
		if but != 0 {
			if a.swapscrollbuttons {
				// the -r flag swaps scroll buttons 1 and 3 (acme.c mousethread).
				// untested: -D never sets -r.
				if but == 1 {
					// untested: live -r swap / scroll-debounce timer path not reached by -D
					but = 3
					// untested: live -r swap / scroll-debounce timer path not reached by -D
				} else if but == 3 {
					// untested: live -r swap / scroll-debounce timer path not reached by -D
					but = 1
				}
			}
			a.winlock(w, 'M')
			t.eq0 = ^0
			a.textscroll(t, but)
			a.winunlock(w)
		}
		return
	}
	// scroll buttons, wheels, etc.
	if w != nil && m.Buttons&(8|16) != 0 {
		if m.Buttons&8 != 0 {
			but = Kscrolloneup
		} else {
			but = Kscrollonedown
		}
		a.winlock(w, 'M')
		t.eq0 = ^0
		a.texttype(t, rune(but))
		a.winunlock(w)
		return
	}
	if m.Point.In(t.scrollr) {
		if but != 0 {
			if t.what == Columntag {
				a.rowdragcol(&a.row, t.col, but)
			} else if t.what == Tag {
				a.coldragwin(t.col, t.w, but)
				if t.w != nil {
					a.barttext = &t.w.body
				}
			}
			if t.col != nil {
				a.activecol = t.col
			}
		}
		return
	}
	if m.Buttons != 0 {
		if w != nil {
			a.winlock(w, 'M')
		}
		t.eq0 = ^0
		if w != nil {
			a.wincommit(w, t)
		} else {
			// untested: a mouse press on a column/row tag (t.w==nil); the -D
			// mouse verbs only target window bodies and tags (t.w!=nil).
			// untested: live -r swap / scroll-debounce timer path not reached by -D
			a.textcommit(t, true)
		}
		if m.Buttons&1 != 0 {
			a.textselect(t)
			if w != nil {
				a.winsettag(w)
			}
			a.argtext = t
			a.seltext = t
			if t.col != nil {
				a.activecol = t.col // button 1 only
			}
			if t.w != nil && t == &t.w.body {
				a.activewin = t.w
			}
		} else if m.Buttons&2 != 0 {
			if q0, q1, argt, ok := a.textselect2(t); ok {
				a.execute(t, q0, q1, false, argt)
			}
		} else if m.Buttons&(4|(4<<shift)) != 0 {
			if q0, q1, ok := a.textselect3(t); ok {
				a.look3(t, q0, q1, false, m.Buttons&(4<<shift) != 0)
			}
		}
		if w != nil {
			a.winunlock(w)
		}
	}
}

// scrsleep is the scroll-loop throttle (scrl.c). The live editor sleeps dt ms
// while watching the mouse, alt()ing over a timerstart channel and the mouse
// channel (scrl.c scrsleep). The live editor uses the wall-clock timerproc, so
// the auto-repeat actually waits dt ms; under -D there is no timerproc and the
// scripted events are pre-queued, so the timer is advanced deterministically.
func (a *Acme) scrsleep(dt uint) {
	mc := a.mousectl
	timer := a.timerstart(int(dt))
	if a.tmr.lk == nil {
		// -D: there is no wall-clock timerproc to fire the timer, and the
		// scripted mouse events are all already queued, so a live alt() would
		// race nondeterministically. Resolve it deterministically: take a queued
		// mouse event if one is waiting (as C's alt does when the mouse beats the
		// timer), otherwise let the timer fire. timertick then retires the timer
		// the same way timerproc would, exercising the real fire/cancel path.
		fired := false
		select {
		case m := <-mc.C:
			mc.Mouse = m // a real event arrived (timercancel)
			a.timercancel(timer)
		default:
			fired = true
		}
		a.timertick(int(dt))
		if fired {
			<-timer.c // timertick dropped it from active without freeing
			a.timerstop(timer)
		}
		return
	}
	// live editor: alt over the wall-clock timer and the mouse (scrl.c scrsleep).
	select {
	case <-timer.c:
		a.timerstop(timer)
	case m := <-mc.C:
		// untested: live -r swap / scroll-debounce timer path not reached by -D
		mc.Mouse = m // a real event arrived (timercancel)
		a.timercancel(timer)
	}
}

// textscroll is the scrollbar drag loop (scrl.c textscroll): button 1 scrolls
// back by lines, button 2 jumps to an absolute position, button 3 scrolls
// forward, repeating while the button is held. Driven under -D by the `scroll`
// verb through mousedispatch.
func (a *Acme) textscroll(t *Text, but int) {
	mc := a.mousectl
	s := t.scrollr.Inset(1)
	h := s.Max.Y - s.Min.Y
	x := (s.Min.X + s.Max.X) / 2
	oldp0 := -1
	first := true
	for {
		a.disp.FlushImage(true)
		my := mc.Point.Y
		if my < s.Min.Y {
			// untested: the scroll verb presses at a y inside the bar, so the
			// top/bottom edge clamps are not exercised.
			// untested: live -r swap / scroll-debounce timer path not reached by -D
			my = s.Min.Y
		}
		if my >= s.Max.Y {
			// untested: as above (bottom edge).
			my = s.Max.Y
		}
		if mc.Point != draw.Pt(x, my) {
			// untested: the scroll verb presses at the scrollbar centre x with a
			// y inside the bar, so this cursor-warp absorb is never taken.
			a.moveto(draw.Pt(x, my))
			mc.Read() // absorb event generated by moveto()
		}
		if but == 2 {
			y := my
			p0 := t.file.b.nc() * (y - s.Min.Y) / h
			if p0 >= t.q1 {
				p0 = a.textbacknl(t, p0, 2)
			}
			if oldp0 != p0 {
				a.textsetorigin(t, p0, false)
			}
			oldp0 = p0
			mc.Read()
			// C's `continue` in the do-while jumps to the loop condition, so the
			// button-release still ends the loop; mirror that check here.
			if mc.Buttons&(1<<(but-1)) == 0 {
				break
			}
			continue
		}
		var p0 int
		if but == 1 {
			p0 = a.textbacknl(t, t.org, (my-s.Min.Y)/t.fr.Font.Height)
		} else {
			p0 = t.org + t.fr.CharOf(draw.Pt(s.Max.X, my))
		}
		if oldp0 != p0 {
			a.textsetorigin(t, p0, true)
		}
		oldp0 = p0
		// debounce: C sleeps 200ms then nbrecv's any event that arrived during
		// the sleep. Under -D there is no real-time gap and the scripted next
		// event (a drag, or the release) is always already queued, so the
		// non-blocking drain becomes a plain receive of that one event.
		if first {
			a.disp.FlushImage(true)
			mc.Read()
			first = false
		}
		a.scrsleep(80)
		if mc.Buttons&(1<<(but-1)) == 0 {
			break
		}
	}
	for mc.Buttons != 0 {
		// untested: under -D the release is the last queued event, so the loop
		// above already left buttons == 0.
		// untested: live -r swap / scroll-debounce timer path not reached by -D
		mc.Read()
	}
}
