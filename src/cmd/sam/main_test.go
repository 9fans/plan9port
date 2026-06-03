package main

import (
	"bytes"
	"os"
	"strings"
	"testing"
)

// TestArgs exercises flag parsing and the startup paths that the txtar cases
// don't reach (no file argument, samterm-only flags, EARGF flags, unknown
// flag, "--", and -R startup hitting EOF -> rescue).
func TestArgs(t *testing.T) {
	cases := [][]string{
		{"-d"},                  // no files: opens a nameless file
		{"-d", "--"},            // end-of-flags marker
		{"-a", "-d"},            // samterm option, ignored
		{"-t", "samterm", "-d"}, // EARGF flag (separate arg)
		{"-ssam", "-d"},         // EARGF flag (attached)
		{"-r", "host", "-d"},    // machine flag (connect is a no-op here)
		{"-z", "-d"},            // unknown flag -> usage
		{"-R"},                  // downloaded startup, then EOF -> rescue
	}
	for _, args := range cases {
		t.Run(strings.Join(args, "_"), func(t *testing.T) {
			t.Chdir(t.TempDir())
			var out, errb bytes.Buffer
			// Empty stdin: -d reaches EOF immediately; -R rescues on EOF.
			Run(args, strings.NewReader(""), &out, &errb)
		})
	}
}

// TestCd exercises a bare cd (which targets $HOME) deterministically by making
// $HOME the current directory, so the working directory does not actually move.
func TestCd(t *testing.T) {
	dir := t.TempDir()
	t.Chdir(dir)
	t.Setenv("HOME", dir)
	if err := os.WriteFile("input", []byte("alpha\n"), 0644); err != nil {
		t.Fatal(err)
	}
	var out, errb bytes.Buffer
	Run([]string{"-d", "input"}, strings.NewReader("cd\n,p\n"), &out, &errb)
}
