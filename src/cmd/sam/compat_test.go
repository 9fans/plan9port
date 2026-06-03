package main

import (
	"errors"
	"io/fs"
	"syscall"
	"testing"
)

// The test binary compares the in-process Go sam against the C sam, so it
// formats errors the C way (capitalized %r). The real binary leaves this off.
func init() { compatMode = true }

// TestOserr covers oserr in both modes and all fallback branches; these are
// not reachable from the differential cases alone.
func TestOserr(t *testing.T) {
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
