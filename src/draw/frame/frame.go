// Package frame supports frames of editable text,
// such as in the Plan 9 text editors sam and acme.
package frame

import (
	"9fans.net/plan9port/draw"
)

// A Frame is a frame of editable text in a single font on a raster display.
// It may hold any character (even NUL, in contrast to the C original).
// Long lines are folded and tabs are set at fixed intervals.
//
// P0 and p1 may be changed by the application provided the
// selection routines are called afterwards to maintain a consistent display.
// MaxTab determines the size of tab stops.
// The Init method sets MaxTab to 8 times the width of a 0 (zero) character
// in the font; it may be changed before any text is added to
// the frame.
// The other elements of the structure are maintained by the library
// and should not be modified directly.
//
// The text within frames is not directly addressable; instead
// frames are designed to work alongside another structure that
// holds the text.  The typical application is to display a
// section of a longer document such as a text file or terminal
// session.  Usually the program will keep its own copy of the
// text in the window (probably as an array of Runes) and pass
// components of this text to the frame routines to display the
// visible portion.  Only the text that is visible is held by
// the Frame; the application must check maxlines, nlines, and
// lastlinefull to determine, for example, whether new text
// needs to be appended at the end of the Frame after calling
// frdelete.
//
// There are no routines in the library to allocate Frames;
// instead the interface assumes that Frames will be components
// of larger structures and initialized using the Init method.
//
// Note that unlike most Go types, Frames must be explicitly
// freed using the Clear method, in order to release the
// associated images.
//
// Programs that wish to manage the selection themselves have
// several routines to help.  They involve the maintenance of
// the `tick', the vertical line indicating a null selection
// between characters, and the colored region representing a
// non-null selection. See the Tick, Drawsel, Drawsel0, and SelectPaint methods.
type Frame struct {
	P0, P1 int // selection
	MaxTab int // max size of tab, in pixels

	// Read-only to clients.
	Font         *draw.Font        // of chars in the frame
	Display      *draw.Display     // on which frame appears
	B            *draw.Image       // on which frame appears
	Cols         [NCOL]*draw.Image // text and background colors
	R            draw.Rectangle    // in which text appears
	Entire       draw.Rectangle    // of full frame
	Scroll       func(*Frame, int) // scroll function provided by application
	NumChars     int               // # runes in frame
	NumLines     int               // # lines with text
	MaxLines     int               // total # lines in frame
	LastLineFull bool              // last line fills frame
	Ticked       bool              // flag: is tick onscreen?
	NoRedraw     int               // don't draw on the screen

	box       []box
	tick      *draw.Image // typing tick
	tickback  *draw.Image // saved image under tick
	tickscale int         // tick scaling factor
	modified  bool        // changed since frselect
}

// BACK, HIGH, BORD, TEXT, and HTEXT are indices into Frame.Cols.
const (
	BACK  = iota // background color
	HIGH         // highlight background color
	BORD         // border color
	TEXT         // text color
	HTEXT        // highlight text color
	NCOL
)

const _FRTICKW = 3

func (b *box) NRUNE() int {
	if b.nrune < 0 {
		return 1
	}
	return b.nrune
}

//FIX: delete
func (b *box) NBYTE() int {
	// untested: the slice-based port reads len(b.bytes) where C used the
	// NBYTE macro (frbox.c, frstr.c), so this mirror method is never called.
	return len(b.bytes)
}
