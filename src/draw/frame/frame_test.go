package frame

import (
	"bytes"
	"context"
	"flag"
	"fmt"
	"image"
	"iter"
	"log"
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
	"9fans.net/plan9port/internal/llvmcover"
	"9fans.net/plan9port/internal/testar"
)

var (
	plan9, _   = filepath.Abs("../../..") // plan9port root
	libframe   = filepath.Join(plan9, "src/libframe")
	framecover = filepath.Join(libframe, "frametest.cover")

	update = flag.Bool("update", false, "update expected outputs")
	cov    = llvmcover.New(framecover, "frametest")
)

func TestMain(m *testing.M) {
	os.Setenv("DEVDRAW", filepath.Join(plan9, "bin/devdraw"))
	os.Setenv("DEVDRAW_TEST_MEMORY", "1")
	os.Setenv("font", filepath.Join(plan9, "font/lucsans/euro.8.font"))

	code := m.Run()
	if err := cov.Finish(); err != nil {
		log.Fatal(err)
	}
	os.Exit(code)
}

// TestInsertInvalidRune drives the one bxscan branch the script harness can't
// reach: an un-encodable rune (a lone surrogate). Both the Go and C harnesses
// read their text as UTF-8, which never yields such a rune, so this calls
// Insert directly. RuneLen reports -1 for it and bxscan charges 3 bytes (the
// width of the replacement encoding), covering frinsert.go:60.
// FIX: use \uD800x\uDC00 in a script.
// CLAUDE: suggestion to drive the invalid-rune (surrogate) case through a script
// CLAUDE: input rather than calling Insert directly. Kept the direct call since the
// CLAUDE: script reader decodes UTF-8 and can't carry a lone surrogate.
func TestInsertInvalidRune(t *testing.T) {
	disp, err := draw.Init(nil, "", "frametest", "400x400", nil)
	if err != nil {
		t.Skipf("no display: %v", err)
	}
	defer disp.Close()
	r := newRunner(t, disp)
	r.font = disp.Font
	r.runLine(t, "startup:1", "init 200 100")
	r.f.Insert([]rune{0xD800, 'x', 0xDC00}, 0) // lone surrogates: RuneLen == -1
	if r.f.NumChars != 3 {
		t.Fatalf("NumChars = %d, want 3", r.f.NumChars)
	}
}

func TestScript(t *testing.T) {
	files, err := filepath.Glob("testdata/*.txtar")
	if err != nil {
		t.Fatal(err)
	}
	if len(files) == 0 {
		t.Fatal("no testdata/*.txtar cases")
	}
	for _, file := range files {
		name := strings.TrimSuffix(filepath.Base(file), ".txtar")
		t.Run(name, func(t *testing.T) {
			disp, err := draw.Init(nil, "", "frametest", "400x400", nil)
			if err != nil {
				t.Fatal(err)
			}
			defer disp.Close()

			ar := testar.Open(t, file)
			script := ar.ReadFile("script")
			if script == nil {
				t.Fatal("missing script")
			}
			golden := "testdata/" + name + ".gif"

			out, frames := runScript(t, file, string(script))
			drawtest.Check(t, frames, golden, *update)
			checkOutput(t, file, ar, out, *update)

			t.Run("old", func(t *testing.T) {
				out, frames := runScriptC(t, string(script))
				drawtest.Check(t, frames, golden, false)
				checkOutput(t, file, ar, out, false)
			})
		})
	}
}

func checkOutput(t *testing.T, file string, ar *testar.Archive, out string, update bool) {
	if update {
		ar.Delete("output")
		if out != "" {
			ar.WriteFile("output", []byte(out))
		}
	}
	want := string(ar.ReadFile("output"))
	if out != want {
		t.Errorf("wrong output\nhave:\n\t%s\nwant:\n\t%s",
			strings.ReplaceAll(out, "\n", "\n\t"),
			strings.ReplaceAll(want, "\n", "\n\t"))
	}
}

// runScript runs script and returns the text output and rendered frames.
func runScript(t *testing.T, file, script string) (string, []image.Image) {
	t.Helper()
	dir := t.TempDir()
	prefix := filepath.Join(dir, "img")
	t.Setenv("DEVDRAW_TEST_OUTPUT", prefix)
	disp, err := draw.Init(nil, "", "frametest", "600x400", nil)
	if err != nil {
		t.Skipf("no display: %v", err)
	}
	defer disp.Close()
	r := newRunner(t, disp)
	for pos, line := range parseScript(file, script) {
		r.runLine(t, pos, line)
	}
	return r.out.String(), drawtest.Frames(t, prefix)
}

// runScriptC runs script through the C reference implementation,
// returning the output and rendered frames.
func runScriptC(t *testing.T, script string) (string, []image.Image) {
	t.Helper()
	if _, err := os.Stat(framecover); err != nil {
		t.Fatalf("%v\nhint: cd %s && mk frametest.cover", err, plan9)
	}
	dir := t.TempDir()
	prefix := filepath.Join(dir, "img")
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	cmd := exec.CommandContext(ctx, framecover)
	cmd.Stdin = strings.NewReader(script)
	cmd.Env = append(os.Environ(),
		cov.Env(t),
		"DEVDRAW_TEST_OUTPUT="+prefix,
	)
	cmd.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	var out bytes.Buffer
	cmd.Stdout = &out
	cmd.Stderr = &out
	if err := cmd.Run(); err != nil {
		t.Errorf("frametest: %v\n%s", err, out.String())
	}
	return out.String(), drawtest.Frames(t, prefix)
}

func parseScript(file, script string) iter.Seq2[string, string] {
	return func(yield func(pos, line string) bool) {
		lineno := 0
		for line := range strings.Lines(script) {
			lineno++
			pos := fmt.Sprintf("%s:%d", file, lineno)
			if !yield(pos, line) {
				return
			}
		}
	}
}

// runner executes one frametest script against an in-process Frame and
// returns the dump output, mirroring the C oracle's frametest.c.
type runner struct {
	disp *draw.Display
	cols []*draw.Image
	font *draw.Font
	f    *Frame
	out  *bytes.Buffer
	imgs []*draw.Image // allocated images, freed by free()
}

func newRunner(t *testing.T, disp *draw.Display) *runner {
	r := &runner{
		disp: disp,
		cols: make([]*draw.Image, NCOL),
		out:  new(bytes.Buffer),
	}
	r.cols[BACK] = disp.White
	r.cols[HIGH] = r.alloc(draw.Rect(0, 0, 1, 1), true, draw.PaleYellow)
	r.cols[BORD] = r.alloc(draw.Rect(0, 0, 1, 1), true, draw.YellowGreen)
	r.cols[TEXT] = disp.Black
	r.cols[HTEXT] = disp.Black
	t.Cleanup(r.clean)
	return r
}

// alloc allocates an image and records it for freeing, so the test does not
// leak images and pile up draw finalizers across the many subtests.
func (r *runner) alloc(rect draw.Rectangle, repl bool, c draw.Color) *draw.Image {
	m, err := r.disp.AllocImage(rect, r.disp.ScreenImage.Pix, repl, c)
	if err != nil {
		panic(err)
	}
	r.imgs = append(r.imgs, m)
	return m
}

func (r *runner) clean() {
	if r.f != nil {
		r.f.Clear(true) // frees tick images
	}
	for _, m := range r.imgs {
		m.Free()
	}
	r.imgs = nil
}

// flush writes a new image output.
func (r *runner) flush() {
	screen := r.disp.ScreenImage
	screen.Draw(screen.R, r.disp.White, nil, draw.ZP)
	screen.Draw(r.f.R.Add(screen.R.Min), r.f.B, nil, r.f.R.Min)
	r.disp.Flush()
}

// runLine runs a single script line.
func (r *runner) runLine(t *testing.T, pos, line string) {
	verb, args, _ := strings.Cut(strings.TrimSpace(line), " ")
	switch verb {
	default:
		t.Fatalf("%s: unknown command: %s", pos, verb)

	case "charofpt":
		var x, y int
		fmt.Sscan(args, &x, &y)
		cp := r.f.CharOf(draw.Pt(x, y))
		fmt.Fprintf(r.out, "charofpt %d %d = %d\n", x, y, cp)

	case "clear":
		r.f.Clear(false)
		r.flush()

	case "clearfont":
		r.f.Clear(true)
		r.flush()

	case "delete":
		var q0, q1 int
		fmt.Sscan(args, &q0, &q1)
		r.f.Delete(q0, q1)
		r.flush()

	case "dump":
		r.dump()

	case "font":
		f, err := r.disp.OpenFont(args)
		if err != nil {
			t.Fatalf("%s: %v", pos, err)
		}
		r.font = f

	case "init":
		var w, h int
		fmt.Sscan(args, &w, &h)
		b := r.alloc(draw.Rect(0, 0, w, h), false, draw.White)
		r.f = new(Frame)
		if r.font == nil {
			r.font = r.disp.Font
		}
		r.f.Init(b.R, r.font, b, r.cols)
		r.flush()

	case "inittick":
		r.f.InitTick()
		r.flush()

	case "insert":
		q0s, s, _ := strings.Cut(args, " ")
		q0, _ := strconv.Atoi(q0s)
		text, err := strconv.Unquote(s)
		if err != nil {
			t.Fatalf("insert %s: %v", s, err)
		}
		r.f.Insert([]rune(text), q0)
		r.flush()

	case "output":
		r.flush()

	case "ptofchar":
		q, _ := strconv.Atoi(args)
		pt := r.f.PointOf(q)
		fmt.Fprintf(r.out, "ptofchar %d = (%d %d)\n", q, pt.X, pt.Y)

	case "redraw":
		r.f.Redraw()
		r.flush()

	case "select":
		r.f.Select(r.parseMouse(t, pos, args))
		r.flush()

	case "selscroll":
		// selscroll drives Select with a self-terminating scroll callback so the
		// scroll branch can be exercised deterministically (a live scroll would move
		// text under the held button). After two invocations it releases the button.
		mc := r.parseMouse(t, pos, args)
		n := 0
		r.f.Scroll = func(fr *Frame, _ int) {
			if n++; n >= 2 {
				mc.Mouse.Buttons = 0
			}
		}
		r.f.Select(mc)
		r.f.Scroll = nil
		r.flush()
	}
}

// parseMouse parses a sequence of x,y,b samples into a mousectl
// returning those mouse events.
func (r *runner) parseMouse(t *testing.T, pos, args string) *draw.Mousectl {
	var mouse []draw.Mouse
	for _, tok := range strings.Fields(args) {
		var x, y, b int
		fmt.Sscanf(tok, "%d,%d,%d", &x, &y, &b)
		mouse = append(mouse, draw.Mouse{Point: draw.Pt(x, y), Buttons: b})
	}
	if len(mouse) == 0 {
		t.Fatalf("%s: no mouse samples", pos)
	}
	ch := make(chan draw.Mouse, len(mouse))
	for _, m := range mouse[1:] {
		ch <- m
	}
	mc := &draw.Mousectl{C: ch, Display: r.disp}
	mc.Mouse = mouse[0]
	return mc
}

func (r *runner) dump() {
	f := r.f
	fmt.Fprintf(r.out, "frame nbox=%d nchars=%d nlines=%d maxlines=%d lastlinefull=%d modified=%d p0=%d p1=%d\n",
		len(f.box), f.NumChars, f.NumLines, f.MaxLines, b2i(f.LastLineFull), b2i(f.modified), f.P0, f.P1)
	fmt.Fprintf(r.out, "  r=(%d %d)(%d %d) entire=(%d %d)(%d %d) maxtab=%d fh=%d\n",
		f.R.Min.X, f.R.Min.Y, f.R.Max.X, f.R.Max.Y,
		f.Entire.Min.X, f.Entire.Min.Y, f.Entire.Max.X, f.Entire.Max.Y,
		f.MaxTab, f.Font.Height)
	for i := range f.box {
		b := &f.box[i]
		if b.nrune < 0 {
			fmt.Fprintf(r.out, "box[%d] brk wid=%d minwid=%d bc=%d\n", i, b.wid, b.minwid, b.bc)
		} else {
			fmt.Fprintf(r.out, "box[%d] run wid=%d nrune=%d \"%s\"\n", i, b.wid, b.nrune, b.bytes)
		}
	}
}

func b2i(b bool) int {
	if b {
		return 1
	}
	return 0
}
