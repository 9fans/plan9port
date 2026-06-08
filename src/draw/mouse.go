package draw

import (
	"fmt"
	"log"
	"os"

	"9fans.net/go/draw/drawfcall"
)

// Mouse is the structure describing the current state of the mouse.
type Mouse struct {
	Point          // Location.
	Buttons int    // Buttons; bit 0 is button 1, bit 1 is button 2, etc.
	Msec    uint32 // Time stamp in milliseconds.
}

// Mousectl holds the interface to receive mouse events.
//
// This Go library differs from the Plan 9 C library in its updating
// of Mouse. Updating the Mouse field is the duty of every
// receiver from C. The Read method does the update, but any use
// of C in a select needs to update the field as well, as in:
//
//	case mc.Mouse <- mc.C:
//
// In the Plan 9 C library, the sender does the write after the send,
// but that write could not be relied upon due to scheduling delays,
// so receivers conventionally also did the write, as above.
// This write-write race, while harmless, impedes using the race detector
// to find more serious races, and it is easily avoided:
// the receiver is now in charge of updating Mouse.
type Mousectl struct {
	Mouse                // Store Mouse events here.
	C       <-chan Mouse // Channel of Mouse events.
	Resize  <-chan bool  // Each received value signals a window resize (see the display.Attach method).
	Display *Display     // The associated display.
}

// InitMouse connects to the mouse and returns a Mousectl to interact with it.
func (d *Display) InitMouse() *Mousectl {
	ch := make(chan Mouse, 0)
	rch := make(chan bool, 2)
	mc := &Mousectl{
		C:       ch,
		Resize:  rch,
		Display: d,
	}
	go mouseproc(mc, d, ch, rch)
	return mc
}

func mouseproc(mc *Mousectl, d *Display, ch chan Mouse, rch chan bool) {
	for {
		m, resized, err := d.conn.ReadMouse()
		if err != nil {
			log.Fatal("readmouse: ", err)
		}
		if resized {
			rch <- true
		}
		mm := Mouse{Point{m.X, m.Y}, m.Buttons, uint32(m.Msec)}
		ch <- mm
		// No "mc.Mouse = mm" here! See Mousectl doc comment.
	}
}

// Read returns the next mouse event.
func (mc *Mousectl) Read() Mouse {
	mc.Display.Flush()
	m := <-mc.C
	mc.Mouse = m
	return m
}

// MoveCursor moves the mouse cursor to the specified location.
func (d *Display) MoveCursor(pt Point) error {
	d.mu.Lock()
	defer d.mu.Unlock()
	err := d.conn.MoveTo(pt)
	if err != nil {
		fmt.Fprintf(os.Stderr, "MoveTo: %v\n", err)
		return err
	}
	return nil
}

// SwitchCursor sets the mouse cursor to the specified cursor image.
// SwitchCursor(nil) changes the cursor to the standard system cursor.
func (d *Display) SwitchCursor(c *Cursor) error {
	d.mu.Lock()
	defer d.mu.Unlock()
	err := d.conn.Cursor((*drawfcall.Cursor)(c))
	if err != nil {
		fmt.Fprintf(os.Stderr, "SetCursor: %v\n", err)
		return err
	}
	return nil
}

// SwitchCursor2 sets the mouse cursor to the specified cursor images.
// SwitchCursor2(nil, nil) changes the cursor to the standard system cursor.
// If c2 is omitted, a scaled version of c is used instead.
func (d *Display) SwitchCursor2(c *Cursor, c2 *Cursor2) error {
	d.mu.Lock()
	defer d.mu.Unlock()
	err := d.conn.Cursor2((*drawfcall.Cursor)(c), (*drawfcall.Cursor2)(c2))
	if err != nil {
		fmt.Fprintf(os.Stderr, "SetCursor: %v\n", err)
		return err
	}
	return nil
}
