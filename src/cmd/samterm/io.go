package main

import (
	"fmt"
	"os"
	"unicode/utf8"

	"9fans.net/plan9port/draw"
)

func (t *Term) initio() {
	// TODO: Not checking for nil because InitMouse and InitKeyboard
	// crash on failure, although perhaps they should not.
	t.mouse = t.display.InitMouse()
	t.keyboardctl = t.display.InitKeyboard()
	t.hoststart()
	t.plumbstart()
}

func (t *Term) getmouse() {
	// TODO: Not checking for error because Read errors are fatal,
	// although perhaps they should not be.
	t.mouse.Read()
}

func (t *Term) mouseunblock() {
	t.got &^= 1 << RMouse
}

func (t *Term) kbdblock() {
	// ca suffit
	t.block = 1<<RKeyboard | 1<<RPlumb
}

func (t *Term) button(but int) int {
	t.getmouse()
	return t.mouse.Buttons & (1 << (but - 1))
}

// externload copies the formatted plumb text in plumbbuf[i] into the extern
// buffer and raises the RPlumb bit, so externchar will feed it as keyboard-like
// input. (C also calls drawtopwindow to raise the host window; that is a no-op
// for the headless test backend and not needed for the protocol, so it is
// omitted.)
func (t *Term) externload(i int) {
	t.extern = append(t.extern[:0], t.plumbbuf[i].data[:t.plumbbuf[i].n]...)
	t.externi = 0
	t.got |= 1 << RPlumb
}

func chanCond[T any](ch <-chan T, cond bool) <-chan T {
	if cond {
		return ch
	}
	return nil
}

func (t *Term) waitforio() int {
	for {
		if t.got&^t.block != 0 {
			return t.got &^ t.block
		}
		// Make drawing visible only when the terminal is idle -- waiting for a
		// fresh top-level event (block == 0), with the host not holding it
		// locked (hostlock == 0). Both conditions defer the 'v', and a frame,
		// past points that depend only on how the OS chunked the protocol
		// stream:
		//   - block != 0 means this is getch's mid-message wait for more host
		//     bytes (block = ^(1<<RHost)); the update is not finished, so a
		//     visible flush here would split one burst into a chunking-dependent
		//     number of frames.
		//   - hostlock != 0 means the host is mid-update between a lock and its
		//     Hunlock (e.g. a pending server round-trip), even while idle.
		// Deferring to idle-and-unlocked collapses each logical update into a
		// single frame. C samterm's waitforio does the same. The draw buffer is
		// still sent (visible=false) so devdraw sees the ops promptly.
		t.display.FlushImage(t.hostlock == 0 && t.block == 0)

		select {
		case i, ok := <-chanCond(t.hostc, t.block&(1<<RHost) == 0):
			if !ok {
				// The host closed the connection (hostproc closed hostc after
				// recording hosterr). C's hostproc prints the read error and
				// exits the whole process here (threadexitsall); raise a
				// hostError instead, unwinding to run's recover.
				panic(hostError{fmt.Sprintf("host read error: %v", t.hosterr)})
			}
			t.hostp = t.hostbuf[i].data[:t.hostbuf[i].n]
			t.block = 0
			t.got |= 1 << RHost
			return t.got
		case i := <-chanCond(t.plumbc, t.block&(1<<RPlumb) == 0):
			t.externload(i)
			return t.got
		case r := <-chanCond(t.keyboardctl.C, t.block&(1<<RKeyboard) == 0):
			t.kbdc = int(r)
			t.got |= 1 << RKeyboard
			return t.got
		case m := <-chanCond(t.mouse.C, t.block&(1<<RMouse) == 0):
			t.mouse.Mouse = m
			t.got |= 1 << RMouse
			return t.got
		case <-chanCond(t.mouse.Resize, t.block&(1<<RResize) == 0):
			t.resized = true
			// do the resize in line if we've finished initializing
			// and we're not in a blocking state
			if t.hasunlocked != 0 && t.block == 0 && t.RESIZED() {
				t.resize()
			}
			// loop again (resize does not set a got bit)
		}
	}
}

func (t *Term) rcvchar() int {
	if t.got&(1<<RHost) == 0 {
		return -1
	}
	c := int(t.hostp[0])
	t.hostp = t.hostp[1:]
	if len(t.hostp) == 0 {
		t.got &^= 1 << RHost
	}
	return c
}

// rcvstring returns the unread host bytes for dumperrmsg's diagnostic,
// consuming them (the rest of the malformed message's buffer is dropped).
func (t *Term) rcvstring() string {
	t.got &^= 1 << RHost
	return string(t.hostp)
}

func (t *Term) getch() int {
	c := t.rcvchar()
	for ; c == -1; c = t.rcvchar() {
		// untested: getch blocks only when a handler reads bytes (Hsetsnarf/Hplumb
		// data) that arrive in a later host read than the message header. Whether
		// the pipe splits them that way is timing-dependent, so the harness cannot
		// force it deterministically.
		t.block = ^(1 << RHost)
		t.waitforio()
		t.block = 0
	}
	return c
}

// externchar returns the next rune of the loaded plumb text, or -1 when none is
// pending. The RPlumb bit stays set until the buffer is drained; NUL runes are
// skipped (as in C's externchar).
func (t *Term) externchar() int {
	for t.got&((1<<RPlumb)&^t.block) != 0 {
		r, size := utf8.DecodeRune(t.extern[t.externi:])
		t.externi += size
		if t.externi >= len(t.extern) {
			t.got &^= 1 << RPlumb
			t.extern = t.extern[:0]
		}
		if r == 0 {
			continue
		}
		return int(r)
	}
	return -1
}

// ecankbd/ekbd peek the keyboard channel for a SECOND character already queued
// behind the one kbdchar is consuming. The deterministic test harness delivers
// exactly one input event per event-loop iteration (DEVDRAW_TEST_NOCOALESCE), so
// a second char is never buffered: only ecankbd's "channel empty" path is
// reached. The buffered-char branches below are therefore untested -- and so are
// their C equivalents in the -uncoverc residual.
func (t *Term) ecankbd() bool {
	if t.kpeekc >= 0 {
		// untested: a peeked char already cached (kbdchar peeks at most once)
		return true
	}
	select {
	case r := <-t.keyboardctl.C:
		// untested: a second keyboard char already queued (see ecankbd doc)
		t.kpeekc = int(r)
		return true
	default:
		return false
	}
}

func (t *Term) ekbd() int {
	// untested: ekbd runs only after ecankbd saw a SECOND buffered keyboard char,
	// which the deterministic one-event-per-loop harness never produces (these
	// lines are likewise uncovered in the C -uncoverc residual).
	if t.kpeekc >= 0 {
		// untested: see top of ekbd
		c := t.kpeekc
		t.kpeekc = -1
		return c
	}
	// untested: see top of ekbd
	r, ok := <-t.keyboardctl.C
	if !ok {
		// unreachable
		fmt.Fprintf(os.Stderr, "samterm: keybard recv error\n") // sic, as in C
		panic("kbd")
	}
	// untested: see top of ekbd
	return int(r)
}

func (t *Term) kbdchar() int {
	if c := t.externchar(); c > 0 {
		return c
	}
	if t.got&(1<<RKeyboard) != 0 {
		c := t.kbdc
		t.kbdc = -1
		t.got &^= 1 << RKeyboard
		return c
	}
	// Drain any plumb notifications that arrived without going through
	// waitforio (C uses a non-blocking nbrecv loop on plumbc). untested: a plumb
	// message arriving mid-type, between waitforio dispatches; TestPlumbRecv
	// delivers plumbs through waitforio's RPlumb case, not this drain.
Drain:
	for t.plumbc != nil {
		// untested: a plumb message arriving mid-type, between waitforio
		// dispatches; TestPlumbRecv delivers plumbs through waitforio's RPlumb
		// case, not this non-blocking drain.
		select {
		case i := <-t.plumbc:
			// untested: see the loop comment above
			t.externload(i)
			if c := t.externchar(); c > 0 {
				// untested: see the loop comment above
				return c
			}
			// untested: see the loop comment above (loop again to drain more)
		default:
			// untested: see the loop comment above (no message pending)
			break Drain
		}
	}
	if !t.ecankbd() {
		return -1
	}
	// untested: reached only when ecankbd found a second buffered char (see ekbd)
	return t.ekbd()
}

func (t *Term) qpeekc() int {
	return t.kbdc
}

func (t *Term) RESIZED() bool {
	if t.resized {
		if err := t.display.Attach(draw.RefNone); err != nil {
			// unreachable
			panic("can't reattach to window")
		}
		t.screen = t.display.ScreenImage
		t.resized = false
		return true
	}
	return false
}
