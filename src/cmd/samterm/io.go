package main

import "9fans.net/plan9port/draw"

func (t *Term) initio() {
	//FIX: check mousectl, keyboardctl nil like C?
	t.mousectl = t.disp.InitMouse()
	t.keyboardctl = t.disp.InitKeyboard()
	t.hoststart()
	//FIX: add plumbstart
}

func (t *Term) getmouse() {
	//FIX: check failure like C
	t.mousectl.Read()
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
	return t.mousep().Buttons & (1 << (but - 1))
}

//FIX: add externload

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
		t.disp.FlushImage(t.hostlock == 0 && t.block == 0)

		//FIX: use cond
		var hostCh chan int
		if t.block&(1<<RHost) == 0 {
			hostCh = t.hostc
		}
		var kbdCh <-chan rune
		if t.block&(1<<RKeyboard) == 0 {
			kbdCh = t.keyboardctl.C
		}
		var mouseCh <-chan draw.Mouse
		if t.block&(1<<RMouse) == 0 {
			mouseCh = t.mousectl.C
		}
		var resizeCh <-chan bool
		if t.block&(1<<RResize) == 0 {
			resizeCh = t.mousectl.Resize
		}

		select {
		case i, ok := <-hostCh:
			if !ok {
				// The host closed the connection (hostproc closed hostc on
				// EOF). Like C samterm's hostproc, which threadexitsall on an
				// unexpected EOF, exit the event loop so the terminal does not
				// linger and later write to the dead host (a panic). Run checks
				// t.exiting on every iteration.
				t.exiting = true
				return t.got
			}
			t.hostp = t.hostbuf[i].data[:t.hostbuf[i].n]
			t.block = 0
			t.got |= 1 << RHost
			return t.got
		//FIX: add plumb case
		case r := <-kbdCh:
			t.kbdc = int(r)
			t.got |= 1 << RKeyboard
			return t.got
		case m := <-mouseCh:
			t.mousectl.Mouse = m
			t.got |= 1 << RMouse
			return t.got
		case <-resizeCh:
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

//FIX: add rcvstring

func (t *Term) getch() int {
	c := t.rcvchar()
	for ; c == -1; c = t.rcvchar() {
		t.block = ^(1 << RHost)
		t.waitforio()
		t.block = 0
	}
	return c
}

//FIX: add externchar

func (t *Term) ecankbd() bool {
	if t.kpeekc >= 0 {
		return true
	}
	select {
	case r := <-t.keyboardctl.C:
		t.kpeekc = int(r)
		return true
	default:
		return false
	}
}

func (t *Term) ekbd() int {
	if t.kpeekc >= 0 {
		c := t.kpeekc
		t.kpeekc = -1
		return c
	}
	r, ok := <-t.keyboardctl.C
	if !ok {
		//FIX: add error print from C
		// unreachable
		panic("kbd")
	}
	return int(r)
}

func (t *Term) kbdchar() int {
	//FIX: use externchar
	if t.got&(1<<RKeyboard) != 0 {
		c := t.kbdc
		t.kbdc = -1
		t.got &^= 1 << RKeyboard
		return c
	}
	//FIX: plumbc code from C
	if !t.ecankbd() {
		return -1
	}
	return t.ekbd()
}

func (t *Term) qpeekc() int {
	return t.kbdc
}

func (t *Term) RESIZED() bool {
	if t.resized {
		if err := t.disp.Attach(draw.RefNone); err != nil {
			// unreachable
			panic("can't reattach to window")
		}
		t.resized = false
		return true
	}
	return false
}

//FIX: move to term.go

// hoststart launches the goroutine that reads protocol bytes from the host.
func (t *Term) hoststart() {
	t.hostc = make(chan int)
	go t.hostproc()
}

func (t *Term) hostproc() {
	i := 0
	for {
		i = 1 - i // toggle
		n, err := t.hostr.Read(t.hostbuf[i].data[:])
		if n <= 0 || err != nil {
			// EOF or error: the host is gone. Close hostc so waitforio wakes
			// and exits the event loop (see its !ok case), mirroring C
			// samterm's hostproc exiting on an unexpected host EOF. In tests
			// this is how a finished run's terminal shuts down.
			close(t.hostc)
			return
		}
		t.hostbuf[i].n = n
		t.hostc <- i
	}
}
