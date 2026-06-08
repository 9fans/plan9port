package draw

import "log"

const (
	KeyFn = '\uF000'

	KeyHome      = KeyFn | 0x0D
	KeyUp        = KeyFn | 0x0E
	KeyPageUp    = KeyFn | 0xF
	KeyPrint     = KeyFn | 0x10
	KeyLeft      = KeyFn | 0x11
	KeyRight     = KeyFn | 0x12
	KeyDown      = 0x80
	KeyView      = 0x80
	KeyPageDown  = KeyFn | 0x13
	KeyInsert    = KeyFn | 0x14
	KeyEnd       = KeyFn | 0x18
	KeyAlt       = KeyFn | 0x15
	KeyShift     = KeyFn | 0x16
	KeyCtl       = KeyFn | 0x17
	KeyBackspace = 0x08
	KeyDelete    = 0x7F
	KeyEscape    = 0x1b
	KeyEOF       = 0x04
	KeyCmd       = 0xF100
)

// Keyboardctl is the source of keyboard events.
type Keyboardctl struct {
	C <-chan rune // Channel on which keyboard characters are delivered.
}

// InitKeyboard connects to the keyboard and returns a Keyboardctl to listen to it.
func (d *Display) InitKeyboard() *Keyboardctl {
	ch := make(chan rune, 20)
	go kbdproc(d, ch)
	return &Keyboardctl{ch}
}

func kbdproc(d *Display, ch chan rune) {
	for {
		r, err := d.conn.ReadKbd()
		if err != nil {
			log.Fatal("readkbd: ", err)
		}
		ch <- r
	}
}
