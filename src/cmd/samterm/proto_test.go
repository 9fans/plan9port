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
	"9fans.net/plan9port/internal/testar"
)

var update = flag.Bool("update", false, "update committed goldens in testdata")

// These tests drive samterm with the test process itself playing the host
// (sam), reading and writing wire-protocol messages directly over a pipe. This
// is the "connected to the test binary" mode: it needs no sam. Each scenario is
// a testdata/proto/<name>.txtar (comment = description, "input" section = host
// operations, one per line); every rendered frame is checked against a committed
// golden GIF (testdata/proto/<name>.gif) and, when bin/samterm is present, frame
// for frame against the C samterm driven by the identical script.
//
// Determinism: the host paces operations one at a time, waiting for each
// operation's frame before sending the next (protoHost.waitframe), so samterm
// never batches redraws -- the same per-event pacing the end-to-end test gets
// from devdraw's waitframe. With a fixed 600x400 window, fixed dpi and the
// built-in default font, the captured frame sequence is reproducible.

// protoHost speaks the wire protocol to a samterm over r/w.
type protoHost struct {
	w   io.Writer
	r   *bufio.Reader
	img string // image output directory
}

// nframe returns how many image frames samterm has emitted so far.
func (h *protoHost) nframe() int {
	files, _ := filepath.Glob(h.img + ".*")
	return len(files)
}

// waitframe blocks until samterm has emitted at least n frames. Like devdraw's
// waitframe, it lets the host pace each operation against the output stream
// (rather than racing samterm's flush-per-wakeup), so the captured frame
// sequence is deterministic.
func (h *protoHost) waitframe(t *testing.T, n int) {
	t.Helper()
	deadline := time.Now().Add(5 * time.Second)
	for h.nframe() < n {
		if time.Now().After(deadline) {
			t.Fatalf("proto: timed out waiting for frame %d (have %d)", n, h.nframe())
		}
		time.Sleep(time.Millisecond)
	}
}

// settle waits for the frame count to stop growing (startup has quiesced) and
// returns it, establishing the baseline the scripted operations pace from.
func (h *protoHost) settle() int {
	last, stable := -1, 0
	for stable < 200 {
		time.Sleep(time.Millisecond)
		if n := h.nframe(); n != last {
			last, stable = n, 0
		} else {
			stable++
		}
	}
	return last
}

func (h *protoHost) send(typ int, payload []byte) error {
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
func (h *protoHost) recv() (typ int, payload []byte, err error) {
	var hdr [3]byte
	if _, err = io.ReadFull(h.r, hdr[:]); err != nil {
		return
	}
	n := int(hdr[1]) | int(hdr[2])<<8
	payload = make([]byte, n)
	if _, err = io.ReadFull(h.r, payload); err != nil {
		return
	}
	return int(hdr[0]), payload, nil
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
	if err != nil || typ != Tversion {
		t.Fatalf("expected Tversion, got %d (%v)", typ, err)
	}
	typ, payload, err := h.recv()
	if err != nil || typ != Tstartcmdfile {
		t.Fatalf("expected Tstartcmdfile, got %d (%v)", typ, err)
	}
	token := int64(binary.LittleEndian.Uint64(payload[0:8]))

	// Bind the command file to menu tag 0, make it current, and unlock,
	// mirroring sam's Tstartcmdfile handler (newfile->Hnewname, then
	// Hbindname/Hcurrent/Hunlock).
	h.send(Hnewname, pShort(0))
	h.send(Hbindname, append(pShort(0), pVlong(token)...))
	h.send(Hcurrent, pShort(0))
	h.send(Hunlock, nil)
}

// growdata inserts runes at pos in the file with the given tag (Hgrowdata =
// Hgrow + Hdata folded: short tag, long pos, long nrunes, utf8 data).
func (h *protoHost) growdata(tag, pos int, s string) {
	r := []rune(s)
	p := append(pShort(tag), pLong(pos)...)
	p = append(p, pLong(len(r))...)
	p = append(p, []byte(s)...)
	h.send(Hgrowdata, p)
}

func TestProto(t *testing.T) {
	files, err := filepath.Glob("testdata/proto/*.txtar")
	if err != nil {
		t.Fatal(err)
	}
	for _, file := range files {
		name := strings.TrimSuffix(filepath.Base(file), ".txtar")
		t.Run(name, func(t *testing.T) {
			ar := testar.Open(t, file)
			input := ar.ReadFile("input")
			golden := strings.TrimSuffix(file, ".txtar") + ".gif"

			frames := runGoProto(t, name, input)
			drawtest.Check(t, frames, golden, *update)

			if haveOld() {
				t.Run("old", func(t *testing.T) {
					frames := runOldProto(t, name, input)
					drawtest.Check(t, frames, golden, false)
				})
			}
		})
	}
}

// runGoProto runs sam in-process against a samterm protocol script,
// returning the rendered frames.
func runGoProto(t *testing.T, name string, script []byte) []image.Image {
	t.Helper()
	img := filepath.Join(t.TempDir(), "img")
	t.Setenv("DEVDRAW_TEST_MEMORY", "1")
	t.Setenv("DEVDRAW_TEST_OUTPUT", img)

	disp, err := draw.Init(nil, "", "sam", "600x400")
	if err != nil {
		t.Skipf("no display: %v", err)
	}

	// Note: we intentionally do not disp.Close() or otherwise shut the terminal
	// down. The draw package's mouse/keyboard reader goroutines log.Fatal on a
	// closed display, so the cleanest thing for an in-process run is to leave
	// the (now idle) terminal goroutines and devdraw subprocess to be reaped
	// when the test binary exits.

	h2tR, h2tW, _ := os.Pipe()
	t2hR, t2hW, _ := os.Pipe()
	term := NewTerm(disp, h2tR, t2hW)
	term.startup()
	go term.run()

	h := &protoHost{w: h2tW, r: bufio.NewReader(t2hR), img: img}
	h.handshake(t)
	runProtoScript(t, h, img, script)

	return drawtest.Frames(t, img)
}

// runOldProto runs the C samterm subprocess against the identical host script.
func runOldProto(t *testing.T, name string, script []byte) []image.Image {
	t.Helper()
	img := filepath.Join(t.TempDir(), "img")
	t.Setenv("DEVDRAW_TEST_OUTPUT", img)

	h2tR, h2tW, _ := os.Pipe()
	t2hR, t2hW, _ := os.Pipe()
	cmd := exec.Command(oldExe(), "-W", "600x400")
	cmd.Stdin = h2tR
	cmd.Stdout = t2hW
	errf, _ := os.Create(filepath.Join(img, "stderr"))
	defer errf.Close()
	cmd.Stderr = errf
	cmd.Env = append(os.Environ(),
		"DEVDRAW_TEST_MEMORY=1",
		"DEVDRAW_TEST_OUTPUT="+img,
	)
	cmd.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	if err := cmd.Start(); err != nil {
		t.Fatalf("start c samterm: %v", err)
	}
	h2tR.Close()
	t2hW.Close()

	h := &protoHost{w: h2tW, r: bufio.NewReader(t2hR), img: img}
	h.handshake(t)
	runProtoScript(t, h, img, script)

	h2tW.Close()

	syscall.Kill(-cmd.Process.Pid, syscall.SIGKILL)
	return drawtest.Frames(t, img)
}

// runProtoScript executes a scenario's host operations (the txtar "input" section,
// one per line) against the terminal:
//
//	growdata TAG POS TEXT   insert TEXT (\t \n \\ escaped) at rune POS in tag TAG
//	setdot TAG P0 P1        Hsetdot: highlight runes [P0,P1) in tag TAG
//	cut TAG A OLD           Hcut: delete OLD runes at A in tag TAG
//
// Each operation redraws once. The host paces them one at a time, waiting for
// each operation's frame before sending the next, so samterm never batches two
// operations' redraws into a single (or differently split) flush and the
// captured frame sequence is deterministic.
func runProtoScript(t *testing.T, h *protoHost, img string, script []byte) {
	t.Helper()
	n := h.settle() // baseline: startup has fully drawn
	for line := range strings.Lines(string(script)) {
		line = strings.TrimSpace(line)
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		f := strings.Fields(line)
		switch f[0] {
		case "growdata":
			rest := strings.SplitN(line, " ", 4)
			if len(rest) < 4 {
				t.Fatalf("bad growdata: %q", line)
			}
			h.growdata(mustAtoi(t, rest[1]), mustAtoi(t, rest[2]), unescape(rest[3]))
		case "setdot":
			h.send(Hsetdot, append(append(pShort(mustAtoi(t, f[1])), pLong(mustAtoi(t, f[2]))...), pLong(mustAtoi(t, f[3]))...))
		case "cut":
			h.send(Hcut, append(append(pShort(mustAtoi(t, f[1])), pLong(mustAtoi(t, f[2]))...), pLong(mustAtoi(t, f[3]))...))
		default:
			t.Fatalf("unknown proto op %q", line)
		}
		n++
		h.waitframe(t, n)
	}
}

func mustAtoi(t *testing.T, s string) int {
	n, err := strconv.Atoi(s)
	if err != nil {
		t.Fatalf("bad number %q: %v", s, err)
	}
	return n
}
