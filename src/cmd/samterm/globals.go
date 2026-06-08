//TODO: rename to samterm.go

package main

import (
	"io"

	"9fans.net/plan9port/draw"
)

// Constants from samterm.h and ../sam/mesg.h.
const (
	RUNESIZE    = 4
	MAXFILES    = 256
	READBUFSIZE = 8192
	NL          = 5 // max windows (Flayers) per Text
	TBLOCKSIZE  = 512
	DATASIZE    = 8*TBLOCKSIZE + 30 // UTFmax*TBLOCKSIZE+30
	SNARFSIZE   = 32000
	VERSION     = 2

	Untagged = 65535 // ushort
)

//FIX: use frame.BACK etc instead of these; delete these
// frame color indices (frame.NCOL == 5).
const (
	BACK = iota
	HIGH
	BORD
	TEXT
	HTEXT
	NCOL
)

// Up/Down for buttons().
const (
	Up = iota
	Down
)

// A Text is a file open in the terminal: its rasp plus up to NL on-screen
// windows (Flayers).
type Text struct {
	rasp  Rasp
	nwin  int
	front int // input window
	tag   int // ushort
	lock  int
	l     [NL]Flayer // screen storage
	token int64      // opaque protocol id (C sends the Text* as a vlong)
}

// Readbuf is one half of the host/plumb double buffer.
type Readbuf struct {
	n    int
	data [READBUFSIZE]byte
}

// resource indices for the event loop (waitforio).
const (
	RHost = iota
	RKeyboard
	RMouse
	RPlumb
	RResize
	NRes
)

// Term holds all of samterm's state (the C globals).
type Term struct {
	disp *draw.Display
	font *draw.Font // default UI font (display.Font)

	// main.c
	cmd         Text
	scratch     []rune
	cursor      *draw.Cursor
	which       *Flayer
	work        *Flayer
	snarflen    int
	typestart   int
	typeend     int
	typeesc     int
	modified    int // strange lookahead for menus
	hostlock    int
	hasunlocked int
	maxtab      int
	chord       int
	autoindent  int

	// menu.c
	name     [][]byte // first byte is ' ' or '\'': modified state
	text     []*Text
	tag      []int // ushort
	nname    int
	mw       int
	menu2    draw.Menu
	menu2c   draw.Menu
	menu3    draw.Menu
	menu2str []string
	menu3str []string

	// flayer.c
	llist     []*Flayer // front to back
	lDrect    draw.Rectangle
	maincols  [NCOL]*draw.Image
	cmdcols   [NCOL]*draw.Image
	somevis   int
	someinvis int
	justvis   int

	// scroll.c
	scrtmp  *draw.Image
	scrback *draw.Image

	// icons.c
	bullseye  draw.Cursor
	deadmouse draw.Cursor
	lockarrow draw.Cursor
	darkgrey  *draw.Image

	// mesg.c
	h        Header
	indata   [DATASIZE + 1]byte
	outdata  [DATASIZE]byte
	outcount int
	hversion int
	exiting  bool

	// io.c / plan9.c
	protodebug  bool
	plumbfd     int
	got         int
	block       int
	kbdc        int
	resized     bool
	hostp       []byte // unread host bytes (current buffer slice)
	mousectl    *draw.Mousectl
	keyboardctl *draw.Keyboardctl
	hostc       chan int
	hostbuf     [2]Readbuf
	winsize     string

	// host connection (fds 3/4 normally; a pipe in tests).
	hostr  io.Reader
	hostw  io.Writer
	kpeekc int

	// protocol tokens: each Text gets an integer id that stands in for the
	// C Text* sent over the wire (see mesg.go token/Hbindname).
	texttoken map[int64]*Text
	nexttoken int64

	// rcv() state machine.
	rcvstate int
	rcvcount int
	rcvi     int
	rcverrs  int
}

// Header mirrors the C Header: type + 16-bit count + data.
type Header struct {
	htype  int
	count0 int
	count1 int
}
