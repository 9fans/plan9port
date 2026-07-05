package main

import (
	"bytes"
	"context"
	"flag"
	"fmt"
	"image"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"testing"
	"time"

	"9fans.net/plan9port/internal/drawtest"
	"9fans.net/plan9port/internal/llvmcover"
	"9fans.net/plan9port/internal/testar"
)

// The acme port is differentially tested against the C acme (now living at
// src/cmd/oacme, bin/oacme) through its -D debug-conversation protocol. Every
// scenario is a testdata/debug/*.txtar archive holding a "script" member (the -D
// commands) and a "stdout" golden; any other member is seeded as an input file
// in the run's working directory under its own name, so a script can open one or
// more named files. TestDebug runs each script through the C oracle (regenerating
// the golden under -update) and through the in-process Go port (goDriver), and
// requires all three to agree.
//
// Both sides render to devdraw's in-memory backend (DEVDRAW_TEST_MEMORY), so
// they need no host display and a deterministic dpi. They also capture the
// drawn frames (DEVDRAW_TEST_OUTPUT): devdraw writes one window frame per drawing
// operation plus a matching cursor frame, which drawtest.FramesAndCursor returns
// interleaved as window,cursor,window,cursor,...; every test checks the C and Go
// frame sequences, pixel-for-pixel and frame-for-frame, against a committed
// golden GIF (testdata/debug/<name>.gif), so each incremental draw -- not just
// the final state -- is verified. This requires the Go port to issue the same
// draw operations in the same order as C. To keep the frames deterministic, each
// acme runs in its own temp-directory root with $HOME and the working directory
// set to that root and -T <root> so the root prefix is trimmed off displayed file
// names (see winsetname).

var update = flag.Bool("update", false, "regenerate golden stdout/frames from the C oracle")

const plan9root = "/usr/local/plan9"

// oacmecover is the LLVM-instrumented C acme (build: mk -C src/cmd/oacme
// o.oacme.cover). cov registers the -uncoverc flag; when -uncoverc=FILE is
// given, the oracle runs use the instrumented binary and their profiles are
// merged into a C-coverage report.
var (
	oacmecover = filepath.Join(plan9root, "src/cmd/oacme/o.oacme.cover")
	cov        = llvmcover.New(oacmecover, "")
)

// testfont is loaded by both acmes (the C oracle via -f/-F, the Go port via
// $font) so font-dependent output -- the ctl file's font name and the window
// geometry -- is identical and deterministic.
const testfont = plan9root + "/font/lucsans/euro.8.font"

func TestMain(m *testing.M) {
	flag.Parse()
	// Pin the zone so timefmt's local-time formatting (the "modified since last
	// read" warning) is deterministic and portable. Set before any use of
	// time.Local, which Go initializes from $TZ once.
	os.Setenv("TZ", "UTC")
	os.Setenv("DEVDRAW_TEST_MEMORY", "1")
	// The Go acme reads $font in draw.Init (its initdraw passes ""). Pin it to
	// the same font the C oracle gets via -f so both sides agree.
	os.Setenv("font", testfont)
	os.Unsetenv("Font")
	code := m.Run()
	if err := cov.Finish(); err != nil {
		if code == 0 {
			code = 1
		}
		println("uncoverc:", err.Error())
	}
	os.Exit(code)
}

// resolve returns dir with symlinks evaluated, matching what acme's getwd sees
// (on macOS /tmp -> /private/tmp), so -T <root> actually trims opened names.
func resolve(t *testing.T, dir string) string {
	t.Helper()
	r, err := filepath.EvalSymlinks(dir)
	if err != nil {
		t.Fatal(err)
	}
	return r
}

// cOracle runs the C acme's -D mode with script on stdin in working directory
// dir (its temp root), returning its stdout and the frames it drew. With
// -uncoverc the oracle is o.oacme.cover and each run writes its own LLVM profile
// (via cov.Env).
func cOracle(t *testing.T, script []byte, dir string) ([]byte, []image.Image, []int) {
	t.Helper()
	bin := filepath.Join(plan9root, "bin", "oacme")
	if cov.Enabled() {
		bin = oacmecover
	}
	if _, err := os.Stat(bin); err != nil {
		t.Skipf("no %s", bin)
	}
	root := resolve(t, dir)
	img := filepath.Join(t.TempDir(), "img")
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()
	cmd := exec.CommandContext(ctx, bin, "-D", "-T", root, "-f", testfont, "-F", testfont)
	cmd.Dir = dir
	cmd.Stdin = bytes.NewReader(script)
	// A private name space per oacme run so its posted "acme" 9P service (which
	// the read/write verbs connect to via nsmount) cannot collide with another
	// run's, and a stale socket from a previous run cannot be reached. $HOME and
	// the working directory point at the temp root so nothing outside it (and no
	// host path) leaks into the window.
	// $TZ (set in TestMain) pins Go's zone; plan9port's localtime instead reads
	// $timezone (a tzfile path), so point it at UTC too, so timefmt's output in
	// the "+Errors" warning is identical and portable.
	cmd.Env = append(os.Environ(), "Font=", "HOME="+dir, "NAMESPACE="+t.TempDir(),
		"timezone=/usr/share/zoneinfo/UTC",
		"DEVDRAW_TEST_OUTPUT="+img, cov.Env(t))
	var stdout, stderr bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	if err := cmd.Run(); err != nil {
		t.Fatalf("oacme -D: %v\nstderr:\n%s", err, stderr.String())
	}
	return stdout.Bytes(), drawtest.FramesAndCursor(t, img), readArrivals(img)
}

// readArrivals reads <img>.setmouse, the per-setmouse frame indices devdraw
// logged (the frame written count when each setmouse RPC arrived). -update
// diffs the C and Go lists to compute each scenario's setmouse deferral.
func readArrivals(img string) []int {
	data, err := os.ReadFile(img + ".setmouse")
	if err != nil {
		return nil
	}
	var a []int
	for _, f := range strings.Fields(string(data)) {
		n, _ := strconv.Atoi(f)
		a = append(a, n)
	}
	return a
}

// setmouseDelays returns the DEVDRAW_TEST_SETMOUSE_DELAYS string that lands the
// Go port's cursor on the same frames as C: defer the k-th setmouse by
// carr[k]-garr[k] frames. ok is false if the runs disagree on the number of
// setmouse calls or Go fires one later than C (which a delay cannot fix).
func setmouseDelays(carr, garr []int) (s string, allZero, ok bool) {
	if len(carr) != len(garr) {
		return "", false, false
	}
	allZero = true
	var b strings.Builder
	for k := range carr {
		d := carr[k] - garr[k]
		if d < 0 {
			return "", false, false
		}
		if d != 0 {
			allZero = false
		}
		if k > 0 {
			b.WriteByte(' ')
		}
		fmt.Fprintf(&b, "%d", d)
	}
	return b.String(), allZero, true
}

// goDriver runs a -D script through the in-process Go acme in working directory
// dir (its temp root), returning its stdout, the frames it drew, and the
// per-setmouse arrival frame indices. delays is the scenario's
// DEVDRAW_TEST_SETMOUSE_DELAYS (so the Go cursor lands on C's frames).
func goDriver(t *testing.T, script, dir, delays string) (string, []image.Image, []int) {
	t.Helper()
	old, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
	}
	if err := os.Chdir(dir); err != nil {
		t.Fatal(err)
	}
	defer os.Chdir(old)
	img := filepath.Join(t.TempDir(), "img")
	// devdraw (spawned by draw.Init inside runD) reads these from the
	// environment; subtests run sequentially, so the process-global env is safe.
	t.Setenv("HOME", dir)
	t.Setenv("DEVDRAW_TEST_OUTPUT", img)
	t.Setenv("DEVDRAW_TEST_SETMOUSE_DELAYS", delays)
	a := new(Acme)
	a.trimpre = []rune(resolve(t, dir))
	var out, errbuf bytes.Buffer
	defer func() {
		if a.disp != nil {
			a.disp.Close()
		}
	}()
	if err := a.runD(strings.NewReader(script), &out, &errbuf); err != nil {
		t.Fatalf("runD: %v\nstderr: %s", err, errbuf.String())
	}
	return out.String(), drawtest.FramesAndCursor(t, img), readArrivals(img)
}

// TestDebugthread covers the live editor's -D stdin loop (debug.go debugthread):
// it drives a set-up editor through debugthread with a small script and checks
// the deterministic verb echoes. This is the entry threadmain wires to os.Stdin
// for `acme -D`; the frame-differential tests reach the identical debugLoop via
// runD, so here we only confirm the live wiring drives the same dispatch.
func TestDebugthread(t *testing.T) {
	dir := t.TempDir()
	old, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
	}
	if err := os.Chdir(dir); err != nil {
		t.Fatal(err)
	}
	defer os.Chdir(old)
	t.Setenv("HOME", dir)
	t.Setenv("DEVDRAW_TEST_OUTPUT", filepath.Join(t.TempDir(), "img"))

	a := new(Acme)
	var out bytes.Buffer
	a.stdout = &out
	a.stderr = &out
	a.reinit()
	if err := a.initdraw("acme", "1024x768"); err != nil {
		t.Skipf("initdraw: %v", err)
	}
	defer a.disp.Close()
	a.rowinit(&a.row, a.screen.Clipr)
	a.fsysinit()
	defer a.fsysclose()

	// threadmain passes os.Stdin here; drive the loop with an in-memory script.
	a.debugthread(strings.NewReader("newcol\nwin\nbody 1 hello world\nprint 1\n"))
	got := out.String()
	for _, want := range []string{"col 0", "win 1", "hello world"} {
		if !strings.Contains(got, want) {
			t.Fatalf("debugthread output = %q, want it to contain %q", got, want)
		}
	}
}

// prepRun makes a per-run temp-directory root for a scenario so that any file
// writes (get/put/edit) land in a throwaway tree, never the package tree, and
// the C and Go runs never share a file. Every archive member other than the
// reserved "script" and "stdout" is seeded into that root under its own name
// (interpreted relative to the working directory, slashes allowed for
// subdirectories), so a script can open one or more named input files.
func prepRun(t *testing.T, ar *testar.Archive) string {
	dir := t.TempDir()
	for _, f := range ar.Files {
		if f.Name == "script" || f.Name == "stdout" || f.Name == "setmousedelay" {
			continue
		}
		p := filepath.Join(dir, filepath.FromSlash(f.Name))
		if err := os.MkdirAll(filepath.Dir(p), 0o777); err != nil {
			t.Fatal(err)
		}
		if err := os.WriteFile(p, f.Data, 0o644); err != nil {
			t.Fatal(err)
		}
	}
	return dir
}

// EVERY debug scenario's frame golden comes from the C oracle, and both the Go
// port and the C reference are checked against it, pixel for pixel, frame for
// frame. There is no Go-sourced golden: the port is faithful enough that its
// draw-op stream matches C's exactly for every scenario.
//
// Getting here meant killing off, one at a time, every case that once seemed to
// "legitimately differ" from C -- each turned out to be a real port bug or a
// test-harness artifact, never an inherent divergence. Kept here as a warning
// against hand-waving a mismatch as "timing" or "architectural":
//
//	- OS-error-text cases (C's %r vs Go's %v): the errstr helper now renders
//	  errors exactly like %r.
//	- editf_pipe/pipe2write/pipein/pipeout, blamed on a "1px flush-boundary"
//	  artifact. Real bug: the Go running-command row-tag label dropped the pipe
//	  char ("tr " vs C's "|tr ") because runpipe stripped it before run() computed
//	  the name. Fixed in run() (label from the full command incl. the pipe char,
//	  as C's runproc does).
//	- builtins_load, blamed on "multi-window Load reflow ordering". Real bug: the
//	  F-record (scratch) body was reloaded with an in-memory textinsert instead of
//	  C's temp-file+textload, skipping textload's textsetselect(0,0) and dropping a
//	  caret-reposition frame. rowload now matches C's temp-file+textload.
//	- editf_errdiscard, blamed on a devdraw "setmouse race" where C's synchronous
//	  flushimage supposedly won and Go rendered a few stale frames first. Wrong:
//	  Go's MoveCursor round-trips exactly like C's _displaymoveto, both log the
//	  +Errors moveto (to 15,85) arriving at the same frame (86), content identical.
//	  Real cause: a STALE setmousedelay member ("0 3") calibrated before the
//	  run()/rowload fixes aligned Go's draw stream with C. Once aligned the correct
//	  deferral was 0, but -update only rewrote the member when the new deferral was
//	  non-zero, so the stale "0 3" survived and held Go's cursor warp 3 frames
//	  (frame 90 vs C's 86). Fixed by making -update drop the member when the
//	  calibrated deferral is all-zero (see below).

func TestDebug(t *testing.T) {
	files, err := filepath.Glob("testdata/debug/*.txtar")
	if err != nil {
		t.Fatal(err)
	}
	if len(files) == 0 {
		t.Skip("no testdata/debug/*.txtar")
	}
	for _, file := range files {
		name := strings.TrimSuffix(filepath.Base(file), ".txtar")
		golden := strings.TrimSuffix(file, ".txtar") + ".gif"
		t.Run(name, func(t *testing.T) {
			ar := testar.Open(t, file)
			script := ar.ReadFile("script")
			if script == nil {
				t.Fatal("missing script member")
			}

			delays := string(ar.ReadFile("setmousedelay"))
			cdir := prepRun(t, ar)
			got, cframes, carr := cOracle(t, script, cdir)
			gdir := prepRun(t, ar)
			goOut, gframes, garr := goDriver(t, string(script), gdir, delays)

			if *update {
				ar.WriteFile("stdout", got)
				// Calibrate the Go cursor deferral against C: the k-th setmouse is
				// held carr[k]-garr[k] frames so the Go cursor lands on the same
				// frame as C's. Always rewrite the member when the runs agree on the
				// setmouse count, even when the calibrated deferral is all-zero:
				// otherwise a stale non-zero delay from an earlier (misaligned) port
				// would survive and wrongly defer the Go cursor. Drop the member
				// entirely when no deferral is needed, so the common case carries no
				// data.
				if s, allZero, ok := setmouseDelays(carr, garr); ok {
					if allZero {
						ar.Delete("setmousedelay")
					} else {
						// Trailing newline so testar's Encode leaves the value clean
						// (no "^D" no-final-newline sentinel appended on disk).
						ar.WriteFile("setmousedelay", []byte(s+"\n"))
					}
				}
				ar.Save(t)
				drawtest.Check(t, cframes, golden, true)
				return
			}
			want := ar.ReadFile("stdout")
			if want == nil {
				t.Fatalf("missing stdout member; run with -update")
			}
			if !bytes.Equal(got, want) {
				t.Errorf("C stdout mismatch\n--- script ---\n%s\n--- got ---\n%s\n--- want ---\n%s",
					script, got, want)
			}
			if goOut != string(want) {
				t.Errorf("Go stdout mismatch\n--- script ---\n%s\n--- go ---\n%s\n--- want ---\n%s",
					script, goOut, want)
			}
			// The golden is the "old" reference's drawing. Both terminals are
			// checked against it, frame-for-frame, the way samterm checks both its Go
			// and old runs: the Go port must reproduce the golden, and the old
			// reference must too (catching a stale golden or oracle nondeterminism).
			// The reference is the C oracle today, but the "old" label keeps the door
			// open for it to become an earlier Go version.
			drawtest.Check(t, gframes, golden, false)
			t.Run("old", func(t *testing.T) {
				drawtest.Check(t, cframes, golden, false)
			})
		})
	}
}
