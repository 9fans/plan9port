package main

import (
	"bufio"
	"errors"
	"flag"
	"fmt"
	"io"
	"os"
	"strconv"
	"syscall"

	p9 "9fans.net/go/plan9"
	"9fans.net/go/plan9/client"
	"9fans.net/plan9port/draw"
	"9fans.net/plan9port/plumb"
	"rsc.io/getopt"
)

func usage() {
	// untested: usage exits the process, so only the real binary reaches it (the
	// tests call getscreen with valid flags).
	fmt.Fprintf(os.Stderr, "usage: samterm -a -W winsize\n")
	os.Exit(1)
}

// getenv returns the value of the environment variable name in t.env
// (later entries override earlier ones of the same name).
func (t *Term) getenv(name string) string {
	v := ""
	for _, kv := range t.env {
		if len(kv) > len(name) && kv[len(name)] == '=' && kv[:len(name)] == name {
			v = kv[len(name)+1:]
		}
	}
	return v
}

func (t *Term) getscreen(args []string) error {
	fs := getopt.NewFlagSet("samterm", flag.ContinueOnError)
	fs.SetOutput(io.Discard)
	fs.BoolVar(&t.autoindent, "a", false, "enable autoindent")
	fs.StringVar(&t.winsize, "W", "", "window size")
	if err := fs.Parse(args); err != nil {
		// untested: see usage
		usage()
	}
	disp, err := draw.Init(nil, "", "sam", t.winsize, t.env)
	if err != nil {
		// untested: getscreen runs only from main() (which the tests bypass via
		// NewTerm); the in-process tests call draw.Init themselves.
		return err
	}
	t.display = disp
	t.screen = disp.ScreenImage
	t.font = disp.Font
	if ts := t.getenv("tabstop"); ts != "" {
		if n, err := strconv.Atoi(ts); err == nil {
			t.maxtab = n
		}
	}
	t.screen.Draw(t.screen.Clipr, disp.White, nil, draw.ZP)
	return nil
}

func (t *Term) screensize() (w, h int, ok bool) {
	return 0, 0, false
}

func (t *Term) snarfswap(fromsam []byte) (tosam []byte) {
	buf := make([]byte, SNARFSIZE)
	got, size, err := t.display.ReadSnarf(buf)
	if err != nil {
		// untested: ReadSnarf fails only on a dead display RPC (an empty snarf
		// reads back as zero bytes, not an error).
		return nil
	}
	if size > 0 {
		if got > size {
			// untested: defensive clamp mirroring C; ReadSnarf never reports more
			// bytes copied than the total snarf size.
			got = size
		}
		tosam = append([]byte(nil), buf[:got]...)
	}
	t.display.WriteSnarf(fromsam)
	return tosam
}

func (t *Term) dumperrmsg(count, typ, count0, c int) {
	fmt.Fprintf(os.Stderr, "samterm: host mesg: count %d %x %x %x %s...ignored\n",
		count, typ, count0, c, t.rcvstring())
}

// removeextern unlinks the extern FIFO. C registers it with atexit; Go calls it
// from main() on the normal exit path (and from extstart's own error paths). It
// is a no-op once exname has been cleared.
func (t *Term) removeextern() {
	if t.exname != "" {
		os.Remove(t.exname)
	}
}

// extproc reads raw "B filename\n" requests from the extern FIFO into a half of
// the plumbbuf double buffer and notifies the event loop (waitforio's RPlumb
// case) with the buffer index, exactly like plumbproc but without the plumb
// formatting (the FIFO bytes are already in command form). A read error is not
// fatal: the goroutine just exits, as C's extproc does (threadexits). Faithful
// to plan9.c extproc.
func (t *Term) extproc(f *os.File) {
	i := 0
	for {
		i = 1 - i // toggle
		n, err := f.Read(t.plumbbuf[i].data[:])
		if n <= 0 || err != nil {
			fmt.Fprintf(os.Stderr, "samterm: extern read error: %v\n", err)
			return // not a fatal error
		}
		t.plumbbuf[i].n = n
		t.plumbc <- i
	}
}

// extstart sets up the legacy "extern" file-open channel: a named FIFO at
// /tmp/.sam.$USER[.$DISPLAY] that anything (notably sam's own button-2 "B"
// command, or older tooling) can write a "B filename\n" request into. It is the
// pre-plumber mechanism, used only when no plumber is reachable (initio calls it
// when plumbstart fails). Faithful to plan9.c extstart.
//
// Differences from C, none observable: C registers atexit(removeextern); Go has
// no atexit, so main() calls t.removeextern() on the normal exit path. C clears
// O_NONBLOCK on the read fd because its extproc issues a blocking read(2)
// directly; here extproc reads through an *os.File, so the runtime poller
// provides the blocking, and the lingering writer (opened below, never closed)
// is what keeps reads from ever seeing EOF.
func (t *Term) extstart() {
	user := os.Getenv("USER")
	if user == "" {
		return
	}
	if disp := os.Getenv("DISPLAY"); disp != "" {
		t.exname = fmt.Sprintf("/tmp/.sam.%s.%s", user, disp)
	} else {
		t.exname = fmt.Sprintf("/tmp/.sam.%s", user)
	}

	if err := syscall.Mkfifo(t.exname, 0600); err != nil {
		// Already exists: reuse it if it is a FIFO, else remove and recreate
		// (matches C's errno==EEXIST + stat + S_ISFIFO check).
		var st syscall.Stat_t
		if err != syscall.EEXIST || syscall.Stat(t.exname, &st) != nil {
			t.exname = ""
			return
		}
		if uint32(st.Mode)&syscall.S_IFMT != syscall.S_IFIFO {
			t.removeextern()
			if syscall.Mkfifo(t.exname, 0600) != nil {
				t.exname = ""
				return
			}
		}
	}

	// Open the FIFO for reading. O_NONBLOCK so the open does not block waiting
	// for a writer to appear.
	rfd, err := syscall.Open(t.exname, syscall.O_RDONLY|syscall.O_NONBLOCK, 0)
	if err != nil {
		t.removeextern()
		t.exname = ""
		return
	}
	// Provide ourselves as a lingering writer (intentionally never closed) so
	// the reader never sees end-of-file when the last client closes. If that
	// open fails, give up on the extern mechanism.
	if _, err := syscall.Open(t.exname, syscall.O_WRONLY, 0); err != nil {
		syscall.Close(rfd)
		t.removeextern()
		t.exname = ""
		return
	}

	t.plumbc = make(chan int)
	go t.extproc(os.NewFile(uintptr(rfd), t.exname))
}

// plumbformat renders a "showfile" plumb message into plumbbuf[i] as the sam
// command "B <data>\n<addr>\n" (typed into the command window to open the file).
// It returns false for messages it can't handle (empty, too large, or a
// non-showfile action), matching C's plumbformat.
func (t *Term) plumbformat(m *plumb.Message, i int) bool {
	data := t.plumbbuf[i].data[:]
	n := len(m.Data)
	if n == 0 || 2+n+2 >= READBUFSIZE {
		return false
	}
	if act := m.LookupAttr("action"); act != "" && act != "showfile" {
		return false // can't handle other cases yet
	}
	addr := m.LookupAttr("addr")
	copy(data[0:], "B ") // we know there's enough room for this
	copy(data[2:], m.Data)
	n += 2
	if data[n-1] != '\n' {
		data[n] = '\n'
		n++
	}
	if addr != "" {
		if s := addr + "\n"; n+len(s) <= READBUFSIZE {
			n += copy(data[n:], s)
		}
	}
	t.plumbbuf[i].n = n
	return true
}

// plumbproc reads plumb messages from the edit port, formats each into one half
// of the plumbbuf double buffer, and notifies the event loop (waitforio's RPlumb
// case) with the buffer index. A read error is not fatal: the goroutine just
// exits, as C's plumbproc does (threadexits).
func (t *Term) plumbproc(fid *client.Fid) {
	r := bufio.NewReader(fid)
	i := 0
	for {
		var m plumb.Message
		if err := m.Recv(r); err != nil {
			fmt.Fprintf(os.Stderr, "samterm: plumb read error: %v\n", err)
			return
		}
		if t.plumbformat(&m, i) {
			t.plumbc <- i
			i = 1 - i // toggle
		}
	}
}

// plumbstart opens the plumbing ports and, if the receive ("edit") port is
// available, launches plumbproc. The send port is optional ("not open is ok" in
// C); plumbfd >= 0 (here: a non-nil plumbsendfid) is what enables the button-2
// "plumb" menu item. Returns -1 if plumbing is unavailable (no plumber running),
// in which case the terminal runs without it -- and initio falls back to the
// legacy extern FIFO (extstart) just as C does.
func (t *Term) plumbstart() int {
	fsys, err := plumb.Mount()
	if err != nil {
		return -1 // no plumber in this name space
	}
	t.plumbfsys = fsys
	t.plumbsendfid, _ = fsys.Open("send", p9.OWRITE) // not open is ok
	if t.plumbsendfid != nil {
		t.plumbfd = 0
	}
	fid, err := fsys.Open("edit", p9.OREAD)
	if err != nil {
		// untested: reached only if the plumber serves the name space but lacks an
		// "edit" port; a running plumber always exports edit, so tests either find
		// both ports (plumb integration tests) or no plumber at all (Mount fails).
		return -1
	}
	t.plumbc = make(chan int)
	go t.plumbproc(fid)
	return 1
}

func (t *Term) hostproc() {
	i := 0
	for {
		i = 1 - i // toggle
		n, err := t.hostr.Read(t.hostbuf[i].data[:])
		if n <= 0 || err != nil {
			// EOF or error: the host is gone. Record why and close hostc so
			// waitforio wakes and raises the hostError; only the event-loop
			// goroutine can unwind the in-process terminal. (After Hexit the
			// loop has already returned and nobody hears the close, matching
			// C's quiet threadexits when exiting is set.)
			if err == nil || err == io.EOF {
				err = errors.New("unexpected eof")
			}
			t.hosterr = err
			close(t.hostc)
			return
		}
		t.hostbuf[i].n = n
		t.hostc <- i
	}
}

// hoststart launches the goroutine that reads protocol bytes from the host.
func (t *Term) hoststart() {
	t.hostc = make(chan int)
	go t.hostproc()
}
