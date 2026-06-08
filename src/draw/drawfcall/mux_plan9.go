package drawfcall

import (
	"bufio"
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"image"
	"io/ioutil"
	"os"
	"strconv"
	"strings"
	"sync"
)

type Conn struct {
	ctl      *os.File
	data     *os.File
	cons     *bufio.Reader
	consctl  *os.File
	mouse    *os.File
	snarf    *os.File
	cursor   *os.File
	n        int // connection number
	initCtl  []byte
	oldLabel string

	readData   []byte
	ctlWasRead bool
	lk         sync.Mutex
}

func New() (*Conn, error) {
	ctl, err := os.OpenFile("/dev/draw/new", os.O_RDWR, 0)
	if err != nil {
		return nil, err
	}

	var b [12*12 + 1]byte
	nr, err := ctl.Read(b[:])
	if err != nil {
		return nil, err
	}
	f := strings.Fields(string(b[:nr]))
	if len(f) != 12 {
		return nil, fmt.Errorf("bad ctl file")
	}
	n, err := strconv.Atoi(f[0])
	if err != nil {
		return nil, err
	}

	data, err := os.OpenFile(fmt.Sprintf("/dev/draw/%v/data", n), os.O_RDWR, 0)
	if err != nil {
		return nil, err
	}
	cons, err := os.Open("/dev/cons")
	if err != nil {
		return nil, err
	}

	consctl, err := os.OpenFile("/dev/consctl", os.O_WRONLY, 0)
	if err != nil {
		return nil, err
	}
	_, err = consctl.WriteString("rawon")
	if err != nil {
		return nil, err
	}

	mouse, err := os.OpenFile("/dev/mouse", os.O_RDWR, 0)
	if err != nil {
		return nil, err
	}
	snarf, err := os.Open("/dev/snarf")
	if err != nil {
		return nil, err
	}
	cursor, err := os.OpenFile("/dev/cursor", os.O_WRONLY, 0)
	if err != nil {
		return nil, err
	}

	return &Conn{
		ctl:     ctl,
		data:    data,
		cons:    bufio.NewReader(cons),
		consctl: consctl,
		mouse:   mouse,
		snarf:   snarf,
		cursor:  cursor,
		initCtl: b[:nr],
		n:       n,
	}, nil
}

func (c *Conn) Close() error {
	return c.ctl.Close()
}

func (c *Conn) Init(label, winsize string) error {
	if b, err := ioutil.ReadFile("/dev/label"); err == nil {
		c.oldLabel = string(b)
	}
	// Ignore error because we may not be running in rio
	ioutil.WriteFile("/dev/label", []byte(label), 0600)
	return nil
}

func atoi(s string) (n int) {
	n, _ = strconv.Atoi(s)
	return
}

func (c *Conn) ReadMouse() (m Mouse, resized bool, err error) {
	var buf [1 + 5*12]byte
	var nr int

	nr, err = c.mouse.Read(buf[:])
	if err != nil {
		return
	}
	f := strings.Fields(string(buf[:nr]))
	if len(f) != 5 {
		err = errors.New("bad mouse event")
		return
	}
	m.Point = image.Pt(atoi(f[1]), atoi(f[2]))
	m.Buttons = atoi(f[3])
	m.Msec = atoi(f[4])
	if f[0] == "r" {
		resized = true
	}
	return
}

func (c *Conn) ReadKbd() (r rune, err error) {
	r, _, err = c.cons.ReadRune()
	return
}

func (c *Conn) MoveTo(p image.Point) error {
	_, err := fmt.Fprintf(c.mouse, "m%11d %11d ", p.X, p.Y)
	return err
}

func (c *Conn) Cursor(cursor *Cursor) error {
	if cursor == nil {
		// Revert to default cursor (Arrow)
		_, err := c.cursor.Write([]byte{0})
		return err
	}
	b := make([]byte, 2*4+len(cursor.Clr)+len(cursor.Set))
	i := 0
	binary.LittleEndian.PutUint32(b[i:], uint32(cursor.Point.X))
	i += 4
	binary.LittleEndian.PutUint32(b[i:], uint32(cursor.Point.Y))
	i += 4
	i += copy(b[i:], cursor.Clr[:])
	i += copy(b[i:], cursor.Set[:])
	_, err := c.cursor.Write(b)
	return err
}

func (c *Conn) BounceMouse(m *Mouse) error {
	panic("unimplemented")
}

func (c *Conn) Label(label string) error {
	panic("unimplemented")
}

// Return values are bytes copied, actual size, error.
func (c *Conn) ReadSnarf(b []byte) (int, int, error) {
	_, err := c.snarf.Seek(0, 0)
	if err != nil {
		return 0, 0, err
	}
	n, err := c.snarf.Read(b)
	return n, n, err
}

func (c *Conn) WriteSnarf(snarf []byte) error {
	// /dev/snarf updates when the file is closed, so we must open it for each call
	f, err := os.OpenFile("/dev/snarf", os.O_WRONLY, 0)
	if err != nil {
		return err
	}
	_, err = f.Write(snarf)
	if err != nil {
		return err
	}
	return f.Close()
}

func (c *Conn) Top() error {
	panic("unimplemented")
}

func (c *Conn) Resize(r image.Rectangle) error {
	panic("unimplemented")
}

func (c *Conn) ReadDraw(b []byte) (n int, err error) {
	c.lk.Lock()
	if len(c.readData) > 0 {
		n = copy(b, c.readData)
		c.readData = c.readData[n:]
		c.lk.Unlock()
		return n, nil
	}
	c.lk.Unlock()
	return c.data.Read(b[:])
}

func bplong(b []byte, n uint32) {
	binary.LittleEndian.PutUint32(b, n)
}

func (c *Conn) WriteDraw(b []byte) (int, error) {
	i := 0
Loop:
	for i < len(b) {
		switch b[i] {
		case 'J': // set image 0 to screen image
			i++

		case 'I': // get image info: 'I'
			c.lk.Lock()
			if !c.ctlWasRead {
				c.readData = append(c.readData, c.initCtl...)
				c.ctlWasRead = true
			} else {
				b := make([]byte, 12*12)
				n, err := c.ctl.Read(b)
				if err != nil {
					c.lk.Unlock()
					return 0, err
				}
				c.readData = append(c.readData, b[:n]...)
			}
			c.lk.Unlock()
			i++

		case 'q': // query: 'Q' n[1] queryspec[n]
			if bytes.Equal(b, []byte{'q', 1, 'd'}) {
				dpi := fmt.Sprintf("%12d", 100)
				c.lk.Lock()
				c.readData = append(c.readData, []byte(dpi)...)
				c.lk.Unlock()
			}
			i += 1 + 1 + int(b[1])

		default:
			break Loop
		}
	}
	if len(b[i:]) == 0 {
		return i, nil
	}
	n, err := c.data.Write(b[i:])
	return n + i, err
}
