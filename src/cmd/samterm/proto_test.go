package main

import (
	"bufio"
	"encoding/binary"
	"flag"
	"image"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
	"testing"
	"time"

	"9fans.net/plan9port/draw"
	"9fans.net/plan9port/internal/drawtest"
	"9fans.net/plan9port/internal/sam"
	"9fans.net/plan9port/internal/testar"
)

var update = flag.Bool("update", false, "update committed goldens in testdata")

// These tests drive samterm with the test process itself playing the host
// (sam), reading and writing wire-protocol messages directly over a pipe. This
// is the "connected to the test binary" mode: it needs no sam. Each scenario is
// a testdata/proto/<name>.txtar (comment = description, "input" section = host
// operations, one per line). Every frame the terminal renders is checked
// against a committed golden GIF (testdata/proto/<name>.gif) and, when
// bin/osamterm is present, against the C samterm driven by the identical script.
//
// Determinism: devdraw writes one image frame per drawing operation (see
// testframe in devdraw.c), so the whole frame sequence is deterministic and
// identical for the Go and C terminals -- there is no jittery, flush-count-
// dependent capture to settle on. The host paces purely on that frame count: a
// normal run replays the per-step counts recorded in the "waitframes" golden
// (waitFrame), which -update discovers by settling (waitIdle). With a fixed
// 300x200 window, fixed dpi and the built-in default font, the frame sequences
// are reproducible and identical between the Go and C samterm.

// protoHost speaks the wire protocol to a samterm over r/w.
type protoHost struct {
	w      io.Writer
	r      *bufio.Reader
	img    string // image output directory
	extern string // legacy extern FIFO path, for the "extern" op ("" if unused)
}

// nframe returns how many window frames samterm has emitted so far. devdraw also
// writes a cursor frame (img.cursor.N) per window frame; those are excluded here
// (the pattern requires a digit after the dot) so the waitframes counts stay in
// terms of drawing operations, unaffected by cursor capture.
func (h *protoHost) nframe() int {
	files, _ := filepath.Glob(h.img + ".[0-9]*")
	return len(files)
}

// drain consumes and discards everything the terminal sends after the
// handshake. The host drives the terminal and never reads its replies (frames,
// not messages, are what we synchronize on), so without a drainer a reply the
// terminal happened to send would eventually fill the pipe and block it.
func (h *protoHost) drain() {
	io.Copy(io.Discard, h.r)
}

// frametimeout bounds how long waitFrame waits for the terminal to reach an
// expected frame count; a count that never arrives means a real divergence or a
// stale waitframes golden, not something to wait out.
const frametimeout = 10 * time.Second

// waitFrame blocks until the terminal has written at least n image frames, the
// count recorded in the waitframes golden for this step. Because devdraw writes
// one frame per drawing operation, this is an exact, timing-independent
// synchronization: once the count is reached, every frame the step produced is
// on disk. A step that drew nothing leaves the count unchanged, so waitFrame
// returns at once.
func (h *protoHost) waitFrame(t *testing.T, n int) {
	t.Helper()
	deadline := time.Now().Add(frametimeout)
	for h.nframe() < n {
		if time.Now().After(deadline) {
			t.Fatalf("proto: waitframe %d timed out (have %d)", n, h.nframe())
		}
		time.Sleep(time.Millisecond)
	}
}

// waitIdle blocks until the terminal has emitted no new frame for ms
// milliseconds and returns the resulting frame count. It is used only to
// discover the waitframes counts during -update; normal runs replay them with
// waitFrame. A step that changes nothing on screen emits no frame, so the count
// simply stays where the previous step left it.
func (h *protoHost) waitIdle(ms int) int {
	last, stable := h.nframe(), 0
	for stable < ms {
		time.Sleep(time.Millisecond)
		if n := h.nframe(); n != last {
			last, stable = n, 0
		} else {
			stable++
		}
	}
	return last
}

func (h *protoHost) send(typ sam.Hmesg, payload []byte) error {
	hdr := []byte{byte(typ), byte(len(payload)), byte(len(payload) >> 8)}
	if _, err := h.w.Write(hdr); err != nil {
		return err
	}
	if len(payload) > 0 {
		if _, err := h.w.Write(payload); err != nil {
			return err
		}
	}
	return nil
}

// recv reads one message: type + 16-bit count + count bytes.
func (h *protoHost) recv() (typ sam.Tmesg, payload []byte, err error) {
	var hdr [3]byte
	if _, err = io.ReadFull(h.r, hdr[:]); err != nil {
		return
	}
	n := int(hdr[1]) | int(hdr[2])<<8
	payload = make([]byte, n)
	if _, err = io.ReadFull(h.r, payload); err != nil {
		return
	}
	return sam.Tmesg(hdr[0]), payload, nil
}

// scriptUsesExtern reports whether a scenario's input uses the "extern" op, which
// needs the legacy extern FIFO set up (extstart). A scenario that doesn't use it
// is unaffected -- no FIFO, no env changes -- so existing goldens are unchanged.
func scriptUsesExtern(script []byte) bool {
	for line := range strings.Lines(string(script)) {
		if f := strings.Fields(line); len(f) > 0 && f[0] == "extern" {
			return true
		}
	}
	return false
}

// externFIFO configures the deterministic FIFO path for an extern scenario and
// returns it. $NAMESPACE is already empty (TestMain), so both terminals' plumbstart
// fails and they fall back to extstart. We pin $USER (and clear $DISPLAY) so the
// path -- /tmp/.sam.$USER -- is identical and computable for the in-process Go run
// and the C subprocess. Pid keeps concurrent runs apart; the file is cleaned up.
func externFIFO(t *testing.T) string {
	user := "samtermextern" + strconv.Itoa(os.Getpid())
	t.Setenv("USER", user)
	if d, ok := os.LookupEnv("DISPLAY"); ok {
		// Clear $DISPLAY so C's getenv("DISPLAY")==nil arm is taken (no display
		// suffix on the path); restore it after the test.
		os.Unsetenv("DISPLAY")
		t.Cleanup(func() { os.Setenv("DISPLAY", d) })
	}
	path := "/tmp/.sam." + user
	os.Remove(path)
	t.Cleanup(func() { os.Remove(path) })
	return path
}

// writeExtern writes data into the terminal's extern FIFO, exercising the
// no-plumber file-open path (extproc -> externload -> the RPlumb block in run ->
// externchar typing into the command window). It waits for the terminal to have
// created the FIFO (extstart runs during its startup, asynchronously for the C
// subprocess); O_WRONLY then blocks only until the terminal has the read end open.
func (h *protoHost) writeExtern(t *testing.T, data []byte) {
	t.Helper()
	if h.extern == "" {
		t.Fatal("extern op used but no extern FIFO was set up")
	}
	deadline := time.Now().Add(frametimeout)
	for {
		if fi, err := os.Stat(h.extern); err == nil && fi.Mode()&os.ModeNamedPipe != 0 {
			break
		}
		if time.Now().After(deadline) {
			t.Fatalf("extern: FIFO %s never appeared", h.extern)
		}
		time.Sleep(time.Millisecond)
	}
	f, err := os.OpenFile(h.extern, os.O_WRONLY, 0)
	if err != nil {
		t.Fatalf("extern: open %s: %v", h.extern, err)
	}
	if _, err := f.Write(data); err != nil {
		t.Fatalf("extern: write %s: %v", h.extern, err)
	}
	f.Close()
}

func pShort(v int) []byte {
	return []byte{byte(v), byte(v >> 8)}
}

func pLong(v int) []byte {
	return []byte{byte(v), byte(v >> 8), byte(v >> 16), byte(v >> 24)}
}

func pVlong(v int64) []byte {
	var b [8]byte
	binary.LittleEndian.PutUint64(b[:], uint64(v))
	return b[:]
}

// handshake performs the sam<->samterm startup exchange and returns once the
// command window (tag 0) is bound, current and unlocked.
func (h *protoHost) handshake(t *testing.T) {
	t.Helper()
	// samterm announces itself.
	typ, _, err := h.recv()
	if err != nil || typ != sam.Tversion {
		t.Fatalf("expected Tversion, got %d (%v)", typ, err)
	}
	typ, payload, err := h.recv()
	if err != nil || typ != sam.Tstartcmdfile {
		t.Fatalf("expected Tstartcmdfile, got %d (%v)", typ, err)
	}
	token := int64(binary.LittleEndian.Uint64(payload[0:8]))

	// Bind the command file to menu tag 0, make it current, and unlock,
	// mirroring sam's Tstartcmdfile handler (newfile->Hnewname, then
	// Hbindname/Hcurrent/Hunlock).
	h.send(sam.Hnewname, pShort(0))
	h.send(sam.Hbindname, append(pShort(0), pVlong(token)...))
	h.send(sam.Hcurrent, pShort(0))
	h.send(sam.Hunlock, nil)
}

// growdata inserts runes at pos in the file with the given tag (Hgrowdata =
// Hgrow + Hdata folded: short tag, long pos, long nrunes, utf8 data).
func (h *protoHost) growdata(tag, pos int, s string) {
	r := []rune(s)
	p := append(pShort(tag), pLong(pos)...)
	p = append(p, pLong(len(r))...)
	p = append(p, []byte(s)...)
	h.send(sam.Hgrowdata, p)
}

func TestProto(t *testing.T) {
	files, err := filepath.Glob("testdata/proto/*.txtar")
	if err != nil {
		t.Fatal(err)
	}
	for _, file := range files {
		name := strings.TrimSuffix(filepath.Base(file), ".txtar")
		t.Run(name, func(t *testing.T) {
			if !*update {
				// The -update discovery run settles on wall-clock idleness
				// (waitidle), which a busy parallel sibling could perturb.
				t.Parallel()
			}
			ar := testar.Open(t, file)
			input := ar.ReadFile("input")
			golden := strings.TrimSuffix(file, ".txtar") + ".gif"

			// waitframes golden: the cumulative frame count after startup and
			// after each operation. The Go run discovers it under -update by
			// settling (waitIdle); a normal run and the C run replay it with
			// waitFrame, so all stay on the same deterministic frame boundaries.
			var counts []int
			if !*update {
				counts = parseInts(ar.ReadFile("waitframes"))
			}

			frames, goCounts, goOps := runGoProto(t, input, counts)
			if *update {
				// Record the goldens from a replay-paced run, not the waitIdle
				// discovery run: pacing can change behavior at the margins, and
				// normal runs (Go and C) replay with waitFrame. The discovery
				// run contributes only the counts (see TestDraw for the same
				// two-pass update).
				counts = goCounts
				ar.WriteFile("waitframes", formatInts(goCounts))
				frames, _, goOps = runGoProto(t, input, counts)
				ar.WriteFile("drawops", []byte(goOps+"\n"))
				ar.Save(t)
			}
			drawtest.Check(t, frames, golden, *update)
			// The "drawops" golden is the sequence of devdraw opcodes the run
			// produced (see readDrawops). Comparing it -- not just the rendered
			// frames -- catches a divergence in the draw protocol even when it
			// leaves the visible pixels unchanged (e.g. an extra invisible draw).
			wantOps := strings.TrimSpace(string(ar.ReadFile("drawops")))
			if goOps != wantOps {
				t.Errorf("Go drawops differ from golden:\n  have %s\n  want %s", goOps, wantOps)
			}

			if haveOld() {
				t.Run("old", func(t *testing.T) {
					frames, _, cOps := runOldProto(t, input, counts)
					drawtest.Check(t, frames, golden, false)
					if cOps != wantOps {
						t.Errorf("C drawops differ from golden:\n  have %s\n  want %s", cOps, wantOps)
					}
				})
			}
		})
	}
}

// runGoProto runs the Go samterm in-process against a host protocol script and
// returns the rendered frames and the per-step frame counts (see
// runProtoScript). With counts == nil it discovers them by settling, for
// -update; otherwise it replays them.
func runGoProto(t *testing.T, script []byte, counts []int) ([]image.Image, []int, string) {
	t.Helper()
	useExtern := scriptUsesExtern(script)
	if useExtern {
		externFIFO(t) // sets $USER/$DISPLAY; the actual path is read from term.exname below
	}
	img := filepath.Join(t.TempDir(), "img")
	drawlog := filepath.Join(filepath.Dir(img), "drawlog")
	// Built as a slice, not with t.Setenv, so parallel tests can configure
	// their own displays independently (see runLocal).
	env := append(os.Environ(),
		"DEVDRAW_TEST_MEMORY=1",
		"DEVDRAW_TEST_OUTPUT="+img,
		"DEVDRAW_DRAWLOG="+drawlog,
	)

	disp, err := draw.Init(nil, "", "sam", "300x200", env)
	if err != nil {
		t.Skipf("no display: %v", err)
	}
	// Clear the screen to white, matching getscreen (plan9.go), which the real
	// samterm runs at startup but NewTerm does not -- so the Go draw stream (and
	// hence the frame sequence) matches the C samterm's, which does run getscreen.
	disp.ScreenImage.Draw(disp.ScreenImage.Clipr, disp.White, nil, draw.ZP)

	// Note: we intentionally do not disp.Close() or otherwise shut the terminal
	// down. The draw package's mouse/keyboard reader goroutines log.Fatal on a
	// closed display, so the cleanest thing for an in-process run is to leave
	// the (now idle) terminal goroutines and devdraw subprocess to be reaped
	// when the test binary exits.

	h2tR, h2tW, _ := os.Pipe()
	t2hR, t2hW, _ := os.Pipe()
	term := NewTerm(disp, h2tR, t2hW, env)
	term.startup()
	go term.run()

	h := &protoHost{w: h2tW, r: bufio.NewReader(t2hR), img: img}
	h.handshake(t)
	go h.drain()
	got := runProtoScript(t, h, script, counts)

	return drawtest.FramesAndCursor(t, img), got, readDrawops(drawlog)
}

// runOldProto runs the C samterm subprocess against the identical host script.
func runOldProto(t *testing.T, script []byte, counts []int) ([]image.Image, []int, string) {
	t.Helper()
	externPath := ""
	if scriptUsesExtern(script) {
		externPath = externFIFO(t) // C computes the same /tmp/.sam.$USER from the env
	}
	img := filepath.Join(t.TempDir(), "img")
	drawlog := filepath.Join(filepath.Dir(img), "drawlog")

	h2tR, h2tW, _ := os.Pipe()
	t2hR, t2hW, _ := os.Pipe()
	cmd := exec.Command(oldExe(), "-W", "300x200")
	cmd.Stdin = h2tR
	cmd.Stdout = t2hW
	errf, _ := os.Create(filepath.Join(img, "stderr"))
	defer errf.Close()
	cmd.Stderr = errf
	cmd.Env = append(os.Environ(),
		"DEVDRAW_TEST_MEMORY=1",
		"DEVDRAW_TEST_OUTPUT="+img,
		"DEVDRAW_DRAWLOG="+drawlog,
		cov.Env(t), // per-run LLVM_PROFILE_FILE when -uncoverc is set
	)
	cmd.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	if err := cmd.Start(); err != nil {
		t.Fatalf("start c samterm: %v", err)
	}
	h2tR.Close()
	t2hW.Close()

	h := &protoHost{w: h2tW, r: bufio.NewReader(t2hR), img: img, extern: externPath}
	h.handshake(t)
	go h.drain() // consume the C terminal's replies so it never blocks on a full pipe
	got := runProtoScript(t, h, script, counts)

	// Shut down cleanly: closing the host write end gives the C samterm an EOF
	// on its host fd, so it exits (and devdraw finishes writing the last frame
	// before exiting). Only SIGKILL the process group as a timeout fallback,
	// since killing mid-write would leave a truncated final image file.
	h2tW.Close()
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	select {
	case <-done:
	case <-time.After(3 * time.Second):
		syscall.Kill(-cmd.Process.Pid, syscall.SIGKILL)
		<-done
	}
	return drawtest.FramesAndCursor(t, img), got, readDrawops(drawlog)
}

// runProtoScript executes a scenario's host operations (the txtar "input" section,
// one per line) against the terminal:
//
//	growdata TAG POS TEXT   insert TEXT (\t \n \\ escaped) at rune POS in tag TAG
//	setdot TAG P0 P1        Hsetdot: highlight runes [P0,P1) in tag TAG
//	cut TAG A OLD           Hcut: delete OLD runes at A in tag TAG
//
// Operations are delivered over a single ordered pipe and the terminal
// processes them in order, writing one frame per drawing operation (see
// testframe in devdraw.c), so the whole frame sequence is deterministic and
// identical for the Go and C terminals. Between steps it synchronizes on that
// frame count: with counts != nil it replays the recorded waitframes counts
// (waitFrame); with counts == nil it discovers them under -update by settling
// (waitIdle). It returns the per-step counts: one after startup and one after
// each operation.
func runProtoScript(t *testing.T, h *protoHost, script []byte, counts []int) []int {
	t.Helper()
	step := 0
	sync := func() int {
		var n int
		if counts != nil {
			if step >= len(counts) {
				t.Fatalf("waitframes golden has %d counts, too few (regenerate with -update)", len(counts))
			}
			n = counts[step]
			h.waitFrame(t, n)
		} else {
			n = h.waitIdle(updateSettleMS)
		}
		step++
		return n
	}
	out := []int{sync()} // baseline: startup has fully drawn
	for line := range strings.Lines(string(script)) {
		line = strings.TrimSpace(line)
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		f := strings.Fields(line)
		// tagPos builds the common {short tag, long pos} payload.
		tagPos := func() []byte {
			return append(pShort(mustAtoi(t, f[1])), pLong(mustAtoi(t, f[2]))...)
		}
		switch f[0] {
		case "growdata":
			rest := strings.SplitN(line, " ", 4)
			if len(rest) < 4 {
				t.Fatalf("bad growdata: %q", line)
			}
			h.growdata(mustAtoi(t, rest[1]), mustAtoi(t, rest[2]), unescape(rest[3]))
		case "data": // data TAG POS [TEXT]: Hdata, fill a hole opened by grow.
			// TEXT may be omitted to send an empty Hdata (length 0).
			rest := strings.SplitN(line, " ", 4)
			if len(rest) < 3 {
				t.Fatalf("bad data: %q", line)
			}
			var text string
			if len(rest) == 4 {
				text = unescape(rest[3])
			}
			h.send(sam.Hdata, append(append(pShort(mustAtoi(t, rest[1])), pLong(mustAtoi(t, rest[2]))...), []byte(text)...))
		case "grow": // grow TAG POS N: Hgrow, open a hole of N runes at POS
			h.send(sam.Hgrow, append(tagPos(), pLong(mustAtoi(t, f[3]))...))
		case "setdot": // setdot TAG P0 P1
			h.send(sam.Hsetdot, append(tagPos(), pLong(mustAtoi(t, f[3]))...))
		case "cut": // cut TAG A OLD
			h.send(sam.Hcut, append(tagPos(), pLong(mustAtoi(t, f[3]))...))
		case "badmsg": // badmsg: a header claiming a count > DATASIZE (no payload),
			// to drive rcv's malformed-message path (rcverrs/dumperrmsg). The
			// stream stays in sync because no payload bytes follow.
			h.w.Write([]byte{byte(sam.Hclean), 0xff, 0xff})
		case "moveto": // moveto TAG POS
			h.send(sam.Hmoveto, tagPos())
		case "origin": // origin TAG POS
			h.send(sam.Horigin, tagPos())
		case "clean": // clean TAG
			h.send(sam.Hclean, pShort(mustAtoi(t, f[1])))
		case "dirty": // dirty TAG
			h.send(sam.Hdirty, pShort(mustAtoi(t, f[1])))
		case "current": // current TAG
			h.send(sam.Hcurrent, pShort(mustAtoi(t, f[1])))
		case "check0": // check0 TAG
			h.send(sam.Hcheck0, pShort(mustAtoi(t, f[1])))
		case "check": // check TAG
			h.send(sam.Hcheck, pShort(mustAtoi(t, f[1])))
		case "unlockfile": // unlockfile TAG
			h.send(sam.Hunlockfile, pShort(mustAtoi(t, f[1])))
		case "snarflen": // snarflen N
			h.send(sam.Hsnarflen, pLong(mustAtoi(t, f[1])))
		case "setpat": // setpat PATTERN
			rest := strings.SplitN(line, " ", 2)
			h.send(sam.Hsetpat, []byte(rest[1]))
		case "movname": // movname TAG NAME
			rest := strings.SplitN(line, " ", 3)
			h.send(sam.Hmovname, append(pShort(mustAtoi(t, rest[1])), rest[2]...))
		case "delname": // delname TAG
			h.send(sam.Hdelname, pShort(mustAtoi(t, f[1])))
		case "newname": // newname TAG: Hnewname, create an (empty) menu entry
			h.send(sam.Hnewname, pShort(mustAtoi(t, f[1])))
		case "bindname": // bindname TAG TOKEN: Hbindname, bind a text token to tag
			h.send(sam.Hbindname, append(pShort(mustAtoi(t, f[1])), pVlong(int64(mustAtoi(t, f[2])))...))
		case "close": // close TAG: Hclose, close all windows of the tag's text
			h.send(sam.Hclose, pShort(mustAtoi(t, f[1])))
		case "setsnarf": // setsnarf TEXT: Hsetsnarf (short count, then count raw bytes)
			rest := strings.SplitN(line, " ", 2)
			data := []byte(unescape(rest[1]))
			h.send(sam.Hsetsnarf, pShort(len(data)))
			h.w.Write(data) // the snarf bytes follow the message in the stream
		case "plumb": // plumb TEXT: Hplumb (short count, then count raw bytes, dropped)
			rest := strings.SplitN(line, " ", 2)
			data := []byte(unescape(rest[1]))
			h.send(sam.Hplumb, pShort(len(data)))
			h.w.Write(data)
		case "extern": // extern TEXT: write TEXT (\t \n \\ escaped) into the legacy
			// extern FIFO -- the no-plumber file-open channel. The terminal reads it
			// (extproc), feeds it as keyboard input, and types it into the command
			// window, so it renders just like a plumbed "B file" request.
			rest := strings.SplitN(line, " ", 2)
			if len(rest) < 2 {
				t.Fatalf("bad extern: %q", line)
			}
			h.writeExtern(t, []byte(unescape(rest[1])))
		case "exit": // exit: Hexit -- the terminal stops responding (run returns)
			// and draws nothing more, so there is no frame to sync on; just send
			// it and finish. The handler still runs (coverage), and the C samterm
			// exits cleanly here too.
			h.send(sam.Hexit, nil)
			return out
		default:
			t.Fatalf("unknown proto op %q", line)
		}
		out = append(out, sync())
	}
	return out
}

func mustAtoi(t *testing.T, s string) int {
	n, err := strconv.Atoi(s)
	if err != nil {
		t.Fatalf("bad number %q: %v", s, err)
	}
	return n
}
