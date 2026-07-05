package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"image"
	"io"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"testing"
	"time"

	"9fans.net/plan9port/draw"
	"9fans.net/plan9port/internal/drawtest"
	"9fans.net/plan9port/internal/llvmcover"
	"9fans.net/plan9port/internal/sam"
	"9fans.net/plan9port/internal/testar"
)

// osamtermcover is the LLVM-instrumented C samterm; cov registers the
// -uncoverc flag. When -uncoverc=FILE is given, the oracle (old) runs use the
// instrumented binary and their profiles are merged into a C-coverage report.
var (
	osamtermcover = filepath.Join(plan9root, "src/cmd/osamterm/o.osamterm.cover")
	cov           = llvmcover.New(osamtermcover, "")
)

func TestMain(m *testing.M) {
	// The goldens are rendered with the built-in default font. Clear $font so a
	// developer's environment (which may point it at a real font via the font
	// server) does not change the metrics -- and so the C samterm subprocess,
	// which inherits os.Environ(), uses the same default the in-process Go run
	// does. Without this both terminals pick up $font and diverge from the
	// goldens (and, when the font fails to load identically, from each other).
	os.Unsetenv("font")

	// Make the run hermetic with respect to the plumber. samterm's button-2 menu
	// shows the entry as "plumb" when it can open the plumb send port and
	// "(plumb)" when it cannot (menu2hit, plan9.c:plumbstart). The Go port has no
	// plumbing yet (plumbfd is always -1), so it always shows "(plumb)"; the C
	// samterm shows "plumb" whenever a plumber happens to be running in the
	// developer's namespace. That makes the menu's width and glyphs -- and thus
	// the menu2* goldens -- depend on the environment. Point $NAMESPACE at an
	// empty directory so plumbopen always fails and both terminals agree on
	// "(plumb)", matching the goldens.
	ns, err := os.MkdirTemp("", "samterm-ns")
	if err != nil {
		log.Fatal(err)
	}
	os.Setenv("NAMESPACE", ns)

	code := m.Run()
	if err := cov.Finish(); err != nil {
		log.Fatal(err)
	}
	os.RemoveAll(ns)
	os.Exit(code)
}

const plan9root = "/usr/local/plan9"

func init() {
	if os.Getenv("TESTSAMTERM") == "1" {
		main()
		os.Exit(0)
	}
}

// quietStderr returns a writer to use as a subprocess's Stderr (sam, the C
// samterm). Their diagnostics -- e.g. the C terminal's "host read error:
// unexpected eof" on teardown -- are buffered and written to the test log only
// if the test fails, instead of cluttering a passing `go test`. (The in-process
// Go samterm writes nothing to os.Stderr on teardown, so it needs no capture.)
func quietStderr(t *testing.T, who string) io.Writer {
	t.Helper()
	buf := new(safeBuffer)
	t.Cleanup(func() {
		if t.Failed() && buf.Len() > 0 {
			t.Logf("%s stderr:\n%s", who, buf.Bytes())
		}
	})
	return buf
}

// safeBuffer is a bytes.Buffer safe for the concurrent writes os/exec's stderr
// copier goroutine makes.
type safeBuffer struct {
	mu  sync.Mutex
	buf bytes.Buffer
}

func (b *safeBuffer) Write(p []byte) (int, error) {
	b.mu.Lock()
	defer b.mu.Unlock()
	return b.buf.Write(p)
}

func (b *safeBuffer) Len() int {
	b.mu.Lock()
	defer b.mu.Unlock()
	return b.buf.Len()
}

func (b *safeBuffer) Bytes() []byte {
	b.mu.Lock()
	defer b.mu.Unlock()
	return b.buf.Bytes()
}

// updateSettleMS is how long rendering must be quiet (no new frame) for a
// scripted action to be considered settled during -update, when the script
// paces with waitidle to discover each action's settled frame count (see
// pacedScript and devdraw's recordsettle). It is generous so that a momentary
// stall does not record too small a count.
const updateSettleMS = 200

// typeText expands a string into one keystroke command per rune. Spaces and
// tabs are sent via "key" because the "type" command's argument parser strips
// leading whitespace; everything else via "type".
func typeText(s string) []string {
	var cmds []string
	for _, r := range s {
		switch r {
		case ' ':
			cmds = append(cmds, "key space")
		case '\t':
			cmds = append(cmds, "key tab")
		default:
			cmds = append(cmds, "type "+string(r))
		}
	}
	return cmds
}

// parseSteps turns a scenario's input section (one high-level command per line)
// into a flat list of devdraw input commands. A "type TEXT" line expands to one
// keystroke per rune (typeText); every other line (key/move/button/click/
// resize/...) passes through unchanged.
func parseSteps(input []byte) []string {
	var cmds []string
	for line := range strings.Lines(string(input)) {
		line = strings.TrimSpace(line)
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		if text, ok := strings.CutPrefix(line, "type "); ok {
			cmds = append(cmds, typeText(unescape(text))...)
		} else {
			cmds = append(cmds, line)
		}
	}
	return cmds
}

// unescape resolves the escapes used in scenario text, which is Go
// quoted-string syntax: the txtar files hold one command per line, so tabs and
// newlines inside typed text are escaped.
func unescape(s string) string {
	t, err := strconv.Unquote(`"` + s + `"`)
	if err != nil {
		panic("bad scenario text: " + s)
	}
	return t
}

// pacedScript turns a command list into a DEVDRAW_TEST_INPUT script. Between
// every action it waits for the output to reach a known state before delivering
// the next, so an asynchronous action -- a resize, a mouse move -- lands at a
// deterministic frame boundary instead of racing the client's redraws.
//
// With frames != nil it emits "waitframe N" using those absolute frame counts,
// for a reproducible run. With frames == nil it emits "waitidle", used by
// -update to discover the counts: devdraw records the frame count at each settle
// point (see recordsettle), and the harness writes them back as the waitframe
// counts. frames must have len(cmds)+1 entries: a settle point before the first
// command (after startup) and one after each command.
func pacedScript(cmds []string, frames []int) string {
	var b strings.Builder
	wait := func(i int) {
		if frames != nil {
			fmt.Fprintf(&b, "waitframe %d\n", frames[i])
		} else {
			fmt.Fprintf(&b, "waitidle %d\n", updateSettleMS)
		}
	}
	wait(0)
	for i, c := range cmds {
		b.WriteString(c)
		b.WriteByte('\n')
		wait(i + 1)
	}
	return b.String()
}

// readFile returns the contents of name, or nil if it cannot be read.
func readFile(name string) []byte {
	data, _ := os.ReadFile(name)
	return data
}

// parseInts parses whitespace-separated integers (the waitframes golden).
func parseInts(data []byte) []int {
	var out []int
	for _, f := range strings.Fields(string(data)) {
		n, err := strconv.Atoi(f)
		if err != nil {
			return nil
		}
		out = append(out, n)
	}
	return out
}

// formatInts formats counts as a space-separated line (for the waitframes golden).
func formatInts(counts []int) []byte {
	var b strings.Builder
	for i, n := range counts {
		if i > 0 {
			b.WriteByte(' ')
		}
		fmt.Fprintf(&b, "%d", n)
	}
	b.WriteByte('\n')
	return []byte(b.String())
}

func TestDraw(t *testing.T) {
	files, err := filepath.Glob("testdata/draw/*.txtar")
	if err != nil {
		t.Fatal(err)
	}
	const samExe = plan9root + "/bin/sam"
	if _, err := os.Stat(samExe); err != nil {
		t.Fatal(err)
	}
	for _, file := range files {
		name := strings.TrimSuffix(filepath.Base(file), ".txtar")
		t.Run(name, func(t *testing.T) {
			ar := testar.Open(t, file)
			if err != nil {
				t.Fatal(err)
			}
			steps := parseSteps(ar.ReadFile("input"))

			// A per-process temp dir (so concurrent runs -- e.g. under `stress`
			// -- do not clobber each other's files), shared by the Go and C runs
			// of this scenario. sam is invoked with this as its working directory
			// and a relative filename (see runLocal/runSam), so the path it
			// echoes into the window is the stable "f" rather than the dir's
			// random name, keeping the golden reproducible.
			dir := t.TempDir()

			// The file sam loads. A scenario may supply its own "file" section
			// (e.g. many lines, to exercise scrolling); otherwise a short default.
			content := []byte("hello world\n")
			if i, _ := ar.Find("file"); i >= 0 {
				content = ar.ReadFile("file")
			}
			// The basename sam loads, normally the stable "f". A scenario may
			// supply its own "filename" section (e.g. a very long name to exercise
			// the menu's width/truncation rendering).
			fname := "f"
			if i, _ := ar.Find("filename"); i >= 0 {
				fname = strings.TrimSpace(string(ar.ReadFile("filename")))
			}
			file := filepath.Join(dir, fname)
			if err := os.WriteFile(file, content, 0666); err != nil {
				t.Fatal(err)
			}

			// The Go run discovers the per-action settled frame counts under
			// -update (pacing with waitidle); otherwise it replays them with
			// waitframe, like the C run, so both stay synchronized to the same
			// frame boundaries (see pacedScript). The counts are the "waitframes"
			// golden.
			var frameCounts []int
			if !*update {
				frameCounts = parseInts(ar.ReadFile("waitframes"))
				if len(frameCounts) != len(steps)+1 {
					t.Fatalf("waitframes golden has %d counts, want %d (regenerate with -update)", len(frameCounts), len(steps)+1)
				}
			}
			goScript := filepath.Join(dir, "goscript")
			if err := os.WriteFile(goScript, []byte(pacedScript(steps, frameCounts)), 0666); err != nil {
				t.Fatal(err)
			}

			golden := "testdata/draw/" + name + ".gif"
			// Every frame is compared: devdraw writes one per drawing operation
			// (see testframe in devdraw.c), a deterministic sequence identical for
			// the Go and C terminals, so the whole animation is checked rather
			// than only a settled end state.
			//
			// Run the Go samterm in-process (runLocal) so it counts toward
			// coverage; the C samterm can only run as a subprocess (runSam).
			frames, goH, goT, goOps, goCounts := runLocal(t, file, goScript)
			if *update {
				// The goldens must record what a replay-paced run produces:
				// pacing can change behavior at the margins (e.g. how a host
				// round-trip interleaves with the final input), and both the
				// normal Go run and the C run replay with waitframe. So use the
				// waitidle discovery run only for the counts, then re-run with
				// those counts and record the replayed frames, ops and streams
				// as the goldens.
				frameCounts = goCounts
				ar.WriteFile("waitframes", formatInts(goCounts))
				replayScript := filepath.Join(dir, "goreplay")
				if err := os.WriteFile(replayScript, []byte(pacedScript(steps, frameCounts)), 0666); err != nil {
					t.Fatal(err)
				}
				frames, goH, goT, goOps, _ = runLocal(t, file, replayScript)
				ar.WriteFile("drawops", []byte(goOps+"\n"))
				ar.Save(t)
			}
			drawtest.Check(t, frames, golden, *update)
			// The "drawops" golden is the sequence of devdraw opcodes the run
			// produced (see readDrawops). Comparing it -- not just the rendered
			// frames -- catches a divergence in the draw protocol even when it
			// does not change the visible pixels.
			wantOps := strings.TrimSpace(string(ar.ReadFile("drawops")))
			if goOps != wantOps {
				t.Errorf("Go drawops differ from golden:\n  have %s\n  want %s", goOps, wantOps)
			}

			if haveOld() {
				t.Run("old", func(t *testing.T) {
					// The C run always replays the (now known) frame counts with
					// waitframe, synchronized to the same boundaries as the golden.
					cScript := filepath.Join(dir, "cscript")
					if err := os.WriteFile(cScript, []byte(pacedScript(steps, frameCounts)), 0666); err != nil {
						t.Fatal(err)
					}
					frames, cH, cT, cOps := runSam(t, oldExe(), file, cScript)
					drawtest.Check(t, frames, golden, false)
					if cOps != wantOps {
						t.Errorf("C drawops differ from golden:\n  have %s\n  want %s", cOps, wantOps)
					}

					// Confirm the raw wire protocol is byte-identical between the
					// Go and C runs (after normalizing the Text tokens, which are
					// integers in Go and pointers in C). Both runs drive their own
					// `sam -R`; matching streams prove the two sams behaved
					// identically (ruling out sam-side nondeterminism) and that
					// both terminals sent sam the same T messages -- so any
					// remaining frame/drawlog divergence is purely in the draw
					// layer, not the protocol.
					checkStream(t, "T(samterm->sam)", normProto(goT, 'T'), normProto(cT, 'T'))
					checkStream(t, "H(sam->samterm)", normProto(goH, 'H'), normProto(cH, 'H'))
				})
			}
		})
	}
}

// waitDone blocks until devdraw writes donefile -- its end-of-script marker, set
// once the whole input script has been delivered and the client has settled --
// so the caller knows every frame has been written before it reads them. The
// scripted run never quits sam, so without this marker there is no other signal
// that the run is complete. Every frame is then compared: devdraw writes one per
// drawing operation (see testframe in devdraw.c), a deterministic sequence that
// is identical for the Go and C terminals, so no settle-point picking is needed.
func waitDone(donefile string) {
	deadline := time.Now().Add(15 * time.Second)
	for time.Now().Before(deadline) {
		if _, err := os.Stat(donefile); err == nil {
			return
		}
		time.Sleep(5 * time.Millisecond)
	}
}

// readDrawops reads devdraw's draw-op trace ($DEVDRAW_DRAWLOG) and returns the
// opcode letters as one string, e.g. "bAddsd...", with any run of 5 or more
// identical opcodes collapsed to "x{n}" (so "ddddddddd" becomes "d{9}") to keep
// the golden line readable. The '#' comment lines (the "# write N bytes" and
// "# frame N" markers) are skipped, and two opcodes are dropped:
//
//   - 'v' (visible flush): emitted whenever the event loop happens to go idle,
//     so when a burst of draws arrives in two read chunks instead of one an
//     extra 'v' lands in the middle. It marks no drawing and no longer drives a
//     frame (devdraw frames per drawing op now), so its jittery position is
//     irrelevant -- drop it.
//   - 'f' (free image): the Go draw library frees its images at shutdown (one
//     'f' each) while the C library leaves that to process exit, so the free
//     ops differ in count and position between the two.
//
// Every other opcode -- the actual drawing -- is identical for the Go and C
// runs. Comparing this string (and against the golden) checks that the two
// terminals drive devdraw with the same draw protocol, not just that the
// rendered frames match.
func readDrawops(file string) string {
	data, _ := os.ReadFile(file)
	var ops []byte
	for _, line := range strings.Split(string(data), "\n") {
		// Real opcode lines are exactly one character (devdraw writes "%c\n").
		// Skip everything else: the "# write/# frame" comment lines, and any
		// diagnostic devdraw prints (e.g. "serveproc: eof during message" when the
		// host pipe closes at shutdown), whose leading byte must not be mistaken
		// for an opcode.
		if len(line) != 1 || line[0] == 'f' || line[0] == 'v' {
			continue
		}
		ops = append(ops, line[0])
	}
	var b strings.Builder
	for i := 0; i < len(ops); {
		j := i
		for j < len(ops) && ops[j] == ops[i] {
			j++
		}
		if n := j - i; n >= 5 {
			fmt.Fprintf(&b, "%c{%d}", ops[i], n)
		} else {
			b.Write(ops[i:j])
		}
		i = j
	}
	return b.String()
}

// syncBuf is a bytes.Buffer safe for one writer goroutine (the stream copier)
// and a concurrent Bytes() snapshot from the test goroutine.
type syncBuf struct {
	mu sync.Mutex
	b  bytes.Buffer
}

func (s *syncBuf) Write(p []byte) (int, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.b.Write(p)
}

func (s *syncBuf) Bytes() []byte {
	s.mu.Lock()
	defer s.mu.Unlock()
	return append([]byte(nil), s.b.Bytes()...)
}

// normProto zeroes the 8-byte Text token that some protocol messages carry, so
// that the Go samterm's integer tokens and the C samterm's real pointers (which
// sam echoes back) normalize to the same bytes. Every message is framed as
// type(1) + count(2, little-endian) + count data bytes. The token-bearing
// messages are, for kind 'H' (sam->samterm) and 'T' (samterm->sam):
//
//	H Hbindname(1):     short tag + vlong token   -> token at data offset 2
//	T Tstartcmdfile(1): vlong token               -> token at data offset 0
//	T Tstartfile(5):    short tag + vlong token   -> token at data offset 2
//	T Tstartnewfile(11): vlong token              -> token at data offset 0
//
// Tstartcmdfile and Tstartnewfile go out via outTv (vlong only), so their token
// is at offset 0 -- unlike Tstartfile (outTsv: short tag first).
func normProto(b []byte, kind byte) []byte {
	out := append([]byte(nil), b...)
	for i := 0; i+3 <= len(out); {
		typ := out[i]
		cnt := int(binary.LittleEndian.Uint16(out[i+1:]))
		data := i + 3
		off, ok := 0, false
		switch {
		case kind == 'H' && typ == byte(sam.Hbindname):
			off, ok = 2, true
		case kind == 'T' && typ == byte(sam.Tstartcmdfile):
			off, ok = 0, true
		case kind == 'T' && typ == byte(sam.Tstartfile):
			off, ok = 2, true
		case kind == 'T' && typ == byte(sam.Tstartnewfile):
			off, ok = 0, true
		}
		if ok && data+off+8 <= len(out) {
			for j := 0; j < 8; j++ {
				out[data+off+j] = 0
			}
		}
		i = data + cnt
	}
	return out
}

// checkStream fails the test if the Go and C normalized protocol streams differ,
// reporting the byte offset of the first divergence with a little surrounding
// context to make protocol debugging tractable.
func checkStream(t *testing.T, label string, go_, c []byte) {
	t.Helper()
	if bytes.Equal(go_, c) {
		return
	}
	n := len(go_)
	if len(c) < n {
		n = len(c)
	}
	at := n
	for i := 0; i < n; i++ {
		if go_[i] != c[i] {
			at = i
			break
		}
	}
	ctx := func(b []byte) string {
		lo := at - 8
		if lo < 0 {
			lo = 0
		}
		hi := at + 64
		if hi > len(b) {
			hi = len(b)
		}
		return fmt.Sprintf("% x", b[lo:hi])
	}
	t.Errorf("%s streams differ (len Go=%d C=%d, first diff at byte %d)\n  Go: %s\n  C:  %s",
		label, len(go_), len(c), at, ctx(go_), ctx(c))
}

// runLocal runs the Go samterm IN-PROCESS, driven by a real `sam -R` host
// subprocess and the devdraw input script, returning the rendered frames and
// the raw H (sam->samterm) and T (samterm->sam) protocol streams captured up to
// settlement (see normProto and TestDraw's stream check).
//
// Running samterm in-process (rather than as a subprocess, as runSam does) is
// what lets `go test -cover` attribute the work to the samterm package. sam in
// -R mode speaks the wire protocol on its stdin/stdout instead of launching its
// own samterm, so we hand its stdout to the in-process terminal's host reader
// and its stdin to the terminal's host writer -- exactly the streams sam would
// otherwise wire to a forked samterm.
func runLocal(t *testing.T, file, script string) ([]image.Image, []byte, []byte, string, []int) {
	t.Helper()
	img := filepath.Join(t.TempDir(), "img")
	if d := os.Getenv("GOIMG"); d != "" {
		img = d
	}
	donefile := filepath.Join(filepath.Dir(img), "done")
	drawlogfile := filepath.Join(filepath.Dir(img), "drawlog")
	settlefile := filepath.Join(filepath.Dir(img), "settle")
	os.Remove(donefile) // clear markers from a prior run sharing this dir
	os.Remove(settlefile)
	// The run's environment: built as a slice, not with t.Setenv, so parallel
	// tests can configure their own displays independently. draw.Init uses it
	// for its lookups and for the devdraw subprocess (later entries override
	// earlier ones).
	//
	// NOCOALESCE disables devdraw's mouse-move coalescing so every scripted
	// event is delivered distinctly: the frame sequence then depends only on
	// the input, not on how fast the client drained the mouse queue.
	// NOFINALIZER disables the Go draw library's image GC finalizer so a stray
	// garbage collection cannot free an image (and emit draws) at a
	// nondeterministic point. Together these make the Go and C runs produce
	// the same frames, so the goldens can hold the full settled sequence
	// rather than only the final frame. SETTLE is where waitidle records its
	// settle points (only under -update, whose script paces with waitidle); a
	// normal waitframe run leaves the file empty.
	env := append(os.Environ(),
		"DEVDRAW="+filepath.Join(plan9root, "bin/devdraw"),
		"DEVDRAW_TEST_MEMORY=1",
		"DEVDRAW_TEST_INPUT="+script,
		"DEVDRAW_TEST_OUTPUT="+img,
		"DEVDRAW_TEST_NOCOALESCE=1",
		"DRAW_TEST_NOFINALIZER=1",
		"DEVDRAW_TEST_DONE="+donefile,
		"DEVDRAW_DRAWLOG="+drawlogfile,
		"DEVDRAW_TEST_SETTLE="+settlefile,
	)

	// sam -R: the host speaks the protocol on stdin/stdout. sam writes
	// H-messages to its stdout (-> samterm host reader) and reads T-messages
	// from its stdin (<- samterm host writer).
	samOutR, samOutW, _ := os.Pipe() // sam stdout -> samterm host-in
	samInR, samInW, _ := os.Pipe()   // samterm host-out -> sam stdin
	// Run sam in the file's directory with a relative filename, so the path it
	// echoes into the window is the stable "f", not the temp dir's random name.
	cmd := exec.Command(plan9root+"/bin/sam", "-R", filepath.Base(file))
	cmd.Dir = filepath.Dir(file)
	cmd.Env = env
	cmd.Stdin = samInR
	cmd.Stdout = samOutW
	cmd.Stderr = quietStderr(t, "sam")
	cmd.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	if err := cmd.Start(); err != nil {
		t.Fatal(err)
	}
	samOutW.Close()
	samInR.Close()

	// Always capture the raw protocol streams (H = sam->samterm, T =
	// samterm->sam) so TestDraw can confirm the Go and C runs are byte-identical
	// after normalization, isolating any sam-side nondeterminism from samterm's
	// processing. We snapshot the buffers at settlement, when sam and the
	// terminal are both quiescent, so the capture boundary is deterministic.
	hbuf, tbuf := new(syncBuf), new(syncBuf)
	hostr := io.TeeReader(samOutR, hbuf)
	hostw := io.MultiWriter(samInW, tbuf)

	disp, err := draw.Init(nil, "", "sam", "300x200", env)
	if err != nil {
		t.Skipf("no display: %v", err)
	}
	// Clear the screen to white, matching getscreen (plan9.go), which the real
	// samterm runs at startup but NewTerm does not -- so the draw stream matches
	// the C samterm's.
	disp.ScreenImage.Draw(disp.ScreenImage.Clipr, disp.White, nil, draw.ZP)
	// Like the proto test, we intentionally do not shut the in-process terminal
	// down: the draw package's reader goroutines log.Fatal on a closed display,
	// so the idle terminal goroutines and devdraw are left to be reaped at
	// test-binary exit.
	term := NewTerm(disp, hostr, hostw, env)
	term.startup()
	go term.run()

	waitDone(donefile)
	// Snapshot the protocol streams now, while sam and the terminal are idle at
	// settlement -- before the shutdown drain below races hostproc on samOutR.
	hraw, traw := hbuf.Bytes(), tbuf.Bytes()

	// Shut down: closing the terminal's host writer gives sam EOF on its stdin,
	// so it exits cleanly. Drain its output so it never blocks on a full pipe.
	go io.Copy(io.Discard, samOutR)
	samInW.Close()
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	select {
	case <-done:
	case <-time.After(3 * time.Second):
		syscall.Kill(-cmd.Process.Pid, syscall.SIGKILL)
		<-done
	}
	return drawtest.Frames(t, img), hraw, traw, readDrawops(drawlogfile), parseInts(readFile(settlefile))
}

// runSam drives the C samterm (samtermExe, a subprocess -- the only way to run
// the C samterm) from a real `sam -R` host, wired by pipes exactly as runLocal
// wires the in-process Go samterm. Both paths therefore drive the identical
// `sam -R`, so any difference is in the terminal, not in two different sam
// invocations. It returns the rendered frames and the raw H (sam->samterm) and
// T (samterm->sam) protocol streams captured up to settlement, so TestDraw can
// confirm the Go and C runs are byte-identical after normalization.
func runSam(t *testing.T, samtermExe, file, script string) ([]image.Image, []byte, []byte, string) {
	img := filepath.Join(t.TempDir(), "img")
	if d := os.Getenv("COIMG"); d != "" {
		img = d
	}
	t.Helper()
	donefile := filepath.Join(filepath.Dir(img), "done")
	drawlogfile := filepath.Join(filepath.Dir(img), "drawlog")
	os.Remove(donefile) // clear any marker from a prior run sharing this dir

	samHR, samHW, _ := os.Pipe() // sam stdout (H) -> us
	samTR, samTW, _ := os.Pipe() // us -> sam stdin (T)
	otHR, otHW, _ := os.Pipe()   // us -> osamterm stdin (H)
	otTR, otTW, _ := os.Pipe()   // osamterm stdout (T) -> us

	// Run sam in the file's directory with a relative filename (see runLocal).
	sam := exec.Command(plan9root+"/bin/sam", "-R", filepath.Base(file))
	sam.Dir = filepath.Dir(file)
	sam.Stdin = samTR
	sam.Stdout = samHW
	sam.Stderr = quietStderr(t, "sam")
	sam.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	if err := sam.Start(); err != nil {
		t.Fatal(err)
	}
	samTR.Close()
	samHW.Close()

	ot := exec.Command(samtermExe, "-W", "300x200")
	ot.Stdin = otHR
	ot.Stdout = otTW
	ot.Stderr = quietStderr(t, "samterm")
	ot.Env = append(os.Environ(),
		"DEVDRAW="+filepath.Join(plan9root, "bin/devdraw"),
		"DEVDRAW_TEST_MEMORY=1",
		"DEVDRAW_TEST_INPUT="+script,
		"DEVDRAW_TEST_OUTPUT="+img,
		"DEVDRAW_TEST_DONE="+donefile,
		"DEVDRAW_DRAWLOG="+drawlogfile,
		"DEVDRAW_TEST_NOCOALESCE=1", // see runLocal: deterministic per-event frames
		cov.Env(t),                  // per-run LLVM_PROFILE_FILE when -uncoverc is set
	)
	ot.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	if err := ot.Start(); err != nil {
		t.Fatal(err)
	}
	otHR.Close()
	otTW.Close()

	// Always capture the raw protocol streams (the copier goroutines below own
	// samHR/otTR for their whole lives, so every byte is captured in order). The
	// buffers are snapshotted at settlement, when sam and osamterm are quiescent.
	hbuf, tbuf := new(syncBuf), new(syncBuf)
	hw := io.MultiWriter(otHW, hbuf)
	tw := io.MultiWriter(samTW, tbuf)
	go func() { io.Copy(hw, samHR); otHW.Close() }() // H: sam -> osamterm
	go func() { io.Copy(tw, otTR); samTW.Close() }() // T: osamterm -> sam

	// The paced script does not quit sam, so sam never exits on its own. Wait for
	// the run to finish, then shut down: closing the H feed (samHR) gives
	// osamterm host EOF so it exits cleanly (flushing any -uncoverc profile),
	// after which sam sees its stdin close and exits too.
	waitDone(donefile)
	hraw, traw := hbuf.Bytes(), tbuf.Bytes()
	samHR.Close()
	done := make(chan error, 2)
	go func() { done <- ot.Wait() }()
	go func() { done <- sam.Wait() }()
	for i := 0; i < 2; i++ {
		select {
		case <-done:
		case <-time.After(3 * time.Second):
			syscall.Kill(-ot.Process.Pid, syscall.SIGKILL)
			syscall.Kill(-sam.Process.Pid, syscall.SIGKILL)
		}
	}
	if cov.Enabled() {
		time.Sleep(300 * time.Millisecond)
	}
	return drawtest.Frames(t, img), hraw, traw, readDrawops(drawlogfile)
}

func haveOld() bool {
	return true
}

func oldExe() string {
	if cov.Enabled() {
		return osamtermcover
	}
	return filepath.Join(plan9root, "bin/osamterm")
}

// TestGetscreen exercises the option parsing and display setup in getscreen
// directly (main's subprocess entry point is not run in-process under coverage).
func TestGetscreen(t *testing.T) {
	env := append(os.Environ(),
		"DEVDRAW_TEST_MEMORY=1",
		"DEVDRAW_TEST_OUTPUT="+filepath.Join(t.TempDir(), "img"),
		"DEVDRAW="+filepath.Join(plan9root, "bin/devdraw"),
		"tabstop=4",
	)
	term := &Term{env: env, maxtab: 8, hostlock: 1, typestart: -1, typeend: -1, typeesc: -1, kpeekc: -1, plumbfd: -1}
	if err := term.getscreen([]string{"-a", "-W", "300x200"}); err != nil {
		t.Skipf("no display: %v", err)
	}
	if !term.autoindent {
		t.Errorf("autoindent = %v, want true (from -a)", term.autoindent)
	}
	if term.maxtab != 4 {
		t.Errorf("maxtab = %d, want 4 (from tabstop)", term.maxtab)
	}
	if term.display == nil {
		t.Error("disp not set")
	}
}
