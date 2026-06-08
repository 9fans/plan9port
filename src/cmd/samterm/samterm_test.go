package main

import (
	"fmt"
	"image"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"syscall"
	"testing"
	"time"

	"9fans.net/plan9port/internal/drawtest"
	"9fans.net/plan9port/internal/testar"
)

const plan9 = "/usr/local/plan9"

// startupFrames is the deterministic number of frames samterm emits bringing up
// the window and drawing the file (frames 0..startupFrames-1) before any
// scripted input.
const startupFrames = 11

func init() {
	if os.Getenv("TESTSAMTERM") == "1" {
		main()
		os.Exit(0)
	}
}

// A step is one scripted input command and the number of window frames it
// produces. Every drawing event (a keystroke, click, button, resize) flushes
// exactly once; a bare mouse move does not flush, so it produces none. The
// script generator (pacedScript) waits for exactly that many frames after the
// command, keeping the sequence deterministic.
type step struct {
	cmd    string
	frames int
}

// typeText expands a string into one keystroke step per rune. Spaces and tabs
// are sent via "key" because the "type" command's argument parser strips leading
// whitespace; everything else via "type".
func typeText(s string) []step {
	var steps []step
	for _, r := range s {
		switch r {
		case ' ':
			steps = append(steps, step{"key space", 1})
		case '\t':
			steps = append(steps, step{"key tab", 1})
		default:
			steps = append(steps, step{"type " + string(r), 1})
		}
	}
	return steps
}

// parseSteps turns a scenario's input section (one high-level command per line)
// into paced steps. A "type TEXT" line expands to one keystroke step per rune
// (typeText); every other command is a single step that produces one frame,
// except a bare "move", which produces none.
func parseSteps(input []byte) []step {
	var steps []step
	for line := range strings.Lines(string(input)) {
		line = strings.TrimSpace(line)
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		if text, ok := strings.CutPrefix(line, "type "); ok {
			steps = append(steps, typeText(unescape(text))...)
		} else if strings.HasPrefix(line, "move ") {
			steps = append(steps, step{line, 0})
		} else {
			steps = append(steps, step{line, 1})
		}
	}
	return steps
}

//FIX: change to Go quoted string syntax and use strconv.Unquote
// unescape resolves the \t \n \\ escapes used in scenario text: the txtar files
// hold one command per line, so tabs and newlines inside typed text are escaped.
func unescape(s string) string {
	var b strings.Builder
	for i := 0; i < len(s); i++ {
		if s[i] == '\\' && i+1 < len(s) {
			i++
			switch s[i] {
			case 't':
				b.WriteByte('\t')
			case 'n':
				b.WriteByte('\n')
			case '\\':
				b.WriteByte('\\')
			default:
				b.WriteByte('\\')
				b.WriteByte(s[i])
			}
		} else {
			b.WriteByte(s[i])
		}
	}
	return b.String()
}

// pacedScript turns a step list into a DEVDRAW_TEST_INPUT script that waits for
// the output frame stream after every event, so the capture is deterministic.
func pacedScript(steps []step) string {
	var b strings.Builder
	n := startupFrames
	fmt.Fprintf(&b, "waitframe %d\n", n)
	for _, s := range steps {
		b.WriteString(s.cmd)
		b.WriteByte('\n')
		n += s.frames
		fmt.Fprintf(&b, "waitframe %d\n", n)
	}
	return b.String()
}

func TestDraw(t *testing.T) {
	files, err := filepath.Glob("testdata/draw/*.txtar")
	if err != nil {
		t.Fatal(err)
	}
	samtermExe, err := os.Executable()
	if err != nil {
		t.Fatal(err)
	}
	const samExe = plan9 + "/bin/sam"
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

			//FIX: run sam inside the temp directory instead of needing the hard-coded path?
			// Use a stable directory (not t.TempDir's random name) so any
			// path sam echoes into the window is identical for the C and Go
			// runs (the difference would otherwise be a spurious mismatch).
			dir := "/tmp/samterm-test/" + name
			os.RemoveAll(dir)
			if err := os.MkdirAll(dir, 0777); err != nil {
				t.Fatal(err)
			}
			t.Cleanup(func() { os.RemoveAll(dir) })

			file := filepath.Join(dir, "f")
			if err := os.WriteFile(file, []byte("hello world\n"), 0666); err != nil {
				t.Fatal(err)
			}
			scriptFile := filepath.Join(dir, "script")
			if err := os.WriteFile(scriptFile, []byte(pacedScript(steps)), 0666); err != nil {
				t.Fatal(err)
			}

			golden := "testdata/draw/" + name + ".gif"
			frames := runSam(t, samtermExe, file, scriptFile)
			drawtest.Check(t, frames, golden, *update)

			if haveOld() {
				t.Run("old", func(t *testing.T) {
					frames := runSam(t, oldExe(), file, scriptFile)
					drawtest.Check(t, frames, golden, false)
				})
			}
		})
	}
}

// runSam runs `sam -W 600x400 file` headless with the input
// script, returning the rendered frame files (in order).
func runSam(t *testing.T, samtermExe, file, script string) []image.Image {
	img := filepath.Join(t.TempDir(), "img")
	const timeout = 3 * time.Second
	t.Helper()
	cmd := exec.Command(plan9+"/bin/sam", "-W", "600x400", "-t", samtermExe, file)
	os.Unsetenv("font")
	cmd.Env = append(os.Environ(),
		"TESTSAMTERM=1",
		"DEVDRAW="+filepath.Join(plan9, "bin/devdraw"),
		"DEVDRAW_TEST_MEMORY=1",
		"DEVDRAW_TEST_INPUT="+script,
		"DEVDRAW_TEST_OUTPUT="+img,
	)
	cmd.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Start(); err != nil {
		t.Fatal(err)
	}

	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	select {
	case <-done:
		// ok
	case <-time.After(timeout):
		syscall.Kill(-cmd.Process.Pid, syscall.SIGKILL)
		t.Errorf("sam did not exit after %v; killed", timeout)
	}
	return drawtest.Frames(t, img)
}

func haveOld() bool {
	return true
}

func oldExe() string {
	return filepath.Join(plan9, "bin/osamterm")
}
