package main

// Faithful port of acme.c's thread model: threadmain wires up the channels and
// spawns the threads (Go goroutines), which talk over the channels (Go chans).
// This is the production startup + event loop, and the in-process live tests
// (acme_test.go) drive it directly under the devdraw memory backend. The
// synchronous -D differential driver (driver.go runD) is a separate surface used
// only by the frame-comparison tests; the live editor never uses it.

import (
	"fmt"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"syscall"

	"9fans.net/plan9port/draw"
)

// wpercol is the number of windows packed per column when opening files named on
// the command line (acme.c WPERCOL).
const wpercol = 8

// threadmain is the faithful port of acme.c threadmain: parse the flags, read
// the environment, open the display, create the channels, init the 9P service
// and the row (loading a dump or opening the named files), spawn the threads,
// then wait for cexit. args is os.Args[1:].
func (a *Acme) threadmain(args []string) error {
	a.stdout = os.Stdout
	a.stderr = os.Stderr
	a.reinit()
	a.shutdownproc() // install the termination-signal handler before anything else

	ncol := -1
	loadfile := ""

	// ARGBEGIN (acme.c): -a -b -c -f -F -l -m -r -W -D -T.
	files, err := a.parseArgs(args, &ncol, &loadfile)
	if err != nil {
		// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
		return err
	}

	a.cputype = os.Getenv("cputype")
	a.objtype = os.Getenv("objtype")
	a.home = os.Getenv("HOME")
	a.acmeshell = os.Getenv("acmeshell")
	if p := os.Getenv("tabstop"); p != "" {
		// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
		if n, err := strconv.Atoi(p); err == nil {
			// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
			a.maxtab = n
		}
	}
	if a.maxtab == 0 {
		a.maxtab = 4
	}
	if wd, err := os.Getwd(); err == nil {
		a.wdir = wd
	}

	if loadfile != "" {
		// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
		a.rowloadfonts(loadfile) // pre-scan the dump's fonts before initdraw
	}
	if a.fontnames[0] != "" {
		// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
		os.Setenv("font", a.fontnames[0]) // C: putenv("font", fontnames[0])
	}

	if a.dflag {
		// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
		a.winsize = "1024x768" // deterministic, display-independent geometry
	}
	if err := a.initdraw("acme", a.winsize); err != nil {
		// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
		return fmt.Errorf("can't open display: %v", err)
	}
	a.timerinit()

	// ccommand/cwait are created in reinit (the command channels live together).
	a.ckill = make(chan []rune)
	a.cxfidalloc = make(chan *Xfid)
	a.cxfidfree = make(chan *Xfid)
	a.cnewwindow = make(chan chan *Window)
	a.cerr = make(chan string)
	a.cedit = make(chan int)
	if a.cexit == nil {
		// an in-process test runner may pre-create cexit so it can unwind
		// threadmain deterministically; the live editor creates it here.
		// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
		a.cexit = make(chan int)
	}
	a.cwarn = make(chan *Mntdir, 1)

	a.mousectl = a.disp.InitMouse()
	a.keyboardctl = a.disp.InitKeyboard()
	a.startplumbing() // connect to the plumber (look.c)
	a.fsysinit()
	a.blockEvents = true // live editor: event reads block (xfid.c xfideventread)
	if err := a.fsyspost(); err != nil {
		// Not fatal: acme still runs, just unmountable by external programs.
		// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
		fmt.Fprintf(os.Stderr, "acme: can't post 9P service: %v\n", err)
	}

	if a.dflag {
		// debug-conversation mode: start with an empty row; the script builds
		// columns and windows via newcol/win.
		// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
		a.rowinit(&a.row, a.screen.Clipr)
	} else if loadfile == "" || !a.rowload(&a.row, loadfile, true) {
		a.rowinit(&a.row, a.screen.Clipr)
		if ncol < 0 {
			if len(files) == 0 {
				ncol = 2
			} else {
				ncol = (len(files) + wpercol - 1) / wpercol
				if ncol < 2 {
					ncol = 2
				}
			}
		}
		if ncol == 0 {
			// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
			ncol = 2
		}
		for i := 0; i < ncol; i++ {
			a.rowadd(&a.row, nil, -1)
		}
		c := a.row.col[a.row.ncol()-1]
		if len(files) == 0 {
			a.readfile(c, a.wdir)
		} else {
			for i, f := range files {
				base := f
				if j := strings.LastIndexByte(f, '/'); j >= 0 {
					// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
					base = f[j:]
				}
				if base == "/guide" || i/wpercol >= a.row.ncol() {
					// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
					a.readfile(c, f)
				} else {
					a.readfile(a.row.col[i/wpercol], f)
				}
			}
		}
	}
	a.disp.FlushImage(true)

	if a.dflag {
		// the scripted driver replaces the keyboard thread but keeps the real
		// mousethread (debug.c feeds mouse events through mousectl.c).
		// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
		go a.debugthread(os.Stdin)
		go a.mousethread()
	} else {
		go a.keyboardthread()
		go a.mousethread()
		go a.consthread() // apply button-2 command output to +Errors
	}
	go a.waitthread()
	go a.xfidallocthread()
	go a.newwindowthread()
	a.acmeerrorinit() // error pipe -> cerr -> waitthread WErr -> +Errors
	<-a.cexit
	a.killprocs()
	return nil
}

// parseArgs parses the acme command line (acme.c threadmain ARGBEGIN). It sets
// the relevant Acme fields and returns the remaining file arguments.
func (a *Acme) parseArgs(args []string, ncol *int, loadfile *string) ([]string, error) {
	usage := func() error {
		return fmt.Errorf("usage: acme -a -b -c ncol -f fontname -F fixedwidthfontname -l loadfile -m mtpt -r -W winsize [file...]")
	}
	i := 0
	for ; i < len(args); i++ {
		arg := args[i]
		if len(arg) < 2 || arg[0] != '-' {
			break
		}
		if arg == "--" {
			i++
			break
		}
		// Each flag is its own token here (acme is normally invoked that way);
		// flags with values take the rest of the token or the next token.
		val := func() (string, bool) {
			if len(arg) > 2 {
				return arg[2:], true
			}
			if i+1 < len(args) {
				i++
				return args[i], true
			}
			return "", false
		}
		switch arg[1] {
		case 'D':
			a.dflag = true
		case 'a':
			a.globalautoindent = true
		case 'b':
			a.bartflag = true
		case 'r':
			a.swapscrollbuttons = true
		case 'T':
			v, ok := val()
			if !ok {
				return nil, usage()
			}
			a.trimpre = []rune(v)
		case 'c':
			v, ok := val()
			if !ok {
				return nil, usage()
			}
			n, err := strconv.Atoi(v)
			if err != nil || n <= 0 {
				return nil, usage()
			}
			*ncol = n
		case 'f':
			v, ok := val()
			if !ok {
				// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
				return nil, usage()
			}
			a.fontnames[0] = v // threadmain puts this in $font before initdraw
		case 'F':
			v, ok := val()
			if !ok {
				// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
				return nil, usage()
			}
			a.fontnames[1] = v
		case 'l':
			v, ok := val()
			if !ok {
				return nil, usage()
			}
			*loadfile = v
		case 'm':
			v, ok := val()
			if !ok {
				// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
				return nil, usage()
			}
			a.mtpt = v
		case 'W':
			v, ok := val()
			if !ok {
				return nil, usage()
			}
			a.winsize = v
		default:
			return nil, usage()
		}
	}
	return args[i:], nil
}

// readfile opens the file named s into a new window in column c (acme.c
// readfile), used at startup for each command-line argument (or the working
// directory when none are given).
func (a *Acme) readfile(c *Column, s string) {
	w := a.coladd(c, nil, nil, -1)
	var rb []rune
	if s == "" || s[0] != '/' {
		rb = []rune(a.wdir + "/" + s)
	} else {
		rb = []rune(s)
	}
	rs := cleanrname(rb)
	a.winsetname(w, rs)
	a.textload(&w.body, 0, s, true)
	w.body.file.mod = false
	w.dirty = false
	a.winsettag(w)
	a.winresize(w, w.r, false, true)
	a.textscrdraw(&w.body)
	nc := w.tag.file.b.nc()
	a.textsetselect(&w.tag, nc, nc)
	a.xfidlog(w, "new")
}

// shutdownproc installs the termination-signal handler (acme.c threadnotify +
// shutdown): SIGHUP/SIGINT/SIGTERM/SIGQUIT trigger a session dump and exit, so
// the windows survive a logout or crash; the job-control / closed-pipe signals
// (C's ignotes) are ignored. A clean quit goes through Exit (xexit), not here.
// Registered first thing in threadmain so a signal arriving during startup is
// caught rather than taking the default (terminate); rowdump no-ops if the row
// is not built yet.
func (a *Acme) shutdownproc() {
	// Register the handlers synchronously (the caller does not use `go`), so the
	// notification is installed before threadmain proceeds to draw -- otherwise a
	// signal arriving during startup could take the default (terminate) action.
	signal.Ignore(syscall.SIGPIPE, syscall.SIGTSTP, syscall.SIGTTIN, syscall.SIGTTOU)
	c := make(chan os.Signal, 1)
	signal.Notify(c, syscall.SIGHUP, syscall.SIGINT, syscall.SIGTERM, syscall.SIGQUIT)
	go func() {
		sig := <-c
		a.shutdown(sig)
	}()
}

// shutdown reaps children, dumps the session, and exits (acme.c shutdown). C
// skips the dump for a clean "kill"/"exit" note; the catchable signals handled
// here are all the unexpected kind, so the dump always runs (guarded by the
// dumping flag against re-entry).
func (a *Acme) shutdown(sig os.Signal) {
	// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
	if !a.dumping {
		// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
		a.dumping = true
		a.row.lk.Lock()
		a.rowdump(&a.row, "")
		a.row.lk.Unlock()
	}
	// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
	a.killprocs()
	fmt.Fprintf(os.Stderr, "acme: %v\n", sig)
	os.Exit(1)
}

// killprocs shuts the 9P service and posts a hangup to every running command on
// exit (acme.c killprocs).
func (a *Acme) killprocs() {
	a.fsysclose()
	for c := a.command; c != nil; c = c.next {
		// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
		if c.pid > 0 {
			// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
			syscall.Kill(-c.pid, syscall.SIGHUP) // C: postnote(PNGROUP, c.pid, "hangup")
		}
	}
}

// acmeerrorproc forwards acme's error-pipe output to the +Errors window (acme.c
// acmeerrorproc): read chunks from errorfd and send each to cerr, where
// waitthread's WErr case shows it. The pipe's write end (erroutfd) is a Local
// command's stderr and acme's own internal-error output.
func (a *Acme) acmeerrorproc() {
	buf := make([]byte, 8192)
	for {
		n, err := a.errorfd.Read(buf)
		if n > 0 {
			a.cerr <- string(buf[:n])
		}
		if err != nil {
			// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
			return
		}
	}
}

// acmeerrorinit creates the error pipe and starts acmeerrorproc (acme.c
// acmeerrorinit). A Plan 9 pipe is bidirectional; the Go port uses a one-way
// os.Pipe: erroutfd is the write end (children/internal writers), errorfd the
// read end (acmeerrorproc). Both are close-on-exec so a spawned command does
// not inherit them except where dup'd in deliberately.
func (a *Acme) acmeerrorinit() {
	r, w, err := os.Pipe()
	if err != nil {
		// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
		a.fatalf("can't create pipe: %v", err)
	}
	a.erroutfd = w
	a.errorfd = r
	go a.acmeerrorproc()
}

// keyboardthread delivers live keystrokes to the text under the mouse (acme.c
// keyboardthread). It selects over the keyboard channel and a 500ms tag-commit
// timer: typing into a tag arms the timer, and when it fires the tag is
// committed (so a half-typed tag eventually settles). A burst of pending keys
// is drained without an intervening flush, then the display is flushed once,
// matching C's nbrecv loop. typetext tracks the last-typed text for the timer.
func (a *Acme) keyboardthread() {
	var timer *Timer
	a.typetext = nil
	for {
		var timerc <-chan int // nil channel blocks forever, i.e. timer disabled
		if timer != nil {
			timerc = timer.c
		}
		select {
		case <-timerc:
			a.timerstop(timer)
			t := a.typetext
			if t != nil && t.what == Tag {
				a.winlock(t.w, 'K')
				a.wincommit(t.w, t)
				a.winunlock(t.w)
				a.disp.FlushImage(true)
			}
			timer = nil
		case r := <-a.keyboardctl.C:
		casekeyboard:
			// Snapshot the mouse position under row.lk (mousethread writes
			// a.mousectl.Mouse there); rowtype re-takes row.lk to hit-test it.
			a.row.lk.Lock()
			mousep := a.mousectl.Mouse.Point
			a.row.lk.Unlock()
			a.typetext = a.rowtype(&a.row, r, mousep)
			t := a.typetext
			// scrolling keys don't change the active column (acme.c)
			if t != nil && t.col != nil && !(r == Kdown || r == Kleft || r == Kright) {
				a.activecol = t.col
			}
			if t != nil && t.w != nil {
				t.w.body.file.curtext = &t.w.body
			}
			if timer != nil {
				a.timercancel(timer)
			}
			if t != nil && t.what == Tag {
				timer = a.timerstart(500)
			} else {
				// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
				timer = nil
			}
			// drain another already-queued keystroke before flushing (C nbrecv)
			select {
			case r = <-a.keyboardctl.C:
				goto casekeyboard
			default:
			}
			a.disp.FlushImage(true)
		}
	}
}

// mousethread is the main event loop (acme.c mousethread). It selects on the
// mouse channel, the resize channel, cwarn (a wakeup to flush pending warnings)
// and (eventually) the plumb channel. At the top of each iteration it drains
// warnings into the +Errors window and flushes the display, exactly as C does.
// MMouse dispatch is delegated to mousedispatch (mouse.go), which is the
// already-ported MMouse body.
func (a *Acme) mousethread() {
	for {
		a.row.lk.Lock()
		a.flushwarnings()
		a.row.lk.Unlock()
		a.disp.FlushImage(true)
		select {
		case <-a.mousectl.Resize:
			// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
			if err := a.disp.Attach(draw.RefNone); err != nil {
				// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
				a.fatalf("attach to window: %v", err)
			}
			// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
			a.screen.Draw(a.screen.R, a.disp.White, nil, draw.ZP)
			a.iconinit()
			a.scrlresize()
			a.rowresize(&a.row, a.screen.Clipr)
		case <-a.cwarn:
			// no-op: just woke us so the next iteration's flushwarnings runs
			// (acme.c mousethread MWarnings; covered by TestCwarnFlush)
		case m := <-a.cplumb:
			// a plumb message arrived (acme.c mousethread MPlumb): open the file
			// (showfile) or show the data in a new window (showdata).
			// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
			if m.Type == "text" {
				// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
				a.row.lk.Lock()
				act := m.LookupAttr("action")
				if act == "" || act == "showfile" {
					// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
					a.plumblook(m)
					// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
				} else if act == "showdata" {
					// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
					a.plumbshow(m)
				}
				// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
				a.row.lk.Unlock()
			}
		case m := <-a.mousectl.C:
			// Update the shared mouse state under row.lk: keyboardthread reads
			// a.mousectl.Mouse.Point (to route a keystroke to the text under the
			// mouse) also under row.lk. C reads mouse->xy unlocked -- safe under
			// libthread's cooperative scheduling, a data race with real goroutines.
			a.row.lk.Lock()
			a.mousectl.Mouse = m
			a.mousedispatch(m)
			a.row.lk.Unlock()
		}
	}
}

// consthread applies button-2 command output to the +Errors window in the live
// editor: it drains ccons (the cons stream from run's reader goroutine) and
// applies each chunk under row.lk, flushing after each. In C this work is done
// by the xfid procs serving the command's cons/wrsel writes, woken through the
// event loop; the Go port routes it through ccons + consToErrors. Under -D there
// is no consthread -- the wait verb drains ccons deterministically instead.
func (a *Acme) consthread() {
	for ch := range a.ccons {
		a.row.lk.Lock()
		a.consToErrors(ch.data)
		a.disp.FlushImage(true)
		a.row.lk.Unlock()
	}
}

// waitthread owns the external-command lifecycle, matching acme.c waitthread's
// alt over WCmd/WWait/WErr/WKill: a started button-2 command is registered and
// its name shown in the row tag (ccommand/WCmd); when it exits, run's goroutine
// (which holds the os/exec handle and does the c.Wait reap) reports it here to be
// removed from the list and the tag (cwait/WWait); cerr shows a diagnostic
// (WErr); ckill notes a command for Kill (WKill). The cons OUTPUT travels
// separately over ccons (consthread), as C's command writes its cons via xfid.
func (a *Acme) waitthread() {
	for {
		select {
		case c := <-a.ccommand:
			a.row.lk.Lock()
			a.registerCommand(c)
			a.disp.FlushImage(true)
			a.row.lk.Unlock()
		case w := <-a.cwait:
			a.row.lk.Lock()
			a.reapCommand(w)
			a.disp.FlushImage(true)
			a.row.lk.Unlock()
		case err := <-a.cerr:
			// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
			a.row.lk.Lock()
			a.warning(nil, "%s", err)
			a.disp.FlushImage(true)
			a.row.lk.Unlock()
		case cmd := <-a.ckill:
			a.row.lk.Lock()
			a.killcommand(cmd)
			a.row.lk.Unlock()
		}
	}
}

// killcommand posts a kill to every running command whose name matches cmd
// (acme.c waitthread WKill). C sends the "kill" note to the process group
// (PNGROUP); the Go port signals the command's process group (set up via
// Setpgid in run) with SIGTERM. Must be called with row.lk held, since the
// command list is also mutated by the cons drain under that lock.
func (a *Acme) killcommand(cmd []rune) {
	found := false
	for c := a.command; c != nil; c = c.next {
		// c.name carries a trailing blank (for waittask); compare without it.
		name := c.name
		if len(name) > 0 && name[len(name)-1] == ' ' {
			name = name[:len(name)-1]
		}
		if runeEq(name, cmd) {
			if c.pid > 0 {
				if err := syscall.Kill(-c.pid, syscall.SIGTERM); err != nil {
					// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
					a.warning(nil, "kill %s: %s\n", string(cmd), errstr(err))
				}
			}
			found = true
		}
	}
	if !found {
		// untested: live startup/thread-loop branch not reached by the in-process live scripts (error, resize, plumb, -D, and -l dump paths)
		a.warning(nil, "Kill: no process %s\n", string(cmd))
	}
}

// xfidallocthread is C's Xfid allocator, spawning an xfidctl goroutine per Xfid
// (acme.c xfidallocthread). The Go port has no counterpart: the srv9p library
// owns request dispatch and its handlers run inline (fsys.go), so there is no
// Xfid free-list to serve. The method is kept (and started by threadmain) only
// to mirror C's thread set; its body is intentionally empty. See PORT.md xfid.c.
func (a *Acme) xfidallocthread() {
}

// newwindowthread is C's cnewwindow handshake, by which fsysproc (a separate
// proc that cannot draw) asks the main proc to open a window for a client walk
// to /new (acme.c newwindowthread). The Go port needs no handshake: srv9p's
// walk handler runs as a goroutine and creates the window inline under row.lk
// (fsWalk -> walkstep "new" -> makenewwindow), which is exactly what C's
// newwindowthread does. Routing through cnewwindow would in fact deadlock here,
// since fsWalk already holds row.lk while this thread re-locks it. The method is
// kept (and started by threadmain) to mirror C's thread set; nothing sends on
// cnewwindow, so the body never runs. See PORT.md acme.c.
func (a *Acme) newwindowthread() {
	for ch := range a.cnewwindow {
		// untested: unreachable -- /new is handled inline in fsWalk, so nothing
		// ever sends on cnewwindow (see the doc comment above).
		a.row.lk.Lock()
		c := a.row.col[len(a.row.col)-1]
		w := a.coladd(c, nil, nil, -1)
		a.row.lk.Unlock()
		ch <- w
	}
}
