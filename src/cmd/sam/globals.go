// Package main is a faithful Go port of the C sam command engine
// (plan9port src/cmd/sam). See HACKING.md for the C-to-Go file mapping
// and design notes.
package main

import (
	"bufio"
	"io"
)

// Constants from sam.h.
const (
	BLOCKSIZE = 2048
	NSUBEXP   = 10
	INFINITY  = 0x7FFFFFFF
	STRSIZE   = 2 * BLOCKSIZE

	Maxblock = 8 * 1024
	BUFSIZE  = Maxblock
	RBUFSIZE = BUFSIZE / 4 // sizeof(Rune) == 4

	Runemax  = 0x10FFFF
	Runeself = 0x80
	UTFmax   = 4
)

// State of a file in the menu (sam.h enum State).
const (
	Clean  = ' '
	Dirty  = '\''
	Unread = '-'
)

// Undo record types (sam.h enum).
const (
	Null     = '-'
	Delete   = 'd'
	Insert   = 'i'
	Filename = 'f'
	Dot      = 'D'
	Mark     = 'm'
)

type (
	Posn = int // file position or address
	Mod  = int // modification number (C ushort)
	Rune = rune
)

// Range is a span of text (sam.h struct Range).
type Range struct {
	p1, p2 Posn
}

// Rangeset holds a match plus its subexpression matches (sam.h struct Rangeset).
type Rangeset struct {
	p [NSUBEXP]Range
}

// Inst is one instruction of a compiled regular expression (regexp.c).
// In the C struct, left and next share a union (the closure construction in
// evaluntil relies on it: a STAR's OR gets its fall-through ".left" set later
// when a following operand assigns ".next"). So they are one field here.
// subid/class/right share the other C union but are never used together on a
// single instruction, so they stay separate.
type Inst struct {
	typ   int // < OPERATOR ==> literal, otherwise action
	subid int
	class int
	right *Inst
	lnext *Inst // C union of left and next
}

// Ilist is one thread of the regexp NFA (regexp.c).
type Ilist struct {
	inst   *Inst // instruction of the thread
	se     Rangeset
	startp Posn // first char of match
}

// Sam holds all of sam's state. In C these are global variables; gathering
// them here lets the Go sam run in-process so each test gets a fresh Sam.
type Sam struct {
	// I/O. These replace fds 0/1/2 (and the protocol connection for -R).
	in  io.Reader
	out io.Writer
	err io.Writer

	// flags / mode (sam.c)
	downloaded bool
	dflag      bool
	Rflag      bool
	quitok     bool

	// samterm launch (io.c bootterm). termprog is the samterm binary (-t),
	// default "samterm"; termargs are options passed through to it (-a, -W size).
	// Set during argument parsing, used by startup.
	termprog string
	termargs []string
	// compat makes error text match the C sam byte-for-byte (capitalized %r
	// strings). Off in ordinary use (Go's lowercase errors read better and
	// match Plan 9); the tests turn it on to compare against the C oracle.
	compat     bool
	panicking  int
	rescuing   int
	bpipeok    bool
	termlocked int

	// core editor state (sam.c). The C global fd `io` becomes ioR/ioW/ioC.
	genbuf   [BLOCKSIZE]Rune
	ioR      io.Reader
	ioW      io.Writer
	ioC      io.Closer
	inbr     *bufio.Reader // buffered command input (non-downloaded)
	genstr   String
	rhs      String
	curwd    String
	cmdstr   String
	genc     string
	curfile  *File
	flist    *File
	cmd      *File
	lastfile *File
	seq      int

	// command list / input buffer (cmd.c)
	line     []Rune // pending input line (runes), consumed via linep
	linep    int
	eof      bool
	termline []Rune
	terminp  int
	termoutp int

	// addressing (address.c)
	addr    Address
	lastpat String
	patset  bool
	menu    *File

	// regexp machine (regexp.c)
	sel         Rangeset
	lastregexp  String
	program     []Inst
	progp       int
	startinst   *Inst // first inst. of program; might not be program[0]
	bstartinst  *Inst // same for backwards machine
	relist      [2][]Ilist
	sempty      Rangeset
	andstack    []regNode
	andp        int
	atorstack   []int
	atorp       int
	subidstack  []int
	subidp      int
	lastwasand  bool // last token was operand
	cursubid    int
	backwards   bool
	nbra        int
	exprp       []Rune // source expression
	exprpos     int    // index of next character in exprp
	nclass      int    // number of character classes active
	class       [][]Rune
	negateclass bool

	// undo merge state (file.c)
	merge Merge

	// file menu (multi.c)
	file     []*File
	tempfile []*File
	tag      int

	// executor (xec.c)
	glooping int
	nest     int

	// shell (shell.c)
	errfile  string
	plan9cmd String
	plan9buf Buffer

	// wire protocol (mesg.c)
	hdr         header
	indata      []byte
	outdata     []byte
	inp         int
	outp        int
	outmsg      int
	cmdpt       Posn
	cmdptadv    Posn
	snarfbuf    Buffer
	waitack     bool
	outbuffered bool
	tversion    int
	// rcv state (mesg.c rcvchar/rcv static locals)
	rcvbuf   [64]byte
	rcvn     int
	rcvi     int
	rcvstate int
	rcvcount int
	rcvidx   int

	// rasp (rasp.c)
	growpos   Posn
	grown     Posn
	shrinkpos Posn
	shrunk    Posn

	// sys.c
	inerror bool
}

// regNode is the regexp parser's operand stack node (regexp.c struct Node).
type regNode struct {
	first *Inst
	last  *Inst
}

// header is the wire-protocol message header (mesg.h struct Header).
type header struct {
	typ    byte
	count0 byte
	count1 byte
}
