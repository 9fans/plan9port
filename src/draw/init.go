package draw

import (
	"encoding/binary"
	"errors"
	"fmt"
	"io/fs"
	"log"
	"os"
	"runtime"
	"runtime/debug"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"

	"9fans.net/plan9port/draw/drawfcall"
)

// Display locking:
// The Exported methods of Display, being entry points for clients, lock the Display structure.
// The unexported ones do not.
// The methods for Font, Image and Screen also lock the associated display by the same rules.

// A Display represents a connection to a graphics display,
// holding all graphics resources associated with the connection,
// including in particular raster image data in use by the client program.
//
// A Display d is created by calling Init.
// Each Display corresponds to a single host window system window.
// Multiple host windows can be created by calling Init multiple times,
// although each allocated Image and Font is only valid for use with
// the Display from which it was allocated.
//
// Historically, Plan 9 graphics programs have used fixed-size
// graphics features that assume a narrow range of display densities,
// around 100 dpi: pixels (or dots) per inch.  The new field DPI
// contains the display's actual density if known, or else DefaultDPI (133).
// The Display.Scale method scales a fixed pixel count n by DPI/DefaultDPI,
// rounding appropriately. Note that the display DPI can change during
// during Display.Attach.
//
// The mouse cursor is always shown. The initial cursor is the system arrow.
// The SwitchCursor method changes the cursor image, and
// MoveCursor moves the cursor.
//
// The various graphics operations are buffered and not guaranteed to
// be made visible until a call to the Flush method.
// Various routines flush automatically, notably Mousectl.Read.
// Programs that receive directly from Mousectl.C should typically
// Flush the display explicitly before the receive.
type Display struct {
	Image       *Image
	Screen      *Screen
	ScreenImage *Image
	Windows     *Image
	DPI         int // display pixels-per-inch

	White       *Image // pre-allocated color
	Black       *Image // pre-allocated color
	Opaque      *Image // pre-allocated color
	Transparent *Image // pre-allocated color

	Font *Font // default font for UI

	mu       sync.Mutex // See comment above.
	conn     *drawfcall.Conn
	errch    chan<- error
	bufsize  int
	buf      []byte
	imageid  uint32
	qmask    *Image
	locking  bool
	dirty    bool // a draw to ScreenImage occurred since the last visible flush
	flushErr int

	firstfont *Font
	lastfont  *Font

	menu menuImages // per-display cache of the menu's colored images

	// SweepRect's cached border temporaries (see drawgetrect). Per-display,
	// like menu, so concurrent clients in one process do not share images.
	grTmp [4]*Image
	grRed *Image

	// env is the display's environment (see Init): "name=value" pairs, later
	// entries overriding earlier ones. It serves the display's environment
	// lookups in place of the process environment.
	env []string

	// noFinalizer, set by $DRAW_TEST_NOFINALIZER, disables the GC finalizer
	// that frees images automatically. C libdraw has no such automatic free,
	// so for a faithful draw-stream comparison against the C original the
	// tests set this and images are freed only by explicit Free/free calls.
	noFinalizer bool
}

// menuImages holds a display's cached images for drawing menus (see MenuHit).
// It lives in the Display, not in a global, so that several displays open in one
// process (e.g. multiple in-process terminals in a test) do not share images:
// an image allocated on one display has an id meaningless to another's devdraw.
type menuImages struct {
	txt   *Image
	back  *Image
	high  *Image
	bord  *Image
	text  *Image
	htext *Image
}

// An Image represents an image on the server, possibly visible on the display.
// It is a rectangular picture along with the methods to draw upon it.
// It is also the building block for higher-level objects such as windows and fonts.
// In particular, a window is represented as an Image; no special operators
// are needed to draw on a window.
//
// Most of the graphics methods come in two forms: a basic form, and an
// extended form that takes an extra Op to specify a Porter-Duff compositing
// operator to use. The basic forms assume the operator is SoverD, which
// suffices for the vast majority of applications.
// The extended forms are named by adding an Op suffix to the basic form's name.
type Image struct {
	// Display is the display the image belongs to.
	// All graphics operations must use images from a single display.
	Display *Display

	// R is the coordinates of the rectangle in the plane for
	// which the Image has defined pixel values.
	// It is read-only and should not be modified.
	R Rectangle

	// Clipr is the clipping rectangle: operations that read or write
	// the image will not access pixels outside clipr.
	// Frequently, clipr is the same as r, but it may differ.
	// See in particular the comment for Repl.
	// Clipr should not be modified directly; use the ReplClipr method instead.
	Clipr Rectangle

	// Pix is the pixel channel format descriptor.
	// See the package documentation for details about pixel formats.
	// It is read-only and should not be modified.
	Pix Pix

	// Depth is the number of bits per pixel in the picture.
	// It is identical to Pix.Depth() and is provided as a convenience.
	// It is read-only and should not be modified.
	Depth int

	// Repl is a boolean value specifying whether the image is tiled
	// to cover the plane when used as a source for a drawing operation.
	// If Repl is false, operations are restricted to the intersection of R and Clipr.
	// If Repl is true, R defines the tile to be replicated and Clipr defines the
	// portion of the plane covered by the tiling; in other words, R is replicated
	// to cover Clipr. In this case, R and Clipr are independent.
	//
	// For example, a replicated image with R set to (0,0)-(1,1)
	// and Clipr set to (0,0)-(100,100), with the single pixel of R set to blue,
	// behaves identically to an image with R and Clipr both set
	// to (0,0)-(100,100) and all pixels set to blue.
	// However, the first image requires far less memory and enables
	// more efficient operations.
	// Repl should not be modified directly; use the ReplClipr method instead.
	Repl bool // Whether the image is replicated (tiles the rectangle).

	Screen *Screen // If non-nil, the associated screen; this is a window.

	id   uint32
	next *Image
}

// A Screen is a collection of windows that are visible on an image.
type Screen struct {
	Display *Display // Display connected to the server.
	id      uint32
	Fill    *Image // Background image behind the windows.
}

// Refresh algorithms to execute when a window is resized or uncovered.
// RefMesg is almost always the correct one to use.
const (
	RefBackup = 0
	RefNone   = 1
	RefMesg   = 2
)

const deffontname = "*default*"

// defontfile is the descriptor for the built-in default font, mirroring C
// libdraw's _defontfile (src/libdraw/defont.h). buildFont needs only the
// height/ascent and the subfont name; the named "*default*" subfont is loaded
// lazily (getsubfont, from the embedded defontdata) on first use, so no subfont
// image is sent to the server at init time -- matching C.
const defontfile = "15 13\n0 255\t*default*\n"

// Init connects to a display server and creates a single host window.
// The error channel is unused.
//
// The font specifies the font name.
// If font is the empty string, Init uses the environment variable $font.
// If $font is not set, Init uses a built-in minimal default font.
// See the package documentation for a full discussion of font syntaxes.
//
// The label and size specify the initial window title and diemnsions.
// The size takes the form "1000x500"; the units are pixels.
//
// The env specifies the environment for the display: it is used for the
// display's environment variable lookups (such as $font and $DEVDRAW) and as
// the environment of the devdraw subprocess. A nil env means os.Environ.
// Entries are "name=value" pairs, and later entries override earlier ones of
// the same name, so appending to os.Environ overrides the process
// environment. Passing an explicit env lets tests configure concurrent
// displays differently without mutating the process environment.
//
// Unlike the Plan 9 C library's initdraw, Init does not establish any global variables.
// The C global variables display, font, and screen correspond to the
// returned value d, d.Font, and d.ScreenImage.
//
// TODO: Use the error channel.
func Init(errch chan<- error, font, label, size string, env []string) (d *Display, err error) {
	if env == nil {
		env = os.Environ()
	}
	c, err := drawfcall.New(env)
	if err != nil {
		return nil, err
	}
	d = &Display{
		conn:        c,
		errch:       errch,
		bufsize:     10000,
		env:         env,
		noFinalizer: getenv(env, "DRAW_TEST_NOFINALIZER") != "",
	}

	// Lock Display so we maintain the contract within this library.
	d.mu.Lock()
	defer d.mu.Unlock()

	d.buf = make([]byte, 0, d.bufsize+5) // 5 for final flush
	if err := c.Init(label, size); err != nil {
		c.Close()
		return nil, err
	}

	i, err := d.getimage0(nil)
	if err != nil {
		c.Close()
		return nil, err
	}

	d.Image = i
	d.White, err = d.allocImage(Rect(0, 0, 1, 1), GREY1, true, White)
	if err != nil {
		return nil, err
	}
	d.Black, err = d.allocImage(Rect(0, 0, 1, 1), GREY1, true, Black)
	if err != nil {
		return nil, err
	}
	d.Opaque = d.White
	d.Transparent = d.Black

	/*
	 * Set up default font. As in C libdraw's geninitdraw, the default font is
	 * built from a fixed descriptor (defontfile) and its "*default*" subfont is
	 * NOT loaded here -- it is sent to the server lazily on first use, via
	 * getsubfont reading the embedded defontdata.
	 */
	if font == "" {
		font = getenv(env, "font")
	}

	/*
	 * Build fonts with caches==depth of screen, for speed.
	 * If conversion were faster, we'd use 0 and save memory.
	 */
	if font == "" {
		d.Font, err = d.buildFont([]byte(defontfile), deffontname)
	} else {
		d.Font, err = d.openFont(font) // BUG: grey fonts
	}
	if err != nil {
		fmt.Fprintf(os.Stderr, "imageinit: can't open default font %s: %v\n", font, err)
		return nil, err
	}

	d.Screen, err = i.allocScreen(d.White, false)
	if err != nil {
		return nil, err
	}
	d.ScreenImage = d.Image // temporary, for d.ScreenImage.Pix
	d.ScreenImage, err = allocwindow(nil, d.Screen, i.R, 0, White)
	if err != nil {
		return nil, err
	}

	// Draw the initial white window and flush it with a single flush, matching
	// C libdraw's initdraw (src/libdraw/init.c: _allocwindow, draw, one
	// flushimage). Flushing once after allocwindow and again after the draw --
	// as the upstream 9fans.net/go/draw does -- would emit a redundant extra
	// startup frame (allocwindow already leaves the window white), making the
	// flush stream differ from the C client for an identical window.
	screen := d.ScreenImage
	screen.draw(screen.R, d.White, nil, ZP)
	if err := d.flush(true); err != nil {
		log.Fatal("draw flush: ", err)
	}

	return d, nil
}

func (d *Display) getimage0(i *Image) (*Image, error) {
	if i != nil {
		i.free()
		*i = Image{}
	}

	a := d.bufimage(2)
	a[0] = 'J'
	a[1] = 'I'
	if err := d.flush(false); err != nil {
		fmt.Fprintf(os.Stderr, "cannot read screen info: %v\n", err)
		return nil, err
	}

	info := make([]byte, 12*12)
	n, err := d.conn.ReadDraw(info)
	if err != nil {
		return nil, err
	}
	if n < len(info) {
		return nil, fmt.Errorf("short info from rddraw")
	}

	pix, _ := ParsePix(strings.TrimSpace(string(info[2*12 : 3*12])))
	if i == nil {
		i = new(Image)
	}
	*i = Image{
		Display: d,
		id:      0,
		Pix:     pix,
		Depth:   pix.Depth(),
		Repl:    atoi(info[3*12:]) > 0,
		R:       ator(info[4*12:]),
		Clipr:   ator(info[8*12:]),
	}

	a = d.bufimage(3)
	a[0] = 'q'
	a[1] = 1
	a[2] = 'd'
	d.DPI = 100
	if err := d.flush(false); err == nil {
		if n, _ := d.conn.ReadDraw(info[:12]); n == 12 {
			d.DPI = atoi(info)
		}
	}

	return i, nil
}

// Attach reattaches to a display, after a resize, updating the
// display's associated image, screen, and screen image data structures.
// The images d.Image and d.ScreenImage and the screen d.Screen
// are reallocated, so the caller must reinitialize any cached copies of
// those fields.
//
// Any open Fonts associated with the Display may be updated in
// response to a DPI change, meaning the caller should expect that
// a Font's Height may be different after calling Attach as well.
// The Font pointers themselves do not change.
func (d *Display) Attach(ref int) error {
	d.mu.Lock()
	defer d.mu.Unlock()
	if runtime.GOOS != "plan9" {
		oi := d.Image
		i, err := d.getimage0(oi)
		if err != nil {
			return err
		}
		d.Image = i
	}
	i := d.Image
	d.Screen.free()
	var err error
	d.Screen, err = i.allocScreen(d.White, false)
	if err != nil {
		return err
	}
	d.ScreenImage.free()
	d.ScreenImage, err = allocwindow(d.ScreenImage, d.Screen, i.R, ref, White)
	if err != nil {
		log.Fatal("allocwindow: ", err)
	}

	if d.HiDPI() {
		for f := d.firstfont; f != nil; f = f.next {
			loadhidpi(f)
		}
	} else {
		for f := d.firstfont; f != nil; f = f.next {
			if f.lodpi != nil && f.lodpi != f {
				swapfont(f, &f.hidpi, &f.lodpi)
			}
		}
	}

	return nil
}

// Close closes the Display.
func (d *Display) Close() error {
	if d == nil {
		return nil
	}
	d.mu.Lock()
	defer d.mu.Unlock()
	return d.conn.Close()
}

// TODO: drawerror

func (d *Display) flushBuffer() error {
	if len(d.buf) == 0 {
		return nil
	}
	_, err := d.conn.WriteDraw(d.buf)
	if errors.Is(err, fs.ErrClosed) {
		return nil
	}
	d.buf = d.buf[:0]
	if err != nil {
		if d.flushErr++; d.flushErr > 100 {
			panic("draw flush: error loop: " + err.Error())
		}
		if d.flushErr > 110 {
			log.Fatal("draw flush: error loop: " + err.Error())
		}
		fmt.Fprintf(os.Stderr, "draw flush: %v\n%s", err, debug.Stack())
		return err
	}
	d.flushErr = 0
	return nil
}

// Flush flushes pending I/O to the server, making any drawing changes visible.
func (d *Display) Flush() error {
	d.mu.Lock()
	defer d.mu.Unlock()
	return d.flush(true)
}

// FlushImage flushes pending I/O to the server. If visible, any drawing changes
// are made visible (a 'v' is sent when the screen is dirty); otherwise only the
// accumulated draw buffer is sent, with no 'v'. samterm flushes non-visibly
// while the host holds the terminal locked, so that a host update split across
// several event-loop wakeups produces a single visible flush at unlock rather
// than a nondeterministic number (matching C libdraw's flushimage(d, visible)).
func (d *Display) FlushImage(visible bool) error {
	d.mu.Lock()
	defer d.mu.Unlock()
	return d.flush(visible)
}

// noteDraw records that a draw operation targeted dst. If dst is the screen
// image, the display is dirty and the next visible flush will actually send a
// 'v'; flushes with no intervening screen draw send nothing, so the flush
// stream does not depend on how the event loop happened to wake (which varies
// with protocol pipe chunking). C libdraw does the same (see flushimage).
func (d *Display) noteDraw(dst *Image) {
	if dst == d.ScreenImage {
		d.dirty = true
	}
}

func (d *Display) flush(visible bool) error {
	if visible {
		if !d.dirty {
			// Nothing drawn to the screen since the last visible flush; the
			// flush would be a no-op, so don't send 'v'.
			return d.flushBuffer()
		}
		d.dirty = false
	}
	return d.flushInner(visible)
}

func (d *Display) flushInner(visible bool) error {
	if visible {
		if getenv(d.env, "DRAWOP_VSTACK") != "" {
			fmt.Fprintf(os.Stderr, "VFLUSH %d:\n%s\n", drawOpCount.Load()+1, debug.Stack())
		}
		d.bufsize++
		a := d.bufimage(1)
		d.bufsize--
		a[0] = 'v'
	}

	return d.flushBuffer()
}

// drawOpCount counts calls to bufimage (one per draw opcode emitted). When
// $DRAWOP_STACK is set to an op number, that op's stack trace is dumped to
// stderr; when $DRAWOP_LOG is set, every op's "number opcode" is logged (the
// opcode is read on the next call, once the caller has written it). Used
// against a devdraw drawlog to locate exactly which Go call emitted a given
// opcode while making the Go port match the C original.
var (
	drawOpCount  atomic.Int64 // incremented from finalizer goroutines too, so atomic
	drawOpTarget = -2
	drawOpLog    = -2
	drawOpPrev   []byte
	drawOpPrevN  int
)

func (d *Display) bufimage(n int) []byte {
	if d == nil || n < 0 || n > d.bufsize {
		panic("bad count in bufimage")
	}
	if drawOpTarget == -2 {
		drawOpTarget = -1
		if s := getenv(d.env, "DRAWOP_STACK"); s != "" {
			drawOpTarget, _ = strconv.Atoi(s)
		}
		drawOpLog = -1
		if getenv(d.env, "DRAWOP_LOG") != "" {
			drawOpLog = 1
		}
	}
	if drawOpPrev != nil { // report previous op's opcode, now written
		fmt.Fprintf(os.Stderr, "OP %d %c\n", drawOpPrevN, drawOpPrev[0])
		drawOpPrev = nil
	}
	count := int(drawOpCount.Add(1))
	if count == drawOpTarget {
		fmt.Fprintf(os.Stderr, "DRAWOP %d:\n%s\n", count, debug.Stack())
	}
	if len(d.buf)+n > d.bufsize {
		if err := d.flushBuffer(); err != nil {
			panic("bufimage flush: " + err.Error())
		}
	}
	i := len(d.buf)
	d.buf = d.buf[:i+n]
	r := d.buf[i:]
	if drawOpLog == 1 {
		drawOpPrev, drawOpPrevN = r, count
	}
	return r
}

// DefaultDPI is the DPI assumed when the actual display DPI is unknown.
// It is also the base DPI assumed for the Display.Scale method,
// which scales fixed-size pixel counts for higher-resolution displays.
// See the Display documentation for more information.
const DefaultDPI = 133

// Scale scales the fixed pixel count n by d.DPI / DefaultDPI,
// rounding appropriately. It can help programs that historically
// assumed fixed pixel counts (for example, a 4-pixel border)
// scale gracefully to high-resolution displays.
// See the Display documentation for more information.
func (d *Display) Scale(n int) int {
	if d == nil || d.DPI <= DefaultDPI {
		return n
	}
	return (n*d.DPI + DefaultDPI/2) / DefaultDPI
}

func atoi(b []byte) int {
	i := 0
	for i < len(b) && b[i] == ' ' {
		i++
	}
	n := 0
	for ; i < len(b) && '0' <= b[i] && b[i] <= '9'; i++ {
		n = n*10 + int(b[i]) - '0'
	}
	return n
}

func atop(b []byte) Point {
	return Pt(atoi(b), atoi(b[12:]))
}

func ator(b []byte) Rectangle {
	return Rectangle{atop(b), atop(b[2*12:])}
}

func bplong(b []byte, n uint32) {
	binary.LittleEndian.PutUint32(b, n)
}

func bpshort(b []byte, n uint16) {
	binary.LittleEndian.PutUint16(b, n)
}

func (d *Display) HiDPI() bool {
	return d.DPI >= DefaultDPI*3/2
}

func (d *Display) ScaleSize(n int) int {
	if d == nil || d.DPI <= DefaultDPI {
		return n
	}
	return (n*d.DPI + DefaultDPI/2) / DefaultDPI
}
