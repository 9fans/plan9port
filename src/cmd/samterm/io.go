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
		t.disp.Flush()

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
		case i := <-hostCh:
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
			// EOF or error: stop feeding the loop. In tests this happens
			// when the host side closes the connection.
			return
		}
		t.hostbuf[i].n = n
		t.hostc <- i
	}
}
