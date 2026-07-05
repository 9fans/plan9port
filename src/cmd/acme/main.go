package main

import (
	"fmt"
	"io"
	"os"
	"sync"

	"9fans.net/go/plan9/client"
	"9fans.net/go/plan9/srv9p"
	"9fans.net/plan9port/draw"
	"9fans.net/plan9port/draw/frame"
	"9fans.net/plan9port/plumb"
)

// Acme holds what the C acme keeps in globals (row, display, channels, …).
// It is grown as the port proceeds; keeping it on one struct lets the editor
// run in-process so tests can spin up fresh instances and drive the -D
// protocol against the C oracle. See HACKING.md.
type Acme struct {
	seq          int // global edit sequence number
	globalincref bool
	warned       bool // printed the out-of-sequence warning
	nabort       int  // Abort builtin: warn once, then abort() (exec.c doabort)
	exiting      bool // Exit builtin requested a clean quit (exec.c xexit)
	dumping      bool // a shutdown dump is in progress (acme.c dumping)

	stdout io.Writer // -D protocol output
	stderr io.Writer // fallback for warnings before the row/display exists

	warnings []*warningMsg // pending warning text, drained into +Errors by flushwarnings

	re reEngine // regular-expression engine

	// edit-command parser stream (edit.c cmdstartp/cmdp/cmdendp)
	cmdbuf     []rune
	cmdp       int
	lastpat    []rune // most recent edit-command regexp
	patset     bool
	editAddr   Address    // ecmd.c global "addr": the address of the running command
	curtext    *Text      // edit-language current text
	nest       int        // edit-command loop nesting (g/x)
	glooping   int        // X/Y command nesting guard
	editing    int        // edit.c global "editing": Inactive/Inserting/Collecting
	collection []rune     // ecmd.c global "collection": text gathered while Collecting
	editoutlk  sync.Mutex // acme.c global editoutlk: one global-editout writer at a time

	// graphics / layout state (acme.c globals)
	disp      *draw.Display
	screen    *draw.Image
	font      *draw.Font
	row       Row
	tagcols   [frame.NCOL]*draw.Image
	textcols  [frame.NCOL]*draw.Image
	reffont   Reffont
	reffonts  [2]*Reffont
	fontnames [2]string   // var (0) and fix (1) default font paths (acme.c)
	fontcache []*Reffont  // loaded fonts (acme.c rfget cache)
	scrtmp    *draw.Image // scratch image for scrollbar drawing (scrl.c)
	winid     int         // window id counter (acme.c winid)
	maxtab    int         // tab width in '0'-widths

	trimpre          []rune // -T: prefix trimmed from displayed file names (acme.c)
	globalautoindent bool   // Indent on/off (exec.c)

	// Startup state (acme.c threadmain globals).
	dflag             bool   // -D: debug-conversation mode (empty row, scripted)
	swapscrollbuttons bool   // -r: swap scroll buttons 1 and 3
	winsize           string // -W: initial window geometry ("WxH")
	mtpt              string // -m: mount point for the acme 9P service
	home              string // $HOME
	acmeshell         string // $acmeshell (the shell external commands run under)
	cputype           string // $cputype
	objtype           string // $objtype
	objdir            []rune // cached "/$objtype/include" for #include resolution (look.c)
	wdir              string // working directory at startup (acme.c wdir)
	activecol         *Column
	activewin         *Window
	seltext           *Text
	argtext           *Text
	mousetext         *Text
	typetext          *Text
	barttext          *Text // text the scroll keys act on while a button is held (acme.c)
	bartflag          bool  // -b: route keystrokes to barttext, not the mouse text (acme.c)

	// Mouse selection state (text.c textselect): the scripted -D mouse driver
	// (driver.go m1) feeds a synthetic Mousectl, and textselect/frselect track
	// the sweep through these, exactly as the live editor does.
	mousectl    *draw.Mousectl
	keyboardctl *draw.Keyboardctl // live keyboard events (acme.c keyboardthread)
	selecttext  *Text             // frame whose sweep framescroll scrolls
	clicktext   *Text             // last single-clicked text, for double-click detection
	clickmsec   uint32
	selectq     int // anchor char position of the current sweep

	// Mouse-cursor position, mirroring libdraw's mousectl->m.xy under -D (where
	// there is no live mouse thread). a.moveto warps the devdraw cursor and
	// records the new position here; savemouse/restoremouse and winresize's
	// mouse-in-tag/body nudges read it. prevmouse/mousew back savemouse (util.c).
	mousexy   draw.Point
	prevmouse draw.Point
	mousew    *Window
	colbutton *draw.Image
	button    *draw.Image
	modbutton *draw.Image
	but2col   *draw.Image
	but3col   *draw.Image
	nullrect  draw.Rectangle
	snarfbuf  Buffer

	// 9P service (fsys.go): a.fsys is a client connection to acme's own
	// in-process 9P server, used by the -D read/write verbs. a.fsysSrv is the
	// server itself, also served on the posted namespace socket (fsyspost) so
	// external programs can mount acme in the live editor.
	fsysSrv  *srv9p.Server
	fsysconn *client.Conn
	fsys     *client.Fsys
	// event signals a blocked event-file reader (fsRead qWevent) that new events
	// were appended (xfid.c xfideventread/winevent); its L is &row.lk so the
	// reader releases the row lock while waiting. winappend broadcasts it.
	event *sync.Cond
	// logread signals a blocked /log reader that a new entry was appended
	// (logf.c xfidlogread/xfidlog); also on row.lk. xfidlog broadcasts it.
	logread *sync.Cond
	// blockEvents enables the blocking event and log reads (the live editor); the
	// single-threaded -D harness leaves it false and reads non-blockingly.
	blockEvents bool
	fids        map[string]*client.Fid // cached open fids, keyed by "name\x00mode"
	log         []string               // pending activity log entries (logf.c eventlog.ev)
	logStart    int                    // global sequence number of log[0] (logf.c eventlog.start)
	logcap      int                    // GC threshold, doubled like C's eventlog.mev
	logfids     map[*srv9p.Fid]bool    // open /log fids, for the consumed-entry GC (logf.c eventlog.f)

	// 9P mount directories (fsys.c mnt): the registry of Mntdirs and a map from
	// each live server fid to the Mntdir its attach bound (nil for the default
	// attach). srvFid keys are stable for a fid's lifetime. mntlk guards the
	// registry (mntid/mntdirs and each Mntdir's ref), matching C's mnt.lk; the
	// per-fid fidmnt map is the analogue of C's fid-local f->mntdir and stays
	// under row.lk like the other per-fid maps.
	mntlk   sync.Mutex
	mntid   int
	mntdirs []*Mntdir
	// Per-fid state (f->w, f->mntdir, f->rpart, ...) lives on each srv9p Fid's
	// Aux as a *fidAux (fsys.go), the analogue of C's fid-local Fid fields, so a
	// handler reads its own fid's state without a lock — see fidAux.
	user string // acme's user, established by the first 9P attach

	tmr timers // active/free timers (time.c)

	// External-command execution (exec.c run/runproc), async like C: the
	// subprocess runs concurrently and a reader goroutine streams its output
	// back over a channel. Edit pipes (| < >) stream over cexec and runpipe
	// drains it inline (blocking, like C's recvul(cedit)); button-2 commands
	// stream their cons output over ccons, drained by the live consthread (the
	// real editor) or by the -D "wait" verb (deterministic tests) -- mirroring
	// C's separate editout vs cons paths. nexec counts running commands.
	cexec chan execChunk
	ccons chan execChunk
	nexec int

	// The error pipe (acme.c acmeerrorinit): acmeerrorproc reads errorfd and
	// forwards to cerr (-> waitthread WErr -> +Errors); erroutfd is the write
	// end handed to a Local (newns=FALSE) command's stderr.
	errorfd  *os.File
	erroutfd *os.File

	// C thread model (acme.c threadmain): the channels wiring the ported
	// threads (mousethread/waitthread/xfidallocthread/newwindowthread/
	// debugthread). Go channels for C chancreate, goroutines for threadcreate.
	ccommand   chan *Command     // a started external command to register (waitthread WCmd)
	cwait      chan *Waitmsg     // a finished external command to reap (waitthread WWait)
	ckill      chan []rune       // Kill: name of a command to note (waitthread)
	cxfidalloc chan *Xfid        // request/return an Xfid (xfidallocthread)
	cxfidfree  chan *Xfid        // free an Xfid (xfidallocthread)
	cnewwindow chan chan *Window // New from a non-graphics proc (newwindowthread)
	cerr       chan string       // a diagnostic string to show (waitthread WErr)
	cedit      chan int          // an edit pipe finished (ecmd.c / waitthread)
	cexit      chan int          // threadmain waits on this to exit
	cwarn      chan *Mntdir      // wake mousethread to flush warnings
	command    *Command          // head of the running-command list (waitthread)
	pids       *Pid              // processes that exited before registerCommand saw them

	// Plumbing (look.c): cplumb relays messages received from the plumber's edit
	// port (plumbthread) to mousethread; plumbsendfid is the plumber's send port
	// for look3's outgoing plumb; nuntitled names plumbshow's scratch windows.
	cplumb       chan *plumb.Message
	plumbsendfid *client.Fid
	nuntitled    int
}

// warningMsg is pending warning text for a mount directory (util.c Warning).
type warningMsg struct {
	md  *Mntdir
	buf []rune
}

// warning reports a diagnostic. Like GUI acme it accumulates the text to be
// shown in a "+Errors" window (drained by flushwarnings after each command);
// before the display exists it falls back to stderr (util.c warning).
func (a *Acme) warning(md *Mntdir, format string, args ...any) {
	if a.disp == nil {
		// untested: a warning before initdraw is never produced under -D or the
		// unit tests (both open the display first); defensive fallback only.
		fmt.Fprintf(os.Stderr, format, args...)
		return
	}
	a.addwarningtext(md, []rune(fmt.Sprintf(format, args...)))
}

// addwarningtext appends r to the pending warning text for md (util.c). The C
// per-Mntdir refcount is unneeded here (GC).
func (a *Acme) addwarningtext(md *Mntdir, r []rune) {
	for _, warn := range a.warnings {
		if warn.md == md {
			warn.buf = append(warn.buf, r...)
			a.wakewarning()
			return
		}
	}
	a.warnings = append(a.warnings, &warningMsg{md: md, buf: append([]rune(nil), r...)})
	a.wakewarning()
}

// wakewarning wakes mousethread to flush pending warnings into +Errors (util.c
// addwarningtext's nbsendp(cwarn,0)), so a warning produced off the mouse thread
// (e.g. waitthread's WErr or a 9P handler) appears without waiting for the next
// mouse event. Non-blocking like C's nbsend; a no-op under -D (nil/!live cwarn),
// where the driver flushes warnings after each command instead.
func (a *Acme) wakewarning() {
	select {
	case a.cwarn <- nil:
	default:
	}
}

// flushwarnings drains pending warning text into the (created if needed)
// "+Errors" window, mirroring the per-event flush acme.c's mousethread does
// under the row lock (util.c flushwarnings). The -D driver calls it once per
// command. Every -D warning passes md==nil, i.e. the global +Errors window; the
// per-md directory GUI acme uses for command output is outside this surface.
func (a *Acme) flushwarnings() {
	for _, warn := range a.warnings {
		w := a.errorwin1(nil, nil)
		t := &w.body
		owner := w.owner
		if owner == 0 {
			w.owner = 'E'
		}
		a.wincommit(w, t)
		q0 := t.file.b.nc()
		// Append in RBUFSIZE chunks. Warning text holds no backspaces, so we
		// advance by the chunk size rather than C's inserted-rune count.
		for n := 0; n < len(warn.buf); n += RBUFSIZE {
			end := n + RBUFSIZE
			if end > len(warn.buf) {
				end = len(warn.buf)
			}
			a.textbsinsert(t, t.file.b.nc(), warn.buf[n:end], true)
		}
		a.textshow(t, q0, t.file.b.nc(), true)
		a.winsettag(t.w)
		a.textscrdraw(t)
		w.owner = owner
		w.dirty = false
	}
	a.warnings = nil
}

// editerror aborts the current edit command (edit.c editerror -> longjmp);
// ported as a panic recovered at the edit-command boundary.
func (a *Acme) editerror(format string, args ...any) {
	panic(editErr(fmt.Sprintf(format, args...)))
}

type editErr string

func main() {
	// untested: process entry / fatal-exit path not exercised in-process
	a := new(Acme)
	if err := a.threadmain(os.Args[1:]); err != nil {
		// untested: process entry / fatal-exit path not exercised in-process
		fmt.Fprintf(os.Stderr, "acme: %v\n", err)
		os.Exit(1)
	}
}
