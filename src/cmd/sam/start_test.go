package main

import (
	"bytes"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"sync"
	"syscall"
	"testing"
	"time"
)

// TestStartSamterm verifies that the Go sam can launch samterm (the GUI front
// end) and drive it: it runs the freshly built Go sam with `-t <samterm>` on a
// headless devdraw (DEVDRAW_TEST_MEMORY), feeds a scripted "q" via
// DEVDRAW_TEST_INPUT, and checks that samterm came up, rendered a window, and
// the whole thing quit cleanly. As a differential cross-check, the C sam
// launching the same samterm must render the identical final window.

const cSamterm = "/usr/local/plan9/bin/samterm"

var (
	goSamOnce sync.Once
	goSamBin  string
	goSamErr  error
)

func buildGoSam(t *testing.T) string {
	t.Helper()
	goSamOnce.Do(func() {
		dir, err := os.MkdirTemp("", "gosam")
		if err != nil {
			goSamErr = err
			return
		}
		goSamBin = filepath.Join(dir, "sam")
		cmd := exec.Command("go", "build", "-o", goSamBin, ".")
		cmd.Stderr = os.Stderr
		goSamErr = cmd.Run()
	})
	if goSamErr != nil {
		t.Fatalf("building go sam: %v", goSamErr)
	}
	return goSamBin
}

func TestStartSamterm(t *testing.T) {
	if _, err := os.Stat(cSamterm); err != nil {
		t.Skipf("no samterm at %s", cSamterm)
	}
	goSam := buildGoSam(t)

	dir := t.TempDir()
	file := filepath.Join(dir, "f")
	if err := os.WriteFile(file, []byte("hello from sam\n"), 0666); err != nil {
		t.Fatal(err)
	}
	// Type "q<enter>" in the command window so sam quits, samterm exits, and
	// the whole pipeline shuts down cleanly.
	script := filepath.Join(dir, "script")
	if err := os.WriteFile(script, []byte("delay 700\ntype q\nkey enter\ndelay 400\n"), 0666); err != nil {
		t.Fatal(err)
	}

	goImg, goClean := runStart(t, goSam, cSamterm, file, script, filepath.Join(dir, "go"))
	if goImg == nil {
		t.Fatal("go sam did not start samterm: no window frames were rendered")
	}
	if !goClean {
		t.Errorf("go sam + samterm did not exit cleanly (had to be killed)")
	}

	// Differential: the C sam launching the same samterm must render the same
	// final window, confirming go sam starts and drives it identically.
	cImg, _ := runStart(t, "/usr/local/plan9/bin/sam", cSamterm, file, script, filepath.Join(dir, "c"))
	if cImg != nil && !bytes.Equal(goImg, cImg) {
		t.Errorf("go sam and C sam launch different samterm windows (%d vs %d bytes)", len(goImg), len(cImg))
	}
}

// runStart runs `samBin -W 600x400 -t termBin file` headless with the input
// script, returning the final rendered frame and whether it exited on its own.
func runStart(t *testing.T, samBin, termBin, file, script, prefix string) (img []byte, clean bool) {
	t.Helper()
	cmd := exec.Command(samBin, "-W", "600x400", "-t", termBin, file)
	cmd.Env = append(os.Environ(),
		"DEVDRAW_TEST_MEMORY=1",
		"DEVDRAW_TEST_OUTPUT="+prefix,
		"DEVDRAW_TEST_INPUT="+script,
	)
	cmd.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	errf, err := os.Create(prefix + ".err")
	if err != nil {
		t.Fatal(err)
	}
	defer errf.Close()
	cmd.Stdout = errf
	cmd.Stderr = errf
	if err := cmd.Start(); err != nil {
		t.Fatalf("start %s: %v", samBin, err)
	}
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	select {
	case <-done:
		clean = true
	case <-time.After(4 * time.Second):
		syscall.Kill(-cmd.Process.Pid, syscall.SIGKILL)
		<-done
	}
	return lastFrame(prefix), clean
}

func lastFrame(prefix string) []byte {
	matches, _ := filepath.Glob(prefix + ".*")
	best, bestN := "", -1
	for _, m := range matches {
		n, err := strconv.Atoi(m[len(prefix)+1:])
		if err != nil {
			continue
		}
		if n > bestN {
			bestN, best = n, m
		}
	}
	if best == "" {
		return nil
	}
	data, _ := os.ReadFile(best)
	return data
}
