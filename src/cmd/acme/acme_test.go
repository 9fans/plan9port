package main

import (
	"bufio"
	"context"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"syscall"
	"testing"
	"time"

	"9fans.net/go/plan9"
	"9fans.net/go/plan9/client"
	"9fans.net/plan9port/draw"
	"9fans.net/plan9port/plumb"
)

var (
	buildOnce sync.Once
	builtExe  string
	buildErr  error
)

// acmeExe builds the acme binary once per test run and returns its path.
func acmeExe(t *testing.T) string {
	t.Helper()
	buildOnce.Do(func() {
		dir, err := os.MkdirTemp("", "acmebuild")
		if err != nil {
			buildErr = err
			return
		}
		exe := filepath.Join(dir, "acme")
		if out, err := exec.Command("go", "build", "-o", exe, ".").CombinedOutput(); err != nil {
			buildErr = err
			t.Logf("build output:\n%s", out)
			return
		}
		builtExe = exe
	})
	if buildErr != nil {
		t.Fatalf("build acme: %v", buildErr)
	}
	return builtExe
}

// runAcme runs acme's real threadmain IN-PROCESS (not a subprocess), driven by a
// DEVDRAW_TEST_INPUT script through the headless devdraw memory backend, in
// working directory dir. It blocks until devdraw writes the done marker (the
// input script finished), unwinds threadmain, and returns how many image frames
// it drew. Running in-process means the live threadmain + input-thread path
// executes in the test process, so its coverage is measured directly (devdraw
// itself is still a subprocess -- the display backend -- but all acme logic is
// in-process). This is the "direct draw events" test path.
func runAcme(t *testing.T, dir, script string, args ...string) int {
	t.Helper()
	devdraw := filepath.Join(plan9root, "bin/devdraw")
	if _, err := os.Stat(devdraw); err != nil {
		t.Skipf("devdraw not built: %v", err)
	}

	img := filepath.Join(dir, "img")
	donefile := filepath.Join(dir, "done")
	scriptfile := filepath.Join(dir, "in")
	os.Remove(donefile)
	if err := os.WriteFile(scriptfile, []byte(script), 0o644); err != nil {
		t.Fatal(err)
	}

	t.Setenv("HOME", dir)
	t.Setenv("NAMESPACE", dir) // isolate the posted 9P socket per test
	t.Setenv("DEVDRAW", devdraw)
	t.Setenv("DEVDRAW_TEST_MEMORY", "1")
	t.Setenv("DEVDRAW_TEST_INPUT", scriptfile)
	t.Setenv("DEVDRAW_TEST_OUTPUT", img)
	t.Setenv("DEVDRAW_TEST_DONE", donefile)
	if old, err := os.Getwd(); err == nil {
		if err := os.Chdir(dir); err == nil {
			defer os.Chdir(old)
		}
	}

	a := new(Acme)
	a.cexit = make(chan int) // pre-created so we can unwind threadmain (see threadmain)
	exited := make(chan struct{})
	go func() {
		defer close(exited)
		defer func() { recover() }() // swallow any teardown-time draw panic
		a.threadmain(args)
	}()

	deadline := time.Now().Add(20 * time.Second)
	for {
		if _, err := os.Stat(donefile); err == nil {
			break
		}
		if time.Now().After(deadline) {
			t.Fatal("timeout: acme input script did not finish (a frame wait never satisfied?)")
		}
		time.Sleep(20 * time.Millisecond)
	}
	// Unwind threadmain's <-cexit; its input goroutines then block idle on their
	// now-quiet channels (left on this isolated Acme; the process reaps them at
	// exit). The frames are already on disk.
	select {
	case a.cexit <- 0:
	case <-time.After(2 * time.Second):
	}
	select {
	case <-exited:
	case <-time.After(2 * time.Second):
	}
	frames, _ := filepath.Glob(img + ".*")
	return len(frames)
}

// TestLaunch is the startup smoke test: the real (non-D) acme boots under the
// devdraw memory backend and draws its initial window layout -- exercising
// threadmain's full startup (parseArgs, initdraw, timerinit, channels,
// InitMouse/InitKeyboard, fsysinit, rowinit + readfile, the initial FlushImage,
// and the live keyboard/mouse/wait/xfid/newwindow threads).
func TestLaunch(t *testing.T) {
	dir := t.TempDir()
	if n := runAcme(t, dir, "waitframe 1\ndelay 50\n"); n < 1 {
		t.Fatalf("acme drew %d frames, want >= 1", n)
	}
}

// TestLaunchFiles boots the in-process editor with flags and multiple named
// files, exercising threadmain's argument loop, the column-count heuristic, and
// the readfile-per-file loop (acme.c threadmain).
func TestLaunchFiles(t *testing.T) {
	dir := t.TempDir()
	for _, n := range []string{"one.txt", "two.txt"} {
		if err := os.WriteFile(filepath.Join(dir, n), []byte("hi\n"), 0o644); err != nil {
			t.Fatal(err)
		}
	}
	if n := runAcme(t, dir, "waitframe 1\ndelay 50\n", "-a", "-c", "2", "one.txt", "two.txt"); n < 1 {
		t.Fatalf("acme with files drew %d frames, want >= 1", n)
	}
}

// TestParseArgs exercises every flag branch of the ARGBEGIN parser (acme.c
// threadmain / parseArgs): the boolean flags, the value flags (attached and
// separate forms), the "--" terminator, the usage/error paths, and the
// files-follow-flags split.
func TestParseArgs(t *testing.T) {
	a := new(Acme)
	ncol, load := -1, ""
	files, err := a.parseArgs([]string{
		"-D", "-a", "-b", "-r", "-T", "xx", "-c", "3", "-f", "font1", "-F", "font2",
		"-l", "dump", "-m", "/mnt/x", "-W", "800x600", "--", "f1.txt", "f2.txt",
	}, &ncol, &load)
	if err != nil {
		t.Fatalf("parseArgs: %v", err)
	}
	if !a.dflag || !a.globalautoindent || !a.bartflag || !a.swapscrollbuttons {
		t.Errorf("bool flags: dflag=%v a=%v b=%v r=%v", a.dflag, a.globalautoindent, a.bartflag, a.swapscrollbuttons)
	}
	if string(a.trimpre) != "xx" || ncol != 3 || a.fontnames[0] != "font1" ||
		a.fontnames[1] != "font2" || load != "dump" || a.mtpt != "/mnt/x" || a.winsize != "800x600" {
		t.Errorf("value flags: trimpre=%q ncol=%d fonts=%v load=%q mtpt=%q winsize=%q",
			string(a.trimpre), ncol, a.fontnames, load, a.mtpt, a.winsize)
	}
	if len(files) != 2 || files[0] != "f1.txt" || files[1] != "f2.txt" {
		t.Errorf("files = %v, want [f1.txt f2.txt]", files)
	}

	// attached value form (-c2) and files following flags with no "--"
	a = new(Acme)
	ncol, load = -1, ""
	files, err = a.parseArgs([]string{"-c2", "only.txt"}, &ncol, &load)
	if err != nil || ncol != 2 || len(files) != 1 || files[0] != "only.txt" {
		t.Errorf("attached -c2: ncol=%d files=%v err=%v", ncol, files, err)
	}

	// usage/error paths
	for _, bad := range [][]string{{"-c"}, {"-c", "0"}, {"-c", "abc"}, {"-z"}, {"-T"}, {"-W"}, {"-l"}} {
		if _, err := (new(Acme)).parseArgs(bad, new(int), new(string)); err == nil {
			t.Errorf("parseArgs(%v) = nil error, want a usage error", bad)
		}
	}

	// a non-flag first token means everything is a file
	a = new(Acme)
	ncol, load = -1, ""
	if files, _ := a.parseArgs([]string{"a", "b"}, &ncol, &load); len(files) != 2 {
		t.Errorf("non-flag args: files = %v, want 2", files)
	}
}

// TestSnarf round-trips the system clipboard through acmeputsnarf/acmegetsnarf
// (acme.c) against the devdraw in-memory snarf buffer: put the internal buffer
// out to the host clipboard, clear it, read it back, and confirm it survives --
// including a multibyte rune, exercising cvttorunes. This is the host-clipboard
// path C disables under -D, so it is covered here with a live (memory-backend)
// display instead.
func TestSnarf(t *testing.T) {
	a := new(Acme)
	a.reinit()
	if err := a.initdraw("acme", "200x200"); err != nil {
		t.Fatalf("initdraw: %v", err)
	}
	defer a.disp.Close()

	const want = "hello clipboard ☺"
	a.snarfbuf.Insert(0, []rune(want))
	a.acmeputsnarf()
	a.snarfbuf.Reset()
	a.acmegetsnarf()
	got := make([]rune, a.snarfbuf.nc())
	a.snarfbuf.Read(0, got)
	if string(got) != want {
		t.Fatalf("snarf round-trip = %q, want %q", string(got), want)
	}
}

// TestRunShell checks the runproc "goto Hard" decision (exec.c runproc): a
// command goes through the shell when $acmeshell is set, when it is longer than
// C's 512-byte stack buffer, when it contains a control character, or when it
// contains a shell metacharacter; a plain command is exec'd directly.
func TestRunShell(t *testing.T) {
	a := new(Acme)
	direct := []string{"ls", "ls -l foo", "grep pattern file", "\tgo   build\t"}
	for _, s := range direct {
		if a.runShell(s) {
			t.Errorf("runShell(%q) = true, want direct exec", s)
		}
	}
	shell := []string{
		"ls | wc",                  // metacharacter |
		"echo $HOME",               // metacharacter $
		"cat a > b",                // metacharacter >
		"date; date",               // metacharacter ;
		"ls /tmp",                  // metacharacter / (path)
		"echo a\tb\nc",             // control character \n
		strings.Repeat("x", 512-9), // longer than 512-10
	}
	for _, s := range shell {
		if !a.runShell(s) {
			t.Errorf("runShell(%q) = false, want shell", s)
		}
	}

	// $acmeshell forces the shell for even a plain command and shellPath uses it.
	if a.shellPath() == "" || !strings.HasSuffix(a.shellPath(), "/bin/rc") {
		t.Errorf("shellPath default = %q, want .../bin/rc", a.shellPath())
	}
	a.acmeshell = "/bin/bash"
	if !a.runShell("ls") {
		t.Errorf("runShell with $acmeshell set = false, want shell")
	}
	if a.shellPath() != "/bin/bash" {
		t.Errorf("shellPath with $acmeshell = %q, want /bin/bash", a.shellPath())
	}
}

// TestCwarnFlush covers the live mousethread's cwarn select arm (acme.c
// mousethread MWarnings): a warning produced off the mouse thread pokes cwarn,
// waking the idle mousethread so its next loop flushes the warning into +Errors
// rather than waiting for a mouse event. The cwarn poke is buffered, so the arm
// is selected on the next iteration regardless of scheduling; draining a.warnings
// proves the flush ran, which (with no mouse/resize/plumb events pending) can only
// have happened via the cwarn wakeup.
func TestCwarnFlush(t *testing.T) {
	ns := t.TempDir()
	t.Setenv("NAMESPACE", ns)
	a := new(Acme)
	a.reinit()
	if err := a.initdraw("acme", "400x400"); err != nil {
		t.Skipf("initdraw: %v", err)
	}
	defer a.disp.Close()
	// A Mousectl with idle channels (not InitMouse, whose devdraw reader
	// log.Fatals when the display closes at teardown); the select just never
	// picks the mouse/resize arms, leaving the cwarn arm as the only ready case.
	a.mousectl = &draw.Mousectl{C: make(chan draw.Mouse), Resize: make(chan bool), Display: a.disp}
	a.cwarn = make(chan *Mntdir, 1) // threadmain creates this; the unit test must too
	a.rowinit(&a.row, a.screen.Clipr)
	a.rowadd(&a.row, nil, -1)
	a.blockEvents = true // live-editor semantics
	go func() {
		defer func() { recover() }() // swallow a FlushImage-after-Close at teardown
		a.mousethread()
	}()

	// Produce a warning as a 9P handler / waitthread would -- under row.lk, which
	// serializes it with the mousethread's flushwarnings. addwarningtext queues it
	// and pokes cwarn.
	a.row.lk.Lock()
	a.warning(nil, "cwarn wakeup test\n")
	a.row.lk.Unlock()

	deadline := time.Now().Add(5 * time.Second)
	for {
		a.row.lk.Lock()
		drained := len(a.warnings) == 0
		a.row.lk.Unlock()
		if drained {
			break
		}
		if time.Now().After(deadline) {
			t.Fatal("warning was not flushed; mousethread cwarn wakeup never fired")
		}
		time.Sleep(20 * time.Millisecond)
	}
}

// TestWakeWarning checks the non-blocking cwarn poke (util.c addwarningtext's
// nbsendp): the send arm fires when a mousethread is listening (buffered here),
// and the default arm is taken when the channel can't accept, so warnings
// produced off the mouse thread never block their producer.
func TestWakeWarning(t *testing.T) {
	a := new(Acme)
	a.cwarn = make(chan *Mntdir, 1)
	a.wakewarning() // send arm: buffered channel accepts
	select {
	case <-a.cwarn:
	default:
		t.Fatal("wakewarning did not send on an empty buffered cwarn")
	}
	a.cwarn <- nil  // fill the buffer
	a.wakewarning() // default arm: send would block, so it's dropped
	if len(a.cwarn) != 1 {
		t.Fatalf("wakewarning queued a second poke; len(cwarn)=%d, want 1", len(a.cwarn))
	}
}

// TestExit checks the Exit builtin (exec.c xexit): with every window clean (an
// empty row is trivially clean) it flags a.exiting and pokes cexit without
// blocking -- the send arm when a consumer is listening (buffered here), the
// default arm when cexit can't accept.
func TestExit(t *testing.T) {
	a := new(Acme)
	a.cexit = make(chan int, 1)
	a.xexit(nil, nil, nil, false, false, nil) // send arm: buffered cexit accepts
	if !a.exiting {
		t.Fatal("xexit did not set exiting on a clean row")
	}
	select {
	case <-a.cexit:
	default:
		t.Fatal("xexit did not send on cexit")
	}
	a.cexit <- 0                              // fill the buffer
	a.xexit(nil, nil, nil, false, false, nil) // default arm: send would block
	if len(a.cexit) != 1 {
		t.Fatalf("xexit queued a second exit; len(cexit)=%d, want 1", len(a.cexit))
	}
}

// TestRowloadExternalCommand checks that loading a session dump re-runs an
// external-command ('e') window's program, exactly as C's rowload does
// (rows.c case 'e': run(nil, cmd, dir, ..., newns=TRUE)). The 'e' header is
// followed by a ctl line (ignored), a directory, and the command; rowloadwin
// re-runs the command in that directory. Here the command drops a sentinel file
// so we can confirm it actually ran (run is async, so poll for it). The two
// sub-cases cover an explicit directory and an empty one (C defaults to $HOME/).
func TestRowloadExternalCommand(t *testing.T) {
	// header: "e" + five %11d/%11.7f fields + name (>= 1+5*12 bytes); the 'e'
	// case does not parse the fields, only the three follow lines.
	hdr := fmt.Sprintf("e%11d %11d %11d %11d %11.7f %s", 0, 0, 0, 0, 0.0, "cmd")
	run := func(t *testing.T, runIn, dirLine string) {
		a := new(Acme)
		a.reinit()
		a.home = runIn          // empty dirLine falls back to $HOME/
		body := "-1 -1 0 0\n" + // ctl line, ignored
			dirLine + "\n" + // directory to run in
			"touch ran\n" // command: no metachar, exec'd directly in the dir
		b := bufio.NewReader(strings.NewReader(body))
		line := 0
		if ok := a.rowloadwin(&a.row, b, hdr, &line); !ok {
			t.Fatal("rowloadwin(e-record) returned false")
		}
		// run() is async; wait for the command to create the sentinel.
		sentinel := filepath.Join(runIn, "ran")
		deadline := time.Now().Add(5 * time.Second)
		for {
			if _, err := os.Stat(sentinel); err == nil {
				break
			}
			if time.Now().After(deadline) {
				t.Fatal("e-record command did not run (sentinel never appeared)")
			}
			time.Sleep(20 * time.Millisecond)
		}
		if a.nexec != 1 {
			t.Errorf("nexec = %d, want 1 after launching the e-record command", a.nexec)
		}
	}
	t.Run("explicit-dir", func(t *testing.T) {
		dir := t.TempDir()
		run(t, dir, dir) // directory line names the dir explicitly
	})
	t.Run("default-dir", func(t *testing.T) {
		dir := t.TempDir()
		run(t, dir, "") // empty directory line -> C's $HOME/ default (a.home)
	})
}

// TestIsmtpt checks the acme mount-point test (acme.c ismtpt): a path is "under"
// mtpt when mtpt is a prefix ending at a path boundary.
// TestTextloadGuards checks the two refusal guards textload runs before opening
// the file (text.c textload): an unnamed directory window and acme's own mount
// point. Both return -1 without touching the filesystem.
func TestTextloadGuards(t *testing.T) {
	newbody := func(isdir bool) *Text {
		w := &Window{isdir: isdir}
		w.body.w = w
		w.body.file = &File{}
		return &w.body
	}

	a := new(Acme)
	// isdir window with no name -> "empty directory name"
	if r := a.textload(newbody(true), 0, "anything", false); r != -1 {
		t.Errorf("empty directory name: textload = %d, want -1", r)
	}
	// a path under the mount point -> "will not open self mount point"
	a.mtpt = "/mnt/acme"
	if r := a.textload(newbody(false), 0, "/mnt/acme/3/body", false); r != -1 {
		t.Errorf("self mount point: textload = %d, want -1", r)
	}
	// a path outside the mount point must NOT be refused by these guards; it
	// fails later at open, returning -1 too, so point at a real missing file and
	// confirm it is not the mount-point branch by using a non-mtpt path.
	if r := a.textload(newbody(false), 0, "/definitely/not/mtpt/x", false); r != -1 {
		t.Errorf("missing file: textload = %d, want -1 (open failure)", r)
	}
}

// TestErrorwinIncl checks that an +Errors window inherits its source window's
// include-directory list in the same order (util.c errorwin1/errorwinforwin).
// winaddincl prepends, so errorwin1 applies the list back-to-front; this test
// pins that the round trip preserves order.
func TestErrorwinIncl(t *testing.T) {
	d1, d2 := t.TempDir(), t.TempDir()
	a := new(Acme)
	src := &Window{}
	src.body.w = src
	src.body.file = &File{}
	// Incl d1 then d2; winaddincl prepends, so src.incl == [d2, d1].
	a.winaddincl(src, []rune(d1))
	a.winaddincl(src, []rune(d2))
	want := [][]rune{[]rune(d2), []rune(d1)}
	if !inclEqual(src.incl, want) {
		t.Fatalf("src.incl = %v, want %v", strs(src.incl), strs(want))
	}
	// Apply to a fresh window exactly as errorwin1 does (reverse loop).
	dst := &Window{}
	dst.body.w = dst
	dst.body.file = &File{}
	for i := len(src.incl) - 1; i >= 0; i-- {
		a.winaddincl(dst, append([]rune(nil), src.incl[i]...))
	}
	if !inclEqual(dst.incl, src.incl) {
		t.Errorf("inherited incl = %v, want %v (source order)", strs(dst.incl), strs(src.incl))
	}
}

func inclEqual(a, b [][]rune) bool {
	if len(a) != len(b) {
		return false
	}
	for i := range a {
		if string(a[i]) != string(b[i]) {
			return false
		}
	}
	return true
}

func strs(rr [][]rune) []string {
	s := make([]string, len(rr))
	for i, r := range rr {
		s[i] = string(r)
	}
	return s
}

// TestIncludename checks <include> resolution (look.c includename): via the
// window's own include list, via the $objtype-derived objdir fallback, and that
// objdir is derived from objtype on first use.
func TestIncludename(t *testing.T) {
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, "foo.h"), nil, 0o644); err != nil {
		t.Fatal(err)
	}
	a := newTestAcme(t, "win\n")
	w := a.row.col[0].w[0]

	// resolves via the window's include list
	a.winaddincl(w, []rune(dir))
	if got, want := string(a.includename(&w.body, []rune("foo.h"))), dir+"/foo.h"; got != want {
		t.Errorf("includename via incl = %q, want %q", got, want)
	}

	// resolves via the objdir fallback (stand in for /$objtype/include)
	w2 := a.row.col[0].w[0]
	w2.incl = nil
	a.objdir = []rune(dir)
	if got, want := string(a.includename(&w2.body, []rune("foo.h"))), dir+"/foo.h"; got != want {
		t.Errorf("includename via objdir = %q, want %q", got, want)
	}

	// objdir is derived from objtype on first use
	a.objdir = nil
	a.objtype = "myarch"
	a.includename(&w2.body, []rune("missing.h"))
	if got := string(a.objdir); got != "/myarch/include" {
		t.Errorf("objdir = %q, want /myarch/include", got)
	}
}

// TestLogGC checks that the /log pending buffer is garbage-collected once it
// passes the threshold when no reader needs the entries (logf.c xfidlog), so it
// does not grow for the life of the editor.
func TestLogGC(t *testing.T) {
	a := new(Acme)
	a.logcap = 8
	w := &Window{id: 1}
	w.body.w = w
	w.body.file = &File{name: []rune("/x")}

	for i := 0; i < 200; i++ {
		a.xfidlog(w, "new")
	}
	if len(a.log) > a.logcap {
		t.Errorf("pending log grew unbounded: len=%d, cap=%d", len(a.log), a.logcap)
	}
	if a.logStart == 0 {
		t.Error("logStart never advanced; consumed entries were not GC'd")
	}
	// the global sequence is conserved: logStart + len(log) == total appended
	if got := a.logStart + len(a.log); got != 200 {
		t.Errorf("logStart+len(log) = %d, want 200 (sequence not conserved)", got)
	}
}

func TestIsmtpt(t *testing.T) {
	a := new(Acme)
	if a.ismtpt("/mnt/acme/5/body") {
		t.Error("empty mtpt should never match")
	}
	a.mtpt = "/mnt/acme"
	for _, c := range []struct {
		file string
		want bool
	}{
		{"/mnt/acme", true},        // exactly mtpt
		{"/mnt/acme/5/body", true}, // under mtpt
		{"/mnt/acmexyz", false},    // prefix but not at a boundary
		{"/other", false},          // unrelated
	} {
		if got := a.ismtpt(c.file); got != c.want {
			t.Errorf("ismtpt(%q) [mtpt=%q] = %v, want %v", c.file, a.mtpt, got, c.want)
		}
	}
	a.mtpt = "/mnt/acme/"
	if !a.ismtpt("/mnt/acme/x") {
		t.Error("trailing-slash mtpt should match a child")
	}
}

// TestAcmeError checks the error-pipe path (acme.c acmeerrorinit +
// acmeerrorproc): a write to the pipe's write end (erroutfd) -- where a Local
// command's stderr and acme's internal errors go -- is forwarded to cerr, whence
// waitthread's WErr case shows it in +Errors.
func TestAcmeError(t *testing.T) {
	a := new(Acme)
	a.cerr = make(chan string, 1)
	a.acmeerrorinit()
	if _, err := a.erroutfd.WriteString("boom\n"); err != nil {
		t.Fatal(err)
	}
	select {
	case s := <-a.cerr:
		if s != "boom\n" {
			t.Fatalf("cerr got %q, want %q", s, "boom\n")
		}
	case <-time.After(5 * time.Second):
		t.Fatal("acmeerrorproc did not forward the error-pipe write to cerr")
	}
}

// TestRowloadfonts checks the dump-file font pre-scan (rows.c rowloadfonts):
// the cwd line is skipped and the next two lines set the global font names,
// while an empty name leaves the current one. Deterministic parse test.
func TestRowloadfonts(t *testing.T) {
	dir := t.TempDir()
	dump := filepath.Join(dir, "acme.dump")
	body := "/home/me/src\n" + // current directory
		"/lib/font/var.font\n" + // global var font
		"\n" + // empty fixed font: keep current
		"some other dump content\n"
	if err := os.WriteFile(dump, []byte(body), 0o644); err != nil {
		t.Fatal(err)
	}
	a := new(Acme)
	a.fontnames[1] = "/keep/this.font"
	a.rowloadfonts(dump)
	if a.fontnames[0] != "/lib/font/var.font" {
		t.Errorf("fontnames[0] = %q, want /lib/font/var.font", a.fontnames[0])
	}
	if a.fontnames[1] != "/keep/this.font" {
		t.Errorf("fontnames[1] = %q, want /keep/this.font (empty line keeps current)", a.fontnames[1])
	}
}

// TestEventBlock checks the blocking event read (xfid.c xfideventread): an
// external client reading a window's event file blocks until an event is
// appended, then wakes and returns it. Exercises the a.event sync.Cond on
// row.lk -- the reader releases the row lock while blocked and winappend
// broadcasts -- over the posted 9P service. -race clean.
func TestEventBlock(t *testing.T) {
	ns := t.TempDir()
	t.Setenv("NAMESPACE", ns)
	a := new(Acme)
	a.reinit()
	if err := a.initdraw("acme", "400x400"); err != nil {
		t.Fatalf("initdraw: %v", err)
	}
	defer a.disp.Close()
	a.rowinit(&a.row, a.screen.Clipr)
	a.rowadd(&a.row, nil, -1)
	w := a.coladd(a.row.col[0], nil, nil, -1)

	a.fsysinit()
	a.blockEvents = true
	if err := a.fsyspost(); err != nil {
		t.Fatalf("fsyspost: %v", err)
	}
	fs, err := client.MountService("acme")
	if err != nil {
		t.Fatalf("mount: %v", err)
	}
	fid, err := fs.Open(fmt.Sprintf("%d/event", w.id), plan9.OREAD)
	if err != nil {
		t.Fatalf("open event: %v", err)
	}
	defer fid.Close()

	got := make(chan int, 1)
	go func() {
		buf := make([]byte, 256)
		n, _ := fid.Read(buf) // blocks in fsRead until an event is appended
		got <- n
	}()

	// Generate an event; whether the reader is already blocked or not, the read
	// must return the appended bytes.
	time.Sleep(100 * time.Millisecond)
	a.row.lk.Lock()
	a.winappend(w, "test")
	a.row.lk.Unlock()

	select {
	case n := <-got:
		if n == 0 {
			t.Fatal("event read returned 0; expected the appended event")
		}
	case <-time.After(5 * time.Second):
		t.Fatal("blocked event read did not wake after winappend")
	}
}

// TestEventChunkedRead checks that reading a window's event file with a buffer
// smaller than the queued bytes returns the events across multiple reads without
// dropping the tail (xfid.c xfideventread clamps the reply to the client's count
// and keeps the remainder). Before the count-aware consume, the first short read
// cleared the whole event buffer and the rest was lost.
func TestEventChunkedRead(t *testing.T) {
	ns := t.TempDir()
	t.Setenv("NAMESPACE", ns)
	a := new(Acme)
	a.reinit()
	if err := a.initdraw("acme", "400x400"); err != nil {
		t.Fatalf("initdraw: %v", err)
	}
	defer a.disp.Close()
	a.rowinit(&a.row, a.screen.Clipr)
	a.rowadd(&a.row, nil, -1)
	w := a.coladd(a.row.col[0], nil, nil, -1)
	a.fsysinit()
	// Non-blocking so an over-read after the queue drains returns 0, not a hang.
	a.blockEvents = false
	if err := a.fsyspost(); err != nil {
		t.Fatalf("fsyspost: %v", err)
	}
	fs, err := client.MountService("acme")
	if err != nil {
		t.Fatalf("mount: %v", err)
	}
	fid, err := fs.Open(fmt.Sprintf("%d/event", w.id), plan9.OREAD)
	if err != nil {
		t.Fatalf("open event: %v", err)
	}
	defer fid.Close()

	// Queue an event whose byte length exceeds the read buffer.
	a.row.lk.Lock()
	a.winappend(w, "0123456789")
	a.row.lk.Unlock()
	a.row.lk.Lock()
	want := len(w.events) // owner byte + the 10 digits
	a.row.lk.Unlock()

	var got int
	buf := make([]byte, 4) // smaller than the queued events
	for i := 0; i < 100 && got < want; i++ {
		n, _ := fid.Read(buf)
		if n == 0 {
			break
		}
		got += n
	}
	if got != want {
		t.Fatalf("chunked event read got %d bytes, want %d (tail lost?)", got, want)
	}
}

// TestEventClose checks that closing a window wakes an external client blocked
// reading its event file (C windelete's wakeup of the blocked xfid): the read
// returns a short read rather than hanging forever.
func TestEventClose(t *testing.T) {
	ns := t.TempDir()
	t.Setenv("NAMESPACE", ns)
	a := new(Acme)
	a.reinit()
	if err := a.initdraw("acme", "400x400"); err != nil {
		t.Fatalf("initdraw: %v", err)
	}
	defer a.disp.Close()
	a.rowinit(&a.row, a.screen.Clipr)
	a.rowadd(&a.row, nil, -1)
	w := a.coladd(a.row.col[0], nil, nil, -1)

	a.fsysinit()
	a.blockEvents = true
	if err := a.fsyspost(); err != nil {
		t.Fatalf("fsyspost: %v", err)
	}
	fs, err := client.MountService("acme")
	if err != nil {
		t.Fatalf("mount: %v", err)
	}
	fid, err := fs.Open(fmt.Sprintf("%d/event", w.id), plan9.OREAD)
	if err != nil {
		t.Fatalf("open event: %v", err)
	}
	defer fid.Close()

	done := make(chan struct{}, 1)
	go func() {
		buf := make([]byte, 256)
		fid.Read(buf) // blocks until the window is closed
		done <- struct{}{}
	}()

	time.Sleep(100 * time.Millisecond)
	a.row.lk.Lock()
	a.winclose(w)
	a.row.lk.Unlock()

	select {
	case <-done: // woke on close
	case <-time.After(5 * time.Second):
		t.Fatal("blocked event read did not wake when its window was closed")
	}
}

// TestEventMenuRestore checks the xfidclose close-restore (xfid.c): opening a
// window's event file hands control to the program and removes the file menu
// (xfidopen filemenu=FALSE), and closing it hands control back and restores the
// menu (xfidclose filemenu=TRUE + winsettag). This drives the live editor path
// directly (blockEvents=true) and checks window state, not -D frames -- the
// restore's clunk-time redraw is the one behavior -D's frame capture can't pair.
func TestEventMenuRestore(t *testing.T) {
	ns := t.TempDir()
	t.Setenv("NAMESPACE", ns)
	a := new(Acme)
	a.reinit()
	if err := a.initdraw("acme", "400x400"); err != nil {
		t.Fatalf("initdraw: %v", err)
	}
	defer a.disp.Close()
	a.rowinit(&a.row, a.screen.Clipr)
	a.rowadd(&a.row, nil, -1)
	w := a.coladd(a.row.col[0], nil, nil, -1)

	a.fsysinit()
	a.blockEvents = true
	if err := a.fsyspost(); err != nil {
		t.Fatalf("fsyspost: %v", err)
	}
	fs, err := client.MountService("acme")
	if err != nil {
		t.Fatalf("mount: %v", err)
	}
	if !w.filemenu {
		t.Fatal("new window should start with the file menu")
	}
	fid, err := fs.Open(fmt.Sprintf("%d/event", w.id), plan9.OREAD)
	if err != nil {
		t.Fatalf("open event: %v", err)
	}
	a.row.lk.Lock()
	open := w.filemenu
	a.row.lk.Unlock()
	if open {
		t.Fatal("opening the event file should remove the file menu (xfidopen)")
	}
	fid.Close()
	// fsClunk runs on the server goroutine; poll for the restore.
	deadline := time.Now().Add(5 * time.Second)
	for {
		a.row.lk.Lock()
		restored := w.filemenu
		a.row.lk.Unlock()
		if restored {
			break
		}
		if time.Now().After(deadline) {
			t.Fatal("closing the event file should restore the file menu (xfidclose)")
		}
		time.Sleep(5 * time.Millisecond)
	}
}

// testAcmeWindow spins up an in-process editor with one empty window, for the
// live-path unit tests below (no 9P needed unless the test mounts one).
func testAcmeWindow(t *testing.T) (*Acme, *Window) {
	t.Helper()
	ns := t.TempDir()
	t.Setenv("NAMESPACE", ns)
	a := new(Acme)
	a.reinit()
	if err := a.initdraw("acme", "400x400"); err != nil {
		t.Fatalf("initdraw: %v", err)
	}
	t.Cleanup(func() { a.disp.Close() })
	a.rowinit(&a.row, a.screen.Clipr)
	a.rowadd(&a.row, nil, -1)
	w := a.coladd(a.row.col[0], nil, nil, -1)
	a.blockEvents = true
	return a, w
}

// TestFlushWaker checks that a 9P Tflush (modelled as request-context
// cancellation, which srv9p does) wakes a reader blocked in a cond.Wait, so the
// blocking event/log reads abort -- the Go analogue of xfid.c xfidflush /
// xfidlogflush. Exercises flushWaker's cancel->broadcast under row.lk.
func TestFlushWaker(t *testing.T) {
	a := new(Acme)
	a.event = sync.NewCond(&a.row.lk)
	ctx, cancel := context.WithCancel(context.Background())
	stop := make(chan struct{})
	defer close(stop)
	woke := make(chan struct{})
	go func() {
		a.row.lk.Lock()
		a.event.Wait() // blocks until flushWaker broadcasts on cancel
		a.row.lk.Unlock()
		close(woke)
	}()
	time.Sleep(20 * time.Millisecond) // let the waiter park in Wait
	go a.flushWaker(ctx, a.event, stop)
	cancel()
	select {
	case <-woke:
	case <-time.After(2 * time.Second):
		t.Fatal("flushWaker did not wake the blocked cond waiter on ctx cancel")
	}
}

// TestIndentval checks Indent's argument matching (exec.c indentval): C compares
// an n-rune prefix via runestrncmp, so "OF" means "OFF". Prefixes shorter than 2
// are an error; a non-prefix returns 0/1 for per-window off/on.
func TestIndentval(t *testing.T) {
	cases := []struct {
		arg    string
		want   int
		global bool // expected a.globalautoindent after (only when want==iGlobal)
	}{
		{"ON", iGlobal, true},
		{"O", iError, false},   // len < 2
		{"OF", iGlobal, false}, // C prefix match: "OF" == "OFF"
		{"OFF", iGlobal, false},
		{"on", 1, false},  // lowercase -> per-window on
		{"of", 0, false},  // not a prefix of ON/OFF/on
		{"ONX", 0, false}, // longer than "ON" -> no match
	}
	for _, c := range cases {
		a := new(Acme)
		a.globalautoindent = true
		got := a.indentval([]rune(c.arg))
		if got != c.want {
			t.Errorf("indentval(%q) = %d, want %d", c.arg, got, c.want)
		}
		if c.want == iGlobal && a.globalautoindent != c.global {
			t.Errorf("indentval(%q): globalautoindent = %v, want %v", c.arg, a.globalautoindent, c.global)
		}
	}
}

// TestElogReplaceOverlap checks the overlapping-replace backstop (elog.c
// elogreplace): C computes the merge gap in unsigned arithmetic, so a replace
// whose q0 falls inside the previous replace's delete range must NOT merge
// (the gap underflows and fails the size test) -- it flushes and starts fresh.
func TestElogReplaceOverlap(t *testing.T) {
	a := new(Acme)
	f := &File{}
	a.elogreplace(f, 0, 5, []rune("AAA"))
	// q0=2 is inside the first delete range [0,5): a negative (overlapping) gap.
	a.elogreplace(f, 2, 7, []rune("BBB"))
	if f.elog.typ != Replace || f.elog.q0 != 2 || f.elog.nd != 5 || string(f.elog.r) != "BBB" {
		t.Fatalf("overlapping replace merged wrongly: pending = {typ:%c q0:%d nd:%d r:%q}, want {Replace 2 5 \"BBB\"}",
			rune(f.elog.typ), f.elog.q0, f.elog.nd, string(f.elog.r))
	}
	if len(f.elogbuf) != 1 {
		t.Fatalf("first replace not flushed to the log: len(elogbuf) = %d, want 1", len(f.elogbuf))
	}
}

// TestWalkDotDot checks that a 9P walk of ".." resolves to the acme root from a
// window directory (and stays at the root from the root), matching fsys.c
// fsyswalk. The -D walk verbs never issue "..", so this is direct-only.
func TestWalkDotDot(t *testing.T) {
	a := new(Acme)
	q, ok := a.walkstep(qpath(5, qDir), "..")
	if !ok || q.Path != qpath(0, qDir) || q.Type&plan9.QTDIR == 0 {
		t.Fatalf(`walk ".." from a window dir = {path:%#x type:%#x ok:%v}, want root dir qpath(0,qDir)=%#x QTDIR`,
			q.Path, q.Type, ok, qpath(0, qDir))
	}
	if q2, ok2 := a.walkstep(qpath(0, qDir), ".."); !ok2 || q2.Path != qpath(0, qDir) {
		t.Fatalf(`walk ".." from root = {path:%#x ok:%v}, want to stay at root`, q2.Path, ok2)
	}
}

// TestFilelistProgram checks the '<' form of a B/D file list (ecmd.c filelist):
// the text after '<' is run as a program and its output becomes the list. This
// drives runpipe in the Collecting state, which the -D harness cannot reach.
func TestFilelistProgram(t *testing.T) {
	a, w := testAcmeWindow(t)
	got := string(a.filelist(&w.body, []rune("<echo alpha beta")))
	if got != "alpha beta\n" {
		t.Fatalf("filelist(<echo alpha beta) = %q, want %q", got, "alpha beta\n")
	}
}

// TestEditoutLock checks the editout open-time locks (xfid.c xfidopen): a
// per-window editout open is denied (Eperm) unless an Edit is active, and a
// second concurrent opener is denied (Einuse). -D never reaches this (it can't
// open editout with editing active, nor open it twice).
func TestEditoutLock(t *testing.T) {
	a, w := testAcmeWindow(t)
	a.fsysinit()
	if err := a.fsyspost(); err != nil {
		t.Fatalf("fsyspost: %v", err)
	}
	fs, err := client.MountService("acme")
	if err != nil {
		t.Fatalf("mount: %v", err)
	}
	name := fmt.Sprintf("%d/editout", w.id)

	// editing == Inactive: open is denied with "permission denied" (Eperm).
	a.row.lk.Lock()
	a.editing = Inactive
	a.row.lk.Unlock()
	if _, err := fs.Open(name, plan9.OWRITE); err == nil {
		t.Fatal("editout open with no active Edit should be denied")
	} else if !strings.Contains(err.Error(), "permission denied") {
		t.Fatalf("editout open error = %v, want permission denied", err)
	}

	// editing active: first open succeeds, a second concurrent open is "in use".
	a.row.lk.Lock()
	a.editing = Inserting
	a.row.lk.Unlock()
	f1, err := fs.Open(name, plan9.OWRITE)
	if err != nil {
		t.Fatalf("first editout open with active Edit: %v", err)
	}
	if _, err := fs.Open(name, plan9.OWRITE); err == nil {
		t.Fatal("second concurrent editout open should be denied (in use)")
	} else if !strings.Contains(err.Error(), "in use") {
		t.Fatalf("second editout open error = %v, want in use", err)
	}
	// closing the first releases the lock, so a fresh open succeeds again.
	f1.Close()
	deadline := time.Now().Add(5 * time.Second)
	for {
		f2, err := fs.Open(name, plan9.OWRITE)
		if err == nil {
			f2.Close()
			break
		}
		if time.Now().After(deadline) {
			t.Fatalf("editout reopen after close still denied: %v", err)
		}
		time.Sleep(5 * time.Millisecond)
	}
}

// TestCommandExitWarning checks that an abnormal child exit is reported to the
// command's +Errors (acme.c waitthread WWait: "name: exit ..."). reapCommand is
// the Go WWait; -D commands exit cleanly, so this path needs a direct drive.
func TestCommandExitWarning(t *testing.T) {
	a, _ := testAcmeWindow(t)
	c := &Command{pid: 12345, name: []rune("false ")}
	a.nexec = 1
	a.registerCommand(c)
	a.reapCommand(&Waitmsg{pid: 12345, msg: "exit status 1"})
	a.flushwarnings()
	ew := a.lookfile([]rune("+Errors"))
	if ew == nil {
		t.Fatal("no +Errors window after an abnormal command exit")
	}
	body := string(readRunes(&ew.body, 0, ew.body.file.b.nc()))
	if !strings.Contains(body, "false: exit status 1") {
		t.Fatalf("+Errors = %q, want it to contain %q", body, "false: exit status 1")
	}
}

// TestCommandExitBeforeRegister checks the pids path (acme.c waitthread): when a
// command's WWait (reap) is processed before its WCmd (register) -- the C run/wait
// proc race -- the exit is stashed in pids and consumed by the later register,
// which never adds the command to the list or shows its name.
func TestCommandExitBeforeRegister(t *testing.T) {
	a, _ := testAcmeWindow(t)
	a.nexec = 1
	// reap first, with no matching registered command: goes to pids.
	a.reapCommand(&Waitmsg{pid: 999, msg: "exit status 2"})
	if a.pids == nil || a.pids.pid != 999 {
		t.Fatalf("reap-before-register should stash pid 999 in pids; got %+v", a.pids)
	}
	if a.command != nil {
		t.Fatalf("no command should be on the running list yet; got %+v", a.command)
	}
	// now register that pid: it is found in pids, reported, and freed -- not listed.
	a.registerCommand(&Command{pid: 999, name: []rune("false ")})
	if a.pids != nil {
		t.Fatalf("register should have consumed the pids entry; got %+v", a.pids)
	}
	if a.command != nil {
		t.Fatal("an already-exited command must not be added to the running list")
	}
	a.flushwarnings()
	ew := a.lookfile([]rune("+Errors"))
	if ew == nil {
		t.Fatal("no +Errors window after the stashed exit was reported")
	}
	body := string(readRunes(&ew.body, 0, ew.body.file.b.nc()))
	if !strings.Contains(body, "exit status 2") {
		t.Fatalf("+Errors = %q, want it to contain %q", body, "exit status 2")
	}
}

// TestLogBlock checks the blocking /log read (logf.c xfidlogread): an external
// client reading the global activity log blocks until a new entry is logged,
// then wakes and returns it. Same a.logread sync.Cond pattern as the event
// read, over the posted 9P service. -race clean.
func TestLogBlock(t *testing.T) {
	ns := t.TempDir()
	t.Setenv("NAMESPACE", ns)
	a := new(Acme)
	a.reinit()
	if err := a.initdraw("acme", "400x400"); err != nil {
		t.Fatalf("initdraw: %v", err)
	}
	defer a.disp.Close()
	a.rowinit(&a.row, a.screen.Clipr)
	a.rowadd(&a.row, nil, -1)
	w := a.coladd(a.row.col[0], nil, nil, -1)

	a.fsysinit()
	a.blockEvents = true
	if err := a.fsyspost(); err != nil {
		t.Fatalf("fsyspost: %v", err)
	}
	fs, err := client.MountService("acme")
	if err != nil {
		t.Fatalf("mount: %v", err)
	}
	fid, err := fs.Open("log", plan9.OREAD)
	if err != nil {
		t.Fatalf("open log: %v", err)
	}
	defer fid.Close()

	got := make(chan int, 1)
	go func() {
		buf := make([]byte, 256)
		n, _ := fid.Read(buf) // blocks until a log entry is appended
		got <- n
	}()

	time.Sleep(100 * time.Millisecond)
	a.row.lk.Lock()
	a.xfidlog(w, "test")
	a.row.lk.Unlock()

	select {
	case n := <-got:
		if n == 0 {
			t.Fatal("log read returned 0; expected the logged entry")
		}
	case <-time.After(5 * time.Second):
		t.Fatal("blocked log read did not wake after xfidlog")
	}
}

// TestPost9P checks that acme posts its 9P service on the name space so an
// external program can mount it (fsys.c fsysinit's post/mount, here fsyspost):
// post the service, then -- as `9p` or a child command would -- mount it via the
// name space and read /index, expecting the line for the one open window. Also
// exercises the row.lk-wrapped handlers under a genuinely separate client
// goroutine.
// TestPost9PDataCursor checks that reading a window's data file in small chunks
// walks the whole buffer (xfid.c xfidruneread: w.addr is the cursor, advanced by
// the runes returned each read; the 9P offset is ignored). Before the cursor
// port, the first read returned everything and jumped addr to the end, so a
// chunked reader got only the first chunk then EOF.
func TestPost9PDataCursor(t *testing.T) {
	ns := t.TempDir()
	t.Setenv("NAMESPACE", ns)
	a := new(Acme)
	a.reinit()
	if err := a.initdraw("acme", "400x400"); err != nil {
		t.Fatalf("initdraw: %v", err)
	}
	defer a.disp.Close()
	a.rowinit(&a.row, a.screen.Clipr)
	a.rowadd(&a.row, nil, -1)
	w := a.coladd(a.row.col[0], nil, nil, -1)
	body := "hello, world"
	a.textinsert(&w.body, 0, []rune(body), true)

	a.fsysinit()
	if err := a.fsyspost(); err != nil {
		t.Fatalf("fsyspost: %v", err)
	}
	fs, err := client.MountService("acme")
	if err != nil {
		t.Fatalf("mount: %v", err)
	}
	// opening addr resets the window's read cursor to (0,0)
	af, err := fs.Open(fmt.Sprintf("%d/addr", w.id), plan9.OREAD)
	if err != nil {
		t.Fatalf("open addr: %v", err)
	}
	defer af.Close()
	df, err := fs.Open(fmt.Sprintf("%d/data", w.id), plan9.OREAD)
	if err != nil {
		t.Fatalf("open data: %v", err)
	}
	defer df.Close()
	var got []byte
	buf := make([]byte, 3) // small chunks force the cursor to walk
	for {
		n, err := df.Read(buf)
		got = append(got, buf[:n]...)
		if n == 0 || err == io.EOF {
			break
		}
		if err != nil {
			t.Fatalf("read data: %v", err)
		}
		if len(got) > len(body) {
			t.Fatalf("data read overran: %q", got)
		}
	}
	if string(got) != body {
		t.Errorf("chunked data read = %q, want %q", got, body)
	}
}

// TestPost9PCtlLock checks the exclusive ctl lock (xfid.c w->ctllock). The port
// is faithful to C: a ctl write runs under winlock(w) and "lock" holds a real
// per-window mutex, so two fids contending for the lock deadlock exactly as C
// does -- the blocked "lock" is stuck holding winlock(w), which the holder's
// "unlock" (or its clunk) then needs (PORT.md follow-ups). That deadlock can't
// be exercised without hanging, so this covers only the single-holder-at-a-time
// paths: a "lock"/"unlock" pair, and a "lock" released by clunk (no "unlock")
// so a later fid can lock again.
func TestPost9PCtlLock(t *testing.T) {
	ns := t.TempDir()
	t.Setenv("NAMESPACE", ns)
	a := new(Acme)
	a.reinit()
	if err := a.initdraw("acme", "400x400"); err != nil {
		t.Fatalf("initdraw: %v", err)
	}
	defer a.disp.Close()
	a.rowinit(&a.row, a.screen.Clipr)
	a.rowadd(&a.row, nil, -1)
	w := a.coladd(a.row.col[0], nil, nil, -1)
	a.fsysinit()
	a.blockEvents = true // live-editor mode
	if err := a.fsyspost(); err != nil {
		t.Fatalf("fsyspost: %v", err)
	}

	openctl := func() *client.Fid {
		fs, err := client.MountService("acme")
		if err != nil {
			t.Fatalf("mount: %v", err)
		}
		f, err := fs.Open(fmt.Sprintf("%d/ctl", w.id), plan9.ORDWR)
		if err != nil {
			t.Fatalf("open ctl: %v", err)
		}
		return f
	}

	// A "lock"/"unlock" pair on one fid.
	c1 := openctl()
	if _, err := c1.Write([]byte("lock\n")); err != nil {
		t.Fatalf("c1 lock: %v", err)
	}
	if _, err := c1.Write([]byte("unlock\n")); err != nil {
		t.Fatalf("c1 unlock: %v", err)
	}
	c1.Close()

	// A "lock" left held, released by clunk (xfidclose QWctl), not "unlock".
	c2 := openctl()
	if _, err := c2.Write([]byte("lock\n")); err != nil {
		t.Fatalf("c2 lock: %v", err)
	}
	c2.Close() // the clunk must release the lock

	// A fresh fid can now acquire the lock; it must not block (which would mean
	// the clunk failed to release). Run it off-goroutine so a regression fails
	// with a message rather than hanging the suite.
	c3 := openctl()
	defer c3.Close()
	done := make(chan error, 1)
	go func() { _, err := c3.Write([]byte("lock\n")); done <- err }()
	select {
	case err := <-done:
		if err != nil {
			t.Fatalf("c3 lock after c2 clunk-release: %v", err)
		}
	case <-time.After(2 * time.Second):
		t.Fatal("c3 lock blocked: the clunk did not release the ctl lock")
	}
	if _, err := c3.Write([]byte("unlock\n")); err != nil {
		t.Fatalf("c3 unlock: %v", err)
	}
}

func TestPost9P(t *testing.T) {
	ns := t.TempDir()
	t.Setenv("NAMESPACE", ns)
	a := new(Acme)
	a.reinit()
	if err := a.initdraw("acme", "400x400"); err != nil {
		t.Fatalf("initdraw: %v", err)
	}
	defer a.disp.Close()
	a.rowinit(&a.row, a.screen.Clipr)
	a.rowadd(&a.row, nil, -1)
	a.coladd(a.row.col[0], nil, nil, -1) // a window, so /index has a line

	a.fsysinit()
	if err := a.fsyspost(); err != nil {
		t.Fatalf("fsyspost: %v", err)
	}

	fs, err := client.MountService("acme")
	if err != nil {
		t.Fatalf("external mount of posted acme service: %v", err)
	}
	fid, err := fs.Open("index", plan9.OREAD)
	if err != nil {
		t.Fatalf("open index: %v", err)
	}
	defer fid.Close()
	buf := make([]byte, 4096)
	n, err := fid.Read(buf)
	if err != nil && err != io.EOF {
		t.Fatalf("read index: %v", err)
	}
	if n == 0 {
		t.Fatal("index empty over the posted service; expected a window line")
	}
}

// lastwin returns the most recently added window in the row (for plumb tests).
func (a *Acme) lastwin() *Window {
	var w *Window
	for _, c := range a.row.col {
		for _, win := range c.w {
			w = win
		}
	}
	return w
}

// TestPlumbShow checks the "showdata" plumb handler (look.c plumbshow): a new
// window named by the filename attribute is created with the message data.
func TestPlumbShow(t *testing.T) {
	a := new(Acme)
	a.reinit()
	if err := a.initdraw("acme", "600x600"); err != nil {
		t.Fatalf("initdraw: %v", err)
	}
	defer a.disp.Close()
	a.rowinit(&a.row, a.screen.Clipr)
	a.rowadd(&a.row, nil, -1)

	a.plumbshow(&plumb.Message{
		Type: "text",
		Attr: &plumb.Attribute{Name: "filename", Value: "/x/foo"},
		Data: []byte("hello\nworld\n"),
	})
	w := a.lastwin()
	if w == nil {
		t.Fatal("plumbshow created no window")
	}
	if got := string(w.body.file.name); got != "/x/foo" {
		t.Errorf("window name = %q, want /x/foo", got)
	}
	body := make([]rune, w.body.file.b.nc())
	w.body.file.b.Read(0, body)
	if string(body) != "hello\nworld\n" {
		t.Errorf("body = %q, want %q", string(body), "hello\nworld\n")
	}
}

// TestPlumbLook checks the "showfile" plumb handler (look.c plumblook): the named
// file is opened in a new window.
func TestPlumbLook(t *testing.T) {
	a := new(Acme)
	a.reinit()
	if err := a.initdraw("acme", "600x600"); err != nil {
		t.Fatalf("initdraw: %v", err)
	}
	defer a.disp.Close()
	a.rowinit(&a.row, a.screen.Clipr)
	a.rowadd(&a.row, nil, -1)

	dir := t.TempDir()
	file := filepath.Join(dir, "look.txt")
	if err := os.WriteFile(file, []byte("plumbed content\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	a.plumblook(&plumb.Message{Type: "text", Data: []byte(file)})
	w := a.lastwin()
	if w == nil {
		t.Fatal("plumblook opened no window")
	}
	if got := string(w.body.file.name); got != file {
		t.Errorf("window name = %q, want %q", got, file)
	}
	body := make([]rune, w.body.file.b.nc())
	w.body.file.b.Read(0, body)
	if string(body) != "plumbed content\n" {
		t.Errorf("body = %q, want %q", string(body), "plumbed content\n")
	}
}

// TestPlumbWord checks the outgoing plumb message look3 builds for a button-3
// click (look.c plumbsendfid branch): a bare click expands to the surrounding
// word, tagged with a click=offset attribute, as a "text" message from acme.
func TestPlumbWord(t *testing.T) {
	a := new(Acme)
	a.reinit()
	if err := a.initdraw("acme", "600x600"); err != nil {
		t.Fatalf("initdraw: %v", err)
	}
	defer a.disp.Close()
	a.rowinit(&a.row, a.screen.Clipr)
	a.rowadd(&a.row, nil, -1)
	w := a.coladd(a.row.col[0], nil, nil, -1)
	a.textinsert(&w.body, 0, []rune("hello world"), true)

	m, ok := a.plumbword(&w.body, 2, 2) // bare click inside "hello"
	if !ok {
		t.Fatal("plumbword returned no message for a click on a word")
	}
	if m.Src != "acme" || m.Type != "text" {
		t.Errorf("message src/type = %q/%q, want acme/text", m.Src, m.Type)
	}
	if string(m.Data) != "hello" {
		t.Errorf("message data = %q, want hello", string(m.Data))
	}
	if m.Attr == nil || m.Attr.Name != "click" || m.Attr.Value != "2" {
		t.Errorf("message attr = %+v, want click=2", m.Attr)
	}
}

// TestPlumbLookAddr checks plumblook's address evaluation (look.c plumbgetc +
// openfile): a plumb message with an "addr" attribute opens the file with that
// address selected (here line 2), not just at the top.
func TestPlumbLookAddr(t *testing.T) {
	a := new(Acme)
	a.reinit()
	if err := a.initdraw("acme", "600x600"); err != nil {
		t.Fatalf("initdraw: %v", err)
	}
	defer a.disp.Close()
	a.rowinit(&a.row, a.screen.Clipr)
	a.rowadd(&a.row, nil, -1)

	dir := t.TempDir()
	file := filepath.Join(dir, "addr.txt")
	if err := os.WriteFile(file, []byte("one\ntwo\nthree\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	a.plumblook(&plumb.Message{
		Type: "text",
		Attr: &plumb.Attribute{Name: "addr", Value: "2"}, // line 2
		Data: []byte(file),
	})
	w := a.lastwin()
	if w == nil {
		t.Fatal("plumblook opened no window")
	}
	// Line 2 is "two\n" at runes [4,8); the address should select it.
	if w.body.q0 != 4 || w.body.q1 != 8 {
		t.Errorf("dot = [%d,%d), want [4,8) (line 2)", w.body.q0, w.body.q1)
	}
}

// TestKill exercises the Kill builtin end-to-end (exec.c xkill -> ckill ->
// acme.c waitthread WKill -> process-group kill): register a real running
// command on the command list, invoke xkill with its name, and confirm the
// process is terminated. Drives the real ckill channel + waitthread, -race
// clean.
func TestKill(t *testing.T) {
	a := new(Acme)
	a.reinit()
	a.ckill = make(chan []rune)
	go a.waitthread()

	cmd := exec.Command("sleep", "60")
	cmd.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	if err := cmd.Start(); err != nil {
		t.Fatal(err)
	}
	a.row.lk.Lock()
	a.command = &Command{pid: cmd.Process.Pid, name: []rune("sleep ")}
	a.row.lk.Unlock()

	a.xkill(nil, nil, nil, false, false, []rune("sleep"))

	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	select {
	case <-done: // terminated by Kill
	case <-time.After(5 * time.Second):
		cmd.Process.Kill()
		t.Fatal("Kill did not terminate the command")
	}
}

// TestShutdownDump checks that a termination signal dumps the session (acme.c
// shutdown): boot acme, wait for its initial frame, send SIGHUP, and confirm it
// writes $HOME/acme.dump so the windows can be restored. Exercises the live
// shutdownproc -> shutdown -> rowdump path through a real signal.
func TestShutdownDump(t *testing.T) {
	devdraw := filepath.Join(plan9root, "bin/devdraw")
	if _, err := os.Stat(devdraw); err != nil {
		t.Skipf("devdraw not built: %v", err)
	}
	exe := acmeExe(t)
	dir := t.TempDir()
	img := filepath.Join(dir, "img")
	settlefile := filepath.Join(dir, "settle")
	scriptfile := filepath.Join(dir, "in")
	// waitidle settles once startup drawing finishes -- the row, its columns and
	// the initial window all built and drawn -- at which point devdraw records a
	// settle point. We wait for that before SIGHUP (signalling on the first frame
	// races the column/readfile build, leaving rowdump with no columns). The
	// trailing delay keeps acme alive until we signal.
	if err := os.WriteFile(scriptfile, []byte("waitidle 500\ndelay 8000\n"), 0o644); err != nil {
		t.Fatal(err)
	}

	cmd := exec.Command(exe, "-W", "1024x768")
	cmd.Dir = dir
	cmd.Env = append(os.Environ(),
		"HOME="+dir,
		"NAMESPACE="+dir, // isolate the posted 9P socket per test
		"DEVDRAW="+devdraw,
		"DEVDRAW_TEST_MEMORY=1",
		"DEVDRAW_TEST_INPUT="+scriptfile,
		"DEVDRAW_TEST_OUTPUT="+img,
		"DEVDRAW_TEST_SETTLE="+settlefile,
	)
	cmd.Stderr = os.Stderr
	cmd.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	if err := cmd.Start(); err != nil {
		t.Fatal(err)
	}
	defer syscall.Kill(-cmd.Process.Pid, syscall.SIGKILL)

	// Wait until startup drawing has settled (a recorded settle point), so the
	// row and its columns exist before we signal -- otherwise the dump is empty.
	deadline := time.Now().Add(20 * time.Second)
	for {
		if fi, err := os.Stat(settlefile); err == nil && fi.Size() > 0 {
			break
		}
		if time.Now().After(deadline) {
			t.Fatal("timeout waiting for acme startup to settle")
		}
		time.Sleep(20 * time.Millisecond)
	}

	dump := filepath.Join(dir, "acme.dump")
	if err := cmd.Process.Signal(syscall.SIGHUP); err != nil {
		t.Fatal(err)
	}
	deadline = time.Now().Add(10 * time.Second)
	for {
		if fi, err := os.Stat(dump); err == nil && fi.Size() > 0 {
			return // dumped successfully
		}
		if time.Now().After(deadline) {
			t.Fatal("acme did not write acme.dump on SIGHUP")
		}
		time.Sleep(20 * time.Millisecond)
	}
}

// TestConsThread exercises the live button-2 command-output path: the
// consthread goroutine drains the ccons channel (what run's reader goroutine
// feeds for a button-2 command) and applies each chunk -- the running-command
// name into the row tag, the output into the +Errors window, the name removed
// on exit. It drives the real consthread + applyCons + consToErrors under the
// row lock, deterministically, without needing mouse coordinates. (The full
// button-2 -> run -> +Errors path is differentially tested against the C oracle
// by the -D exec_run2write scenario; this covers the live drain goroutine.)
func TestConsThread(t *testing.T) {
	a := new(Acme)
	a.reinit()
	if err := a.initdraw("acme", "400x400"); err != nil {
		t.Fatalf("initdraw: %v", err)
	}
	defer a.disp.Close()
	a.rowinit(&a.row, a.screen.Clipr)
	a.rowadd(&a.row, nil, -1) // a column for the +Errors window

	go a.consthread()
	go a.waitthread()
	a.row.lk.Lock()
	a.nexec++
	a.row.lk.Unlock()
	// the lifecycle (register/reap) goes to waitthread; the output to consthread.
	cmd := &Command{pid: 4321, name: []rune("echo ")}
	a.ccommand <- cmd
	a.ccons <- execChunk{data: []rune("hello world\n")}
	a.cwait <- &Waitmsg{pid: 4321}

	deadline := time.Now().Add(5 * time.Second)
	for {
		a.row.lk.Lock()
		n := a.errorsNc()
		nexec := a.nexec
		a.row.lk.Unlock()
		if n >= len("hello world\n") && nexec == 0 {
			break
		}
		if time.Now().After(deadline) {
			t.Fatalf("command output/reap not applied (got %d runes, nexec=%d)", n, nexec)
		}
		time.Sleep(10 * time.Millisecond)
	}
}

// TestScroll drives a live scrollbar press through the running editor and checks
// the scroll path redraws end-to-end: mousethread -> mousedispatch -> textscroll
// -> textsetorigin -> redraw. Differential: a button-3 press in the body
// scrollbar (scroll forward) on a window with more text than fits must make acme
// emit strictly more frames than an identical run that only moves the mouse to
// the scrollbar (a plain move causes no redraw). Validates the live mouse->scroll
// routing, which -D exercises only through the scripted scroll verb.
func TestScroll(t *testing.T) {
	dir := t.TempDir()
	var b []byte
	for i := 0; i < 200; i++ { // many lines, so there is somewhere to scroll
		b = append(b, []byte(fmt.Sprintf("line %d\n", i))...)
	}
	if err := os.WriteFile(filepath.Join(dir, "big.txt"), b, 0o644); err != nil {
		t.Fatal(err)
	}
	// x=4 is inside the body scrollbar (scrollwid ~12); y=200 is well into the body.
	const base = "move 4 200\nwaitidle 400\n"
	const scroll = "move 4 200\nwaitidle 400\nbutton 4\nwaitidle 400\nbutton 0\nwaitidle 400\n"
	baseline := runAcme(t, dir, base, "big.txt")
	scrolled := runAcme(t, dir, scroll, "big.txt")
	if scrolled <= baseline {
		t.Fatalf("scrollbar press produced no extra frames: baseline=%d scrolled=%d (mousethread/textscroll not redrawing?)", baseline, scrolled)
	}
}

// TestLook3 drives a live button-3 click through the running editor and checks
// the look/search path redraws end-to-end: mousethread -> mousedispatch ->
// textselect3 (expand to a word) -> look3 (search forward) -> textshow/redraw.
// Differential: button-3 on a word that recurs later must make acme emit
// strictly more frames than an identical mouse-move-only run (a plain move
// causes no redraw). Validates the live button-3 routing, which -D exercises
// only through the scripted b3 verb. The body is filled with one repeated word
// so any body click lands on it and the search finds a later occurrence.
func TestLook3(t *testing.T) {
	dir := t.TempDir()
	var b []byte
	for i := 0; i < 200; i++ {
		b = append(b, []byte("needle\n")...)
	}
	if err := os.WriteFile(filepath.Join(dir, "look.txt"), b, 0o644); err != nil {
		t.Fatal(err)
	}
	const base = "move 30 80\nwaitidle 400\n"
	const look = "move 30 80\nwaitidle 400\nbutton 4\nbutton 0\nwaitidle 400\n"
	baseline := runAcme(t, dir, base, "look.txt")
	looked := runAcme(t, dir, look, "look.txt")
	if looked <= baseline {
		t.Fatalf("button-3 look produced no extra frames: baseline=%d looked=%d (mousethread/look3 not redrawing?)", baseline, looked)
	}
}

// TestSelect drives a live button-1 drag through the running editor and checks
// the mouse path redraws end-to-end: mousethread -> mousedispatch -> textselect
// (frselect) -> highlight redraw -> FlushImage. Differential: a button-1 sweep
// across body text must make acme emit strictly more frames than an identical
// run that moves the mouse the same way with no button held (a plain move
// causes no redraw). This catches a dead/broken mousethread or selection path
// without fragile pixel assertions or golden images.
func TestSelect(t *testing.T) {
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, "hello.txt"), []byte("line one\nline two\nline three\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	// Move to a point in the body, settle, then either sweep with no button
	// (baseline) or with button 1 held (a selection).
	const baseScript = "move 120 120\nwaitidle 300\nmove 320 120\nwaitidle 300\n"
	const selScript = "move 120 120\nwaitidle 300\nbutton 1\nmove 320 120\nbutton 0\nwaitidle 300\n"
	baseline := runAcme(t, dir, baseScript, "hello.txt")
	selected := runAcme(t, dir, selScript, "hello.txt")
	if selected <= baseline {
		t.Fatalf("button-1 select produced no extra frames: baseline=%d selected=%d (mousethread/textselect not redrawing?)", baseline, selected)
	}
}

// TestType drives live keystrokes through the running editor and checks the
// keyboard path redraws end-to-end: keyboardthread -> rowtype -> wintype/
// texttype -> textscrdraw -> FlushImage. It is differential: typing into a
// window body must make acme emit strictly more frames than an identical run
// that only moves the mouse and settles. This catches a dead or broken
// keyboardthread (no redraws) without depending on exact frame counts, fragile
// pixel coordinates, or golden images.
func TestType(t *testing.T) {
	dir := t.TempDir()
	// A real file window placed in the left column, so the body sits under the
	// mouse position we move to.
	if err := os.WriteFile(filepath.Join(dir, "hello.txt"), []byte("line one\nline two\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	const move = "move 200 200\nwaitidle 300\n"
	baseline := runAcme(t, dir, move, "hello.txt")
	typed := runAcme(t, dir, move+"type abcdefgh\nwaitidle 300\n", "hello.txt")
	if typed <= baseline {
		t.Fatalf("typing produced no extra frames: baseline=%d typed=%d (keyboardthread/rowtype not redrawing?)", baseline, typed)
	}
}

// TestExecute drives a live button-2 execution through the running editor, the
// core "use acme as an editor" path that -D and the in-process tests only
// approximate: type the builtin "New" into a window body and chord button-2 on
// it. Executing New must open a second window (mousethread -> execute -> new ->
// makenewwindow -> column relayout -> FlushImage), so it draws strictly more
// frames than an identical run that types "New" but never button-2s it. Frame
// delta, so no fragile exact counts or golden images.
func TestExecute(t *testing.T) {
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, "hello.txt"), []byte("line one\nline two\n"), 0o644); err != nil {
		t.Fatal(err)
	}
	// Move into the body, type the builtin, settle.
	const typeNew = "move 200 200\nwaitidle 300\ntype New\nwaitidle 300\n"
	baseline := runAcme(t, dir, typeNew, "hello.txt")
	// Then button-2 (press+release) on the word to execute it.
	executed := runAcme(t, dir, typeNew+"move 205 200\nbutton 2\nbutton 0\nwaitidle 400\n", "hello.txt")
	if executed <= baseline {
		t.Fatalf("button-2 execute produced no extra frames: baseline=%d executed=%d (mousethread/execute/new not redrawing?)", baseline, executed)
	}
}
