package draw

import (
	"fmt"
	"os"
	"runtime"
	"strings"
)

// AllocImage allocates a new Image on display d.
// It will have the rectangle, pixel format (RGBA32 etc),
// and replication flag given by its arguments.
//
// All the new image's pixels will have the initial color
// (Black, White, Red, and so on).
// If color is NoFill, the pixels will be left uninitialized.
// (In Go, uninitialized means zeroed, so the pixels will be
// Transparent or Black depending on whether the pixel
// format includes an alpha channel.)
//
// If repl is true, the result's Clipr rectangle is set to a very large region.
// Otherwise, the result's Clipr is set to r.
//
// The result's Depth is computed from pix.
//
// AllocImage returns an error if the display cannot allocate the image
// or is no longer available.
//
// For example, to allocate a single-pixel replicated image that may be
// used to paint a region red:
//
//	red, err := display.AllocImage(draw.Rect(0, 0, 1, 1), draw.RGB24, true, draw.Red)
func (d *Display) AllocImage(r Rectangle, pix Pix, repl bool, color Color) (*Image, error) {
	d.mu.Lock()
	defer d.mu.Unlock()
	return allocImage(d, nil, r, pix, repl, color, 0, 0)
}

func (d *Display) allocImage(r Rectangle, pix Pix, repl bool, val Color) (i *Image, err error) {
	return allocImage(d, nil, r, pix, repl, val, 0, 0)
}

func allocImage(d *Display, ai *Image, r Rectangle, pix Pix, repl bool, val Color, screenid uint32, refresh int) (i *Image, err error) {
	defer func() {
		if err != nil {
			err = fmt.Errorf("allocimage %v %v: %v", r, pix, err)
			i.free()
			i = nil
		}
	}()

	depth := pix.Depth()
	if depth == 0 {
		err = fmt.Errorf("bad channel descriptor")
		return
	}

	// flush pending data so we don't get error allocating the image
	d.flush(false)
	a := d.bufimage(1 + 4 + 4 + 1 + 4 + 1 + 4*4 + 4*4 + 4)
	d.imageid++
	id := d.imageid
	a[0] = 'b'
	bplong(a[1:], id)
	bplong(a[5:], screenid)
	a[9] = byte(refresh)
	bplong(a[10:], uint32(pix))
	if repl {
		a[14] = 1
	} else {
		a[14] = 0
	}
	bplong(a[15:], uint32(r.Min.X))
	bplong(a[19:], uint32(r.Min.Y))
	bplong(a[23:], uint32(r.Max.X))
	bplong(a[27:], uint32(r.Max.Y))
	clipr := r
	if repl {
		// huge but not infinite, so various offsets will leave it huge, not overflow
		clipr = Rect(-0x3FFFFFFF, -0x3FFFFFFF, 0x3FFFFFFF, 0x3FFFFFFF)
	}
	bplong(a[31:], uint32(clipr.Min.X))
	bplong(a[35:], uint32(clipr.Min.Y))
	bplong(a[39:], uint32(clipr.Max.X))
	bplong(a[43:], uint32(clipr.Max.Y))
	bplong(a[47:], uint32(val))
	if err = d.flush(false); err != nil {
		return
	}

	i = ai
	if i == nil {
		i = new(Image)
	}
	*i = Image{
		Display: d,
		id:      id,
		Pix:     pix,
		Depth:   pix.Depth(),
		R:       r,
		Clipr:   clipr,
		Repl:    repl,
	}
	runtime.SetFinalizer(i, (*Image).Free)
	return i, nil
}

func namedImage(d *Display, ai *Image, name string) (i *Image, err error) {
	n := len(name)
	if n >= 256 {
		return nil, fmt.Errorf("namedImage: name too long")
	}
	// flush pending data so we don't get error allocating the image
	d.flush(false)
	a := d.bufimage(1 + 4 + 1 + n)
	d.imageid++
	id := d.imageid
	a[0] = 'n'
	bplong(a[1:], id)
	a[5] = byte(n)
	copy(a[6:], name)
	if err := d.flush(false); err != nil {
		fmt.Fprintf(os.Stderr, "namedImage: %v\n", err)
		return nil, err
	}

	a = d.bufimage(1)
	a[0] = 'I'
	if err := d.flush(false); err != nil {
		fmt.Fprintf(os.Stderr, "cannot read image info: %v\n", err)
		return nil, err
	}
	info := make([]byte, 12*12)
	n, err = d.conn.ReadDraw(info)
	if err != nil {
		return nil, err
	}
	if n < len(info) {
		return nil, fmt.Errorf("short info from rddraw")
	}

	pix, err := ParsePix(strings.TrimSpace(string(info[2*12 : 3*12])))
	if err != nil {
		a := d.bufimage(1 + 4)
		a[0] = 'f'
		bplong(a[1:], id)
		d.flush(false)
		return nil, fmt.Errorf("bad channel %q from devdraw", info[2*12:3*12])
	}
	i = ai
	if i == nil {
		i = new(Image)
	}
	*i = Image{
		Display: d,
		id:      id,
		Pix:     pix,
		Depth:   pix.Depth(),
		Repl:    atoi(info[3*12:]) > 0,
		R:       ator(info[4*12:]),
		Clipr:   ator(info[8*12:]),
		Screen:  nil,
		next:    nil,
	}
	runtime.SetFinalizer(i, (*Image).Free)
	return i, nil
}

/*
func nameimage(i *Image, name string, in bool) error {
	a := i.Display.bufimage(1+4+1+1+len(name))
	a[0] = 'N'
	bplong(a[1:], i.ID)
	if in {
		a[5] = 1
	}
	a[6] = len(name)
	copy(a[7:], name)
	return d.flushimage(false)
}
*/

func (i *Image) free() error {
	if i == nil || i.Display == nil {
		return nil
	}
	// make sure no refresh events occur on this if we block in the write
	d := i.Display
	// flush pending data so we don't get error deleting the image
	d.flush(false)
	a := d.bufimage(1 + 4)
	a[0] = 'f'
	bplong(a[1:], i.id)
	if i.Screen != nil {
		w := d.Windows
		if w == i {
			d.Windows = i.next
		} else {
			for ; w != nil; w = w.next {
				if w.next == i {
					w.next = i.next
					break
				}
			}
		}
	}
	i.Display = nil // so a second free will be OK
	runtime.SetFinalizer(i, nil)
	return d.flush(i.Screen != nil)
}

// Free frees the server resources for the image.
// Images have a finalizer that calls Free automatically,
// if necessary, when the Image is garbage collected,
// but it is more efficient to be explicit.
func (i *Image) Free() error {
	if i == nil || i.Display == nil {
		// Already freed (free sets Display=nil and clears the finalizer),
		// or never associated with a display. Guard before locking so a
		// double free or a finalizer racing an explicit Free can't nil-deref.
		return nil
	}
	if i == i.Display.ScreenImage {
		panic("freeimage of ScreenImage")
	}
	i.Display.mu.Lock()
	defer i.Display.mu.Unlock()
	return i.free()
}
