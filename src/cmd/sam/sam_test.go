// Note: Most tests are in testdata scripts working at either the
// sam -d command level (stream_test.go) or the sam wire protocol level (proto_test.go).
// The tests in this file only cover details that cannot be tested by those end-to-end tests.

package main

import (
	"bytes"
	"errors"
	"io"
	"io/fs"
	"os"
	"path/filepath"
	"slices"
	"strings"
	"syscall"
	"testing"
)

// The test binary compares the in-process Go sam against the C sam, so it
// formats errors the C way (capitalized %r). The real binary leaves this off.
func init() { compatMode = true }

// TestOSErr covers oserr in both modes and all fallback branches; these are
// not reachable from the differential cases alone.
func TestOSErr(t *testing.T) {
	compatS := &Sam{compat: true}
	plainS := &Sam{compat: false}

	// syscall.Errno: capitalized in compat mode, lowercase otherwise.
	if got := compatS.oserr(syscall.ENOENT); got != "No such file or directory" {
		t.Errorf("compat ENOENT = %q", got)
	}
	if got := plainS.oserr(syscall.ENOENT); got != "no such file or directory" {
		t.Errorf("plain ENOENT = %q", got)
	}

	// A *PathError that is not itself an Errno falls through to pe.Err.
	pe := &fs.PathError{Op: "open", Path: "x", Err: errors.New("boom")}
	if got := plainS.oserr(pe); got != "boom" {
		t.Errorf("PathError = %q", got)
	}

	// A plain error falls through to err.Error().
	if got := plainS.oserr(errors.New("plain")); got != "plain" {
		t.Errorf("plain error = %q", got)
	}
}

// catchExit runs f, which is expected to end by calling s.exits (a samExit
// panic), and returns the exit status. It fails the test if f returns normally.
func catchExit(t *testing.T, f func()) (status string) {
	t.Helper()
	defer func() {
		switch r := recover().(type) {
		case nil:
			t.Fatal("expected exits, but the function returned")
		case samExit:
			status = r.status
		default:
			panic(r)
		}
	}()
	f()
	return
}

// TestBoottermLocal exercises the local samterm launch.
func TestBoottermLocal(t *testing.T) {
	errf, err := os.CreateTemp(t.TempDir(), "err")
	if err != nil {
		t.Fatal(err)
	}
	defer errf.Close()

	s := newSam(strings.NewReader(""), io.Discard, errf)
	s.termprog = "cat"
	s.startup("", false, nil, nil)
	if s.in == nil || s.out == nil {
		t.Fatal("bootterm did not wire s.in/s.out")
	}
	// startup wrote Hversion into the pipe; closing our ends gives cat EOF so it
	// exits (the go cmd.Wait() reaps it).
	s.out.(*os.File).Close()
	s.in.(*os.File).Close()
}

// TestBoottermDefaultProg covers bootterm's default to "samterm"
// when termprog is unset, using a fake samterm on $PATH.
func TestBoottermDefaultProg(t *testing.T) {
	dir := t.TempDir()
	if err := os.WriteFile(filepath.Join(dir, "samterm"), []byte("#!/bin/sh\nexec cat\n"), 0o755); err != nil {
		t.Fatal(err)
	}
	t.Setenv("PATH", dir+string(os.PathListSeparator)+os.Getenv("PATH"))
	s := newSam(strings.NewReader(""), io.Discard, io.Discard)
	if s.termprog != "" {
		t.Fatalf("termprog = %q, want empty so bootterm uses the default", s.termprog)
	}
	s.startup("", false, nil, nil)
	if s.in == nil || s.out == nil {
		t.Fatal("bootterm did not wire s.in/s.out")
	}
	s.out.(*os.File).Close()
	s.in.(*os.File).Close()
}

// TestBoottermLocalExecFail covers the local bootterm exec-failure path.
func TestBoottermLocalExecFail(t *testing.T) {
	var errb bytes.Buffer
	s := newSam(strings.NewReader(""), io.Discard, &errb)
	s.termprog = "/no/such/samterm"
	if got := catchExit(t, func() { s.startup("", false, nil, nil) }); got != "damn" {
		t.Errorf("exit = %q, want damn", got)
	}
	if !strings.Contains(errb.String(), "can't exec") {
		t.Errorf("missing exec error on stderr: %q", errb.String())
	}
}

// TestConnectRemote exercises the remote path (io.c connectto + bootterm with a
// machine): RX and termprog are harmless stand-ins that exit immediately, so
// connectto forks the "ssh" and bootterm forks "samterm", waits for it, and
// exits(0). s.err is a buffer so stderrFile's fallback branch is covered.
func TestConnectRemote(t *testing.T) {
	defer func(old string) { RX = old }(RX)
	RX = "true"
	s := newSam(strings.NewReader(""), io.Discard, io.Discard)
	s.rsamname = "sam"
	s.termprog = "true"
	if got := catchExit(t, func() { s.startup("host", false, nil, nil) }); got != "0" {
		t.Errorf("exit = %q, want 0", got)
	}
}

// TestConnectExecFail covers connectto's exec-failure path.
func TestConnectExecFail(t *testing.T) {
	defer func(old string) { RX = old }(RX)
	RX = "/no/such/ssh"
	var errb bytes.Buffer
	s := newSam(strings.NewReader(""), io.Discard, &errb)
	s.rsamname = "sam"
	if got := catchExit(t, func() { s.connectto("host", nil) }); got != "exec" {
		t.Errorf("exit = %q, want exec", got)
	}
}

// TestBoottermRemoteExecFail covers the remote bootterm exec-failure path: a
// good connectto (RX exits) followed by a missing samterm.
func TestBoottermRemoteExecFail(t *testing.T) {
	var errb bytes.Buffer
	s := newSam(strings.NewReader(""), io.Discard, &errb)
	s.termprog = "/no/such/samterm"
	r0, w0, _ := os.Pipe()
	r1, w1, _ := os.Pipe()
	defer func() { r0.Close(); w0.Close(); r1.Close(); w1.Close() }()
	s.remotefd0, s.remotefd1 = r0, w1
	if got := catchExit(t, func() { s.bootterm("host") }); got != "damn" {
		t.Errorf("exit = %q, want damn", got)
	}
}

// TestRcvEOFInHeader covers rcv when the connection drops after the type byte,
// before the 2-byte count is complete: rcv returns 0.
func TestRcvEOFInHeader(t *testing.T) {
	s := newSam(bytes.NewReader([]byte{0x02}), io.Discard, io.Discard)
	if r := s.rcv(); r != 0 {
		t.Errorf("rcv after partial header = %d, want 0", r)
	}
}

// TestRcvEOFInPayload covers rcv when the connection drops mid-payload:
// a header promising 2 bytes followed by only 1, then EOF. rcv returns 0.
func TestRcvEOFInPayload(t *testing.T) {
	s := newSam(bytes.NewReader([]byte{0x02, 2, 0, 'x'}), io.Discard, io.Discard)
	if r := s.rcv(); r != 0 {
		t.Errorf("rcv after partial payload = %d, want 0", r)
	}
}

// TestLoadflistNewline covers loadflist's inline-list collection stopping at a newline:
// everything before the newline becomes the file name.
func TestLoadflistNewline(t *testing.T) {
	s := newSam(strings.NewReader(""), io.Discard, io.Discard)
	r := s.loadflist([]rune(" foo\nbar"))
	if got := string(s.genstr); got != "foo" {
		t.Errorf("genstr = %q, want %q", got, "foo")
	}
	if r != 'f' {
		t.Errorf("loadflist returned %q, want 'f'", r)
	}
}

// TestParseArgsTermFlags checks that -a and -W accumulate into termargs in
// order (termFlag.Set): -a is a bool (no value), -W takes the window size.
func TestParseArgsTermFlags(t *testing.T) {
	s := newSam(strings.NewReader(""), io.Discard, io.Discard)
	files := s.parseArgs([]string{"-a", "-W", "100", "file.txt"})
	if want := []string{"-a", "-W", "100"}; !slices.Equal(s.termargs, want) {
		t.Errorf("termargs = %v, want %v", s.termargs, want)
	}
	if !slices.Equal(files, []string{"file.txt"}) {
		t.Errorf("files = %v, want [file.txt]", files)
	}
}

// TestConnectArgv checks the ssh command connectto builds for remote operation.
func TestConnectArgv(t *testing.T) {
	got := connectArgv("host.example", "sam", []string{"a.txt", "b.txt"})
	want := []string{"ssh", "host.example", "sam", "-R", "a.txt", "b.txt"}
	if !slices.Equal(got, want) {
		t.Errorf("connectArgv = %v, want %v", got, want)
	}
	// -s overrides the remote sam name; no files.
	got = connectArgv("h", "mysam", nil)
	want = []string{"ssh", "h", "mysam", "-R"}
	if !slices.Equal(got, want) {
		t.Errorf("connectArgv = %v, want %v", got, want)
	}
}

// TestParseArgsRemote checks that -r/-s populate machine/rsamname.
func TestParseArgsRemote(t *testing.T) {
	s := newSam(strings.NewReader(""), io.Discard, io.Discard)
	files := s.parseArgs([]string{"-r", "host", "-s", "mysam", "foo.txt"})
	if s.machine != "host" {
		t.Errorf("machine = %q, want host", s.machine)
	}
	if s.rsamname != "mysam" {
		t.Errorf("rsamname = %q, want mysam", s.rsamname)
	}
	if s.machineName() != "host" {
		t.Errorf("machineName = %q, want host", s.machineName())
	}
	if !slices.Equal(files, []string{"foo.txt"}) {
		t.Errorf("files = %v, want [foo.txt]", files)
	}

	// With no -s, the remote sam name defaults to "sam"; no -r means local.
	s2 := newSam(strings.NewReader(""), io.Discard, io.Discard)
	s2.parseArgs([]string{"x.txt"})
	if s2.rsamname != "sam" {
		t.Errorf("default rsamname = %q, want sam", s2.rsamname)
	}
	if s2.machineName() != "" {
		t.Errorf("machineName = %q, want empty (local)", s2.machineName())
	}
}

// TestPanic drives a fatal panic the way it is actually reachable in practice:
// a malformed protocol message in -R (downloaded) mode.
// It covers the panic body, the downloaded ("wasd") branch
// that writes the message straight to stderr,
// and the recover that repanics the samExit from exits.
func TestPanic(t *testing.T) {
	t.Chdir(t.TempDir())
	var out, errb bytes.Buffer
	// A message whose type byte is > TMAX makes inmesg call s.panic("inmesg").
	status := Run([]string{"-R"}, bytes.NewReader([]byte{0xff, 0, 0}), &out, &errb)
	if status != "abort" {
		t.Errorf("status = %q, want %q", status, "abort")
	}
	// -R startup set downloaded, so panic clears it and dprints to stderr, and
	// because it was downloaded it also writes the message straight to stderr
	// (C's fprint(2,...)): two copies.
	if n := strings.Count(errb.String(), "sam: panic: inmesg"); n != 2 {
		t.Errorf("panic message appeared %d times on stderr, want 2:\n%s", n, errb.String())
	}
}

// TestPanicReentrant checks that a re-entrant panic (panicking already set)
// returns to its caller without printing or exiting, mirroring C where the if is false
// and panic falls off the end.
// It is unclear whether this is correct behavior but it is at least consistent with C.
func TestPanicReentrant(t *testing.T) {
	var out, errb bytes.Buffer
	s := newSam(strings.NewReader(""), &out, &errb)
	s.panicking = 1
	defer func() {
		if r := recover(); r != nil {
			t.Fatalf("re-entrant panic should return, got panic %v", r)
		}
	}()
	s.panic("again")
	if errb.Len() != 0 {
		t.Errorf("re-entrant panic wrote to stderr: %q", errb.String())
	}
}

// errWriter fails every write, modeling stderr breaking mid-panic.
type errWriter struct{}

func (errWriter) Write(p []byte) (int, error) { return 0, errors.New("write failed") }

// TestPanicWriteError covers C's setjmp(mainloop) in panic:
// the panic message's dprint goes through Write, which here fails and panics
// (syserror -> error_s -> hiccough -> mainloopJump).
// panic recovers that and returns instead of letting it resume the command loop,
// so the rescue/exits at the end are skipped.
func TestPanicWriteError(t *testing.T) {
	var out bytes.Buffer
	s := newSam(strings.NewReader(""), &out, errWriter{})
	defer func() {
		if r := recover(); r != nil {
			t.Fatalf("a failed panic dprint should be recovered, got panic %v", r)
		}
	}()
	s.panic("boom")
	if s.rescuing != 0 {
		t.Errorf("rescue ran; expected the failed dprint to divert before it")
	}
}

// errCloser fails Close, exercising sam.c Close()'s syserror path.
type errCloser struct{}

func (errCloser) Close() error { return errors.New("close failed") }

// TestCloseError covers s.Close's error branch:
// a failed close routes through syserror -> error_s -> hiccough,
// which unwinds with mainloopJump.
func TestCloseError(t *testing.T) {
	var out, errb bytes.Buffer
	s := newSam(strings.NewReader(""), &out, &errb)
	defer func() {
		if _, ok := recover().(mainloopJump); !ok {
			t.Fatal("a failed Close should panic mainloopJump")
		}
	}()
	s.Close(errCloser{})
	t.Fatal("Close did not panic")
}

// TestArgs exercises flag parsing and the startup paths that the txtar cases
// don't reach (no file argument, samterm-only flags, EARGF flags, unknown
// flag, "--", and -R startup hitting EOF -> rescue).
func TestArgs(t *testing.T) {
	cases := [][]string{
		{"-d"},                  // no files: opens a nameless file
		{"-d", "--"},            // end-of-flags marker
		{"-a", "-d"},            // samterm option, ignored
		{"-t", "samterm", "-d"}, // EARGF flag (separate arg)
		{"-ssam", "-d"},         // EARGF flag (attached)
		{"-r", "host", "-d"},    // machine flag (connect is a no-op here)
		{"-z", "-d"},            // unknown flag -> usage
		{"-R"},                  // downloaded startup, then EOF -> rescue
	}
	for _, args := range cases {
		t.Run(strings.Join(args, "_"), func(t *testing.T) {
			t.Chdir(t.TempDir())
			var out, errb bytes.Buffer
			// Empty stdin: -d reaches EOF immediately; -R rescues on EOF.
			Run(args, strings.NewReader(""), &out, &errb)
		})
	}
}

// TestCd exercises a bare cd (which targets $HOME) deterministically by making
// $HOME the current directory, so the working directory does not actually move.
func TestCd(t *testing.T) {
	dir := t.TempDir()
	t.Chdir(dir)
	t.Setenv("HOME", dir)
	if err := os.WriteFile("input", []byte("alpha\n"), 0644); err != nil {
		t.Fatal(err)
	}
	var out, errb bytes.Buffer
	Run([]string{"-d", "input"}, strings.NewReader("cd\n,p\n"), &out, &errb)
}
