package main

import (
	"sync"
	"sync/atomic"

	"9fans.net/go/plan9/srv9p"
	"9fans.net/plan9port/draw"
)

// This is a faithful port of plan9port's acme (src/cmd/acme) following the
// go/cmd/sam model: the C globals become fields of *Acme, every C function a
// method or package function, so the editor runs in-process and is tested
// against the C acme's -D protocol as a live oracle. See HACKING.md.

// Undo/elog record types (dat.h).
const (
	Empty    = 0
	Null     = '-'
	Delete   = 'd'
	Insert   = 'i'
	Replace  = 'r'
	Filename = 'f'
)

// editing states (dat.h).
const (
	Inactive = iota
	Inserting
	Collecting
)

// printposn modes (edit.h).
const (
	PosnLine = iota
	PosnChars
	PosnLineChars
)

// Text.what values (dat.h).
const (
	Columntag = iota
	Rowtag
	Tag
	Body
)

const (
	// fbufalloc() guarantees room off the end of BUFSIZE; we keep the chunk
	// sizes that are load-bearing for the 9P wire protocol.
	Maxblock  = 8 * 1024
	BUFSIZE   = Maxblock + 1
	RBUFSIZE  = BUFSIZE
	EVENTSIZE = 256
)

// Range is a [q0,q1) span of rune positions.
type Range struct {
	q0 int
	q1 int
}

func runge(q0, q1 int) Range { return Range{q0, q1} }

// Runestr pairs a rune slice with its length (dat.h Runestr); in Go the
// slice carries its own length, so r alone usually suffices.
type Runestr struct {
	r []rune
}

// Reffont wraps a Font (dat.h Reffont). The C version reference-counts shared
// fonts; Go's GC makes that unnecessary, so this is just the font.
type Reffont struct {
	f *draw.Font
}

// Window is one acme window: a tag and body sharing a column (dat.h Window).
// Fields are added as the layout is ported.
// Dirlist is one entry of a directory window's listing (dat.h).
type Dirlist struct {
	r   []rune
	wid int
}

type Window struct {
	lk          sync.Mutex // per-window lock held by winlock/winunlock (dat.h Window.lk)
	tag         Text
	body        Text
	r           draw.Rectangle
	dlp         []*Dirlist // directory-window listing (wind.c)
	isdir       bool
	isscratch   bool
	filemenu    bool
	dirty       bool
	autoindent  bool
	showdel     bool
	id          int
	addr        Range
	limit       Range
	nopen       [qMax]int // open-fid counts per file qid (fsys.go)
	incl        [][]rune  // per-window include directories (Incl), most recent first
	events      []byte    // queued event records for an open event file (wind.c)
	wrselrange  Range     // where a wrsel/editout write inserts (xfid.c)
	nomark      bool
	col         *Column
	ref         atomic.Int32 // window reference count (dat.h Window.ref); winclose frees at 0
	owner       int
	maxlines    int
	putseq      int
	reffont     *Reffont
	ctlfid      *srv9p.Fid // the fid holding the exclusive ctl lock (nil = unlocked)
	ctllock     sync.Mutex // C w->ctllock: held between a ctl "lock" and "unlock"
	editoutlk   sync.Mutex // C w->editoutlk: one editout writer at a time per window
	dumpstr     string     // ctl "dump" command (window's dump command line)
	dumpdir     string     // ctl "dumpdir" command (window's dump directory)
	dumpid      int
	utflastqid  int
	utflastboff int
	utflastq    int
	tagsafe     bool
	tagexpand   bool
	taglines    int
	tagtop      draw.Rectangle
}

// Mntdir is a 9P mount directory: a command's working directory and include
// list, attached to via the decimal id in the 9P aname (dat.h Mntdir). It is
// reference-counted (one ref for the originating command, one per attach).
type Mntdir struct {
	id   int
	dir  []rune
	ref  int
	incl [][]rune
}

// Command is a running external command (dat.h Command), tracked by waitthread.
type Command struct {
	pid       int
	name      []rune
	text      string
	av        []string
	iseditcmd bool
	md        *Mntdir
	next      *Command
}

// Pid records the exit status of a process that finished before waitthread saw
// its ccommand message (dat.h Pid). Threaded onto a.pids by reapCommand (WWait),
// consumed by registerCommand (WCmd).
type Pid struct {
	pid  int
	msg  string
	next *Pid
}

// Waitmsg is a finished child's pid and exit message, delivered to cwait by run's
// reaper goroutine (a subset of libthread's Waitmsg; C's waitthread WWait).
type Waitmsg struct {
	pid int
	msg string
}

// Xfid is C's in-flight 9P request (dat.h Xfid). The Go port serves 9P through
// the srv9p library, whose handlers run inline as goroutines (fsys.go), so C's
// Xfid free-list, per-Xfid xfidctl goroutine, and the cxfidalloc/cxfidfree hand-off
// have no counterpart here (see PORT.md fsys.c/xfid.c). This type and those
// channels are vestigial and unused; kept only as a marker of where C's Xfid
// machinery lived, pending the line-by-line audit.
type Xfid struct{}

// Column is a vertical stack of windows (dat.h Column).
type Column struct {
	r    draw.Rectangle
	tag  Text
	row  *Row
	w    []*Window
	safe bool
}

func (c *Column) nw() int { return len(c.w) }

// Row is the whole acme display: a horizontal row of columns (dat.h Row).
type Row struct {
	lk  sync.Mutex // serializes mutation of row/columns/windows (C row.lk)
	r   draw.Rectangle
	tag Text
	col []*Column
}

func (r *Row) ncol() int { return len(r.col) }

// bug reports a can't-happen internal inconsistency, mirroring C error()
// on an "internal error" guard.
func bug(s string) {
	panic("acme: " + s)
}
