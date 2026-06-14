package main

import (
	"bufio"
	"bytes"
	"os"
	"os/exec"
	"path/filepath"
	"syscall"
	"testing"
	"time"

	"9fans.net/go/plan9"
	"9fans.net/plan9port/draw"
	"9fans.net/plan9port/plumb"
)

// TestPlumbformat checks the plan9.c plumbformat port: a "showfile" plumb
// message is rendered into the plumbbuf as the sam command "B <data>\n<addr>\n",
// non-showfile actions and empty/oversized data are rejected.
func TestPlumbformat(t *testing.T) {
	tests := []struct {
		name string
		msg  plumb.Message
		want string // "" means plumbformat should return false
	}{
		{
			name: "showfile with addr",
			msg: plumb.Message{
				Data: []byte("/a/file.go"),
				Attr: &plumb.Attribute{Name: "action", Value: "showfile",
					Next: &plumb.Attribute{Name: "addr", Value: "12"}},
			},
			want: "B /a/file.go\n12\n",
		},
		{
			name: "no action, no addr, trailing newline already present",
			msg:  plumb.Message{Data: []byte("/a/file.go\n")},
			want: "B /a/file.go\n",
		},
		{
			name: "no addr",
			msg:  plumb.Message{Data: []byte("/a/file.go")},
			want: "B /a/file.go\n",
		},
		{
			name: "empty data rejected",
			msg:  plumb.Message{Data: []byte("")},
			want: "",
		},
		{
			name: "non-showfile action rejected",
			msg: plumb.Message{Data: []byte("/a/file.go"),
				Attr: &plumb.Attribute{Name: "action", Value: "edit"}},
			want: "",
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			tm := &Term{}
			ok := tm.plumbformat(&tt.msg, 0)
			if (tt.want != "") != ok {
				t.Fatalf("plumbformat ok=%v, want %v", ok, tt.want != "")
			}
			if !ok {
				return
			}
			got := string(tm.plumbbuf[0].data[:tm.plumbbuf[0].n])
			if got != tt.want {
				t.Errorf("plumbformat = %q, want %q", got, tt.want)
			}
		})
	}
}

// TestExternchar checks io.c's externload/externchar port: loaded plumb text is
// fed out one rune at a time (skipping NULs), the RPlumb bit clears when the
// buffer drains, and a blocked RPlumb yields nothing.
func TestExternchar(t *testing.T) {
	tm := &Term{}
	tm.plumbbuf[0].data[0] = 'B'
	tm.plumbbuf[0].data[1] = '\n'
	tm.plumbbuf[0].n = 2
	tm.externload(0)
	if tm.got&(1<<RPlumb) == 0 {
		t.Fatal("externload did not set RPlumb")
	}
	var got []rune
	for {
		c := tm.externchar()
		if c < 0 {
			break
		}
		got = append(got, rune(c))
	}
	if string(got) != "B\n" {
		t.Errorf("externchar drained %q, want %q", string(got), "B\n")
	}
	if tm.got&(1<<RPlumb) != 0 {
		t.Error("RPlumb still set after draining")
	}

	// A blocked RPlumb source yields nothing even with data loaded.
	tm.externload(0)
	tm.block = 1 << RPlumb
	if c := tm.externchar(); c != -1 {
		t.Errorf("externchar with RPlumb blocked = %d, want -1", c)
	}

	// A multibyte rune and an embedded NUL (skipped).
	tm2 := &Term{}
	copy(tm2.plumbbuf[0].data[:], "é\x00x")
	tm2.plumbbuf[0].n = len("é\x00x")
	tm2.externload(0)
	var runes []rune
	for c := tm2.externchar(); c >= 0; c = tm2.externchar() {
		runes = append(runes, rune(c))
	}
	if string(runes) != "éx" {
		t.Errorf("externchar = %q, want %q", string(runes), "éx")
	}
}

// startPlumber starts a plumber in a fresh name space and points $NAMESPACE at
// it for the test, so the in-process samterm (which mounts the plumber per Term;
// see plumb.Mount) and the test itself share one plumber. It returns once the
// plumb service is posted. The hermetic empty $NAMESPACE that TestMain sets is
// overridden for this test (and restored after by t.Setenv).
func startPlumber(t *testing.T) {
	t.Helper()
	ns, err := os.MkdirTemp("", "samterm-plumb")
	if err != nil {
		t.Fatal(err)
	}
	t.Setenv("NAMESPACE", ns)
	// Minimal rules declaring the ports we use: "edit" (samterm receives showfile
	// messages here) and "samtermtest" (TestPlumbSend reads samterm's forwarded
	// message here). Explicit-Dst messages route to a declared port regardless of
	// matching rules.
	rules := filepath.Join(ns, "rules")
	if err := os.WriteFile(rules, []byte("plumb to edit\nplumb to samtermtest\n"), 0666); err != nil {
		t.Fatal(err)
	}
	cmd := exec.Command(filepath.Join(plan9root, "bin/plumber"), "-f", "-p", rules)
	cmd.Env = os.Environ()
	cmd.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	if err := cmd.Start(); err != nil {
		t.Fatalf("start plumber: %v", err)
	}
	t.Cleanup(func() {
		syscall.Kill(-cmd.Process.Pid, syscall.SIGKILL)
		cmd.Wait()
		os.RemoveAll(ns)
	})
	deadline := time.Now().Add(5 * time.Second)
	for {
		if _, err := os.Stat(filepath.Join(ns, "plumb")); err == nil {
			return
		}
		if time.Now().After(deadline) {
			t.Fatal("plumber did not post its service")
		}
		time.Sleep(10 * time.Millisecond)
	}
}

// goSamterm sets up an in-process Go samterm wired to a protoHost (the test
// plays sam), with the display on the headless test backend. It returns the
// host and the started Term. Plumbing is live: startup runs plumbstart, which
// connects to the plumber in $NAMESPACE.
func goSamterm(t *testing.T) (*protoHost, *Term) {
	t.Helper()
	img := filepath.Join(t.TempDir(), "img")
	t.Setenv("DEVDRAW", filepath.Join(plan9root, "bin/devdraw"))
	t.Setenv("DEVDRAW_TEST_MEMORY", "1")
	t.Setenv("DEVDRAW_TEST_OUTPUT", img)
	disp, err := draw.Init(nil, "", "sam", "600x400")
	if err != nil {
		t.Skipf("no display: %v", err)
	}
	disp.ScreenImage.Draw(disp.ScreenImage.Clipr, disp.White, nil, draw.ZP)
	h2tR, h2tW, _ := os.Pipe()
	t2hR, t2hW, _ := os.Pipe()
	term := NewTerm(disp, h2tR, t2hW)
	term.startup()
	go term.run()
	h := &protoHost{w: h2tW, r: bufio.NewReader(t2hR), img: img}
	h.handshake(t)
	go h.drain()
	return h, term
}

// plumbFile sends a "showfile" plumb message to the given port (addressed by
// explicit Dst so it bypasses rule matching).
func plumbFile(t *testing.T, port, addr, file string) {
	t.Helper()
	fsys, err := plumb.Mount()
	if err != nil {
		t.Fatalf("mount plumber: %v", err)
	}
	send, err := fsys.Open("send", plan9.OWRITE)
	if err != nil {
		t.Fatalf("open send: %v", err)
	}
	defer send.Close()
	m := &plumb.Message{
		Src: "test", Dst: port, Type: "text",
		Attr: &plumb.Attribute{Name: "action", Value: "showfile",
			Next: &plumb.Attribute{Name: "addr", Value: addr}},
		Data: []byte(file),
	}
	if err := m.Send(send); err != nil {
		t.Fatalf("plumb send: %v", err)
	}
}

// settle waits until the terminal has emitted no new frame for ~100ms and
// returns the frame count.
func settle(h *protoHost) int {
	last, stable := h.nframe(), 0
	for stable < 100 {
		time.Sleep(time.Millisecond)
		if n := h.nframe(); n != last {
			last, stable = n, 0
		} else {
			stable++
		}
	}
	return last
}

// TestPlumbRecv drives the receive path: a "showfile" plumb message arrives on
// the edit port, and samterm (plumbproc -> waitforio's RPlumb case -> externload
// -> the RPlumb block in run -> typing/externchar) types the formatted "B
// file\naddr" command into the command window. The host here is the proto stub,
// so sam never executes the command (no window opens, no sweeptext), but the
// terminal's full receive-and-type path runs and draws the echoed command --
// observed as new frames.
func TestPlumbRecv(t *testing.T) {
	startPlumber(t)
	h, _ := goSamterm(t)
	before := settle(h)
	plumbFile(t, "edit", "3", "/some/plumbed/file.txt")
	after := settle(h)
	if after <= before {
		t.Fatalf("plumb produced no new frames (%d -> %d): receive path not exercised", before, after)
	}
}

// TestPlumbSend drives the in-process Go samterm to forward a plumb message
// (mesg.go hplumb / plan9.go plumbstart's send port): the host sends Hplumb with
// a serialized plumb message addressed to a private port, and samterm re-sends
// it to the plumber, which routes it to that port for the test to read back.
func TestPlumbSend(t *testing.T) {
	startPlumber(t)

	fsys, err := plumb.Mount()
	if err != nil {
		t.Fatalf("mount plumber: %v", err)
	}
	rfid, err := fsys.Open("samtermtest", plan9.OREAD)
	if err != nil {
		t.Fatalf("open recv port: %v", err)
	}

	h, term := goSamterm(t)
	if term.plumbsendfid == nil {
		t.Fatal("samterm did not open the plumb send port")
	}

	// sendPlumb frames a serialized plumb message as Hplumb for samterm to forward.
	sendPlumb := func(data string) {
		var buf bytes.Buffer
		(&plumb.Message{Src: "sam", Dst: "samtermtest", Type: "text", Data: []byte(data)}).Send(&buf)
		h.send(Hplumb, pShort(buf.Len()))
		h.w.Write(buf.Bytes())
	}
	// recvPlumb reads the next message routed back to our port (one persistent
	// reader: a plumb fid returns one message per read, so a shared bufio keeps
	// message framing across reads).
	pr := bufio.NewReader(rfid)
	recvPlumb := func() string {
		done := make(chan string, 1)
		go func() {
			var m plumb.Message
			if m.Recv(pr) == nil {
				done <- string(m.Data)
			} else {
				done <- "<recv error>"
			}
		}()
		select {
		case s := <-done:
			return s
		case <-time.After(5 * time.Second):
			t.Fatal("timed out waiting for a forwarded plumb message")
			return ""
		}
	}

	sendPlumb("hello world")
	if got := recvPlumb(); got != "hello world" {
		t.Fatalf("plumber received %q, want %q", got, "hello world")
	}

	// A malformed Hplumb (not a serializable plumb message) is logged and dropped
	// (hplumb's unpack-error path), not forwarded. Send a good message after it
	// and confirm only the good one arrives -- proving the bad one was consumed
	// without desyncing the stream.
	h.send(Hplumb, pShort(len("garbage\n")))
	h.w.Write([]byte("garbage\n"))
	sendPlumb("after bad")
	if got := recvPlumb(); got != "after bad" {
		t.Fatalf("after malformed plumb, received %q, want %q", got, "after bad")
	}
}
