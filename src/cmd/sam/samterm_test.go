package main

import (
	"image"
	"os"
	"os/exec"
	"path/filepath"
	"syscall"
	"testing"
	"time"

	"9fans.net/plan9port/internal/drawtest"
)

func init() {
	if os.Getenv("TESTSAM") == "1" {
		println("running as sam")
		main()
		os.Exit(0)
	}
}

func TestStartSamterm(t *testing.T) {
	dir := t.TempDir()
	file := filepath.Join(dir, "f")
	if err := os.WriteFile(file, []byte("hello from sam\n"), 0666); err != nil {
		t.Fatal(err)
	}

	script := "waitframe 11\n" +
		"type q\n" +
		"waitframe 12\n" +
		"key enter\n" +
		"delay 400\n"
	scriptFile := filepath.Join(dir, "script")
	if err := os.WriteFile(scriptFile, []byte(script), 0666); err != nil {
		t.Fatal(err)
	}

	samExe, err := os.Executable()
	if err != nil {
		t.Fatal(err)
	}
	const golden = "testdata/startsamterm.gif"
	frames := runSam(t, samExe, file, scriptFile)
	drawtest.Check(t, frames, golden, *update)

	if oldExe() != "" {
		t.Run("old", func(t *testing.T) {
			frames := runSam(t, oldExe(), file, scriptFile)
			drawtest.Check(t, frames, golden, false)
		})
	}
}

// runSam runs `sam -W 600x400 file` headless with the input
// script, returning the rendered frame files (in order).
func runSam(t *testing.T, samExe, file, script string) []image.Image {
	img := filepath.Join(t.TempDir(), "img")
	const timeout = 10 * time.Second
	t.Helper()
	cmd := exec.Command(samExe, "-t", "../../../bin/samterm", file)
	cmd.Env = append(os.Environ(),
		"TESTSAM=1",
		"DEVDRAW=../../../bin/devdraw",
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
