package main

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"

	"9fans.net/plan9port/internal/testar"
)

// TestRaspScript runs each testdata/rasp/*.txtar script against the Go Rasp
// and compares the queries' output to the committed "output" golden
// (regenerate with -update). The "old" subtest feeds the identical script to
// the C oracle (src/cmd/osamterm/rasptest, built by mk) and requires
// byte-identical output.
//
// Script commands (one per line; see rasptest.c for the C side):
//
//	resize A OLD NEW	replace OLD runes at A with a hole of NEW
//	data P0 P1 TEXT		fill [P0,P1) with TEXT (rune length P1-P0)
//	fill P0 P1 C		fill [P0,P1) with repeated character C
//	clean			coalesce adjacent sections
//	clear			drop the sections, keep nrunes
//	init			reset to empty
//	nrunes			print "nrunes = N"
//	load P0 P1		print load's present text (holes skipped)
//	missing P0 P1		print missing
//	contig P0 P1 T		print contig (T=1 present, T=0 absent)
//	sects			print the section list: +N present, -N hole
//	#...			comment (ignored)
func TestRaspScript(t *testing.T) {
	files, err := filepath.Glob("testdata/rasp/*.txtar")
	if err != nil {
		t.Fatal(err)
	}
	if len(files) == 0 {
		t.Fatal("no testdata/rasp/*.txtar cases")
	}
	for _, file := range files {
		name := strings.TrimSuffix(filepath.Base(file), ".txtar")
		t.Run(name, func(t *testing.T) {
			ar := testar.Open(t, file)
			script := string(ar.ReadFile("script"))
			if script == "" {
				t.Fatal("missing script")
			}
			out := runRaspScript(t, script)
			if *update {
				ar.WriteFile("output", []byte(out))
				ar.Save(t)
			}
			want := string(ar.ReadFile("output"))
			if out != want {
				t.Errorf("wrong output\nhave:\n\t%s\nwant:\n\t%s",
					strings.ReplaceAll(out, "\n", "\n\t"),
					strings.ReplaceAll(want, "\n", "\n\t"))
			}
			t.Run("old", func(t *testing.T) {
				cout := runRaspScriptC(t, script)
				if cout != want {
					t.Errorf("C output differs from golden\nhave:\n\t%s\nwant:\n\t%s",
						strings.ReplaceAll(cout, "\n", "\n\t"),
						strings.ReplaceAll(want, "\n", "\n\t"))
				}
			})
		})
	}
}

// runRaspScript executes one rasp script against an in-process Rasp and
// returns the queries' output, mirroring the C oracle's rasptest.c.
func runRaspScript(t *testing.T, script string) string {
	t.Helper()
	tm := &Term{}
	r := new(Rasp)
	r.init()
	var out strings.Builder
	for line := range strings.Lines(script) {
		line = strings.TrimSuffix(line, "\n")
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		f := strings.Fields(line)
		switch f[0] {
		case "resize":
			r.resize(mustAtoi(t, f[1]), mustAtoi(t, f[2]), mustAtoi(t, f[3]))
		case "data":
			rest := strings.SplitN(line, " ", 4)
			if len(rest) < 4 {
				t.Fatalf("bad data: %q", line)
			}
			p0, p1 := mustAtoi(t, rest[1]), mustAtoi(t, rest[2])
			text := []rune(rest[3])
			if len(text) != p1-p0 {
				t.Fatalf("data: TEXT has %d runes, want %d", len(text), p1-p0)
			}
			r.data(p0, p1, text)
		case "fill":
			p0, p1 := mustAtoi(t, f[1]), mustAtoi(t, f[2])
			text := make([]rune, p1-p0)
			for i := range text {
				text[i] = rune(f[3][0])
			}
			r.data(p0, p1, text)
		case "clean":
			r.clean()
		case "clear":
			r.clear()
		case "init":
			r.init()
		case "nrunes":
			fmt.Fprintf(&out, "nrunes = %d\n", r.nrunes)
		case "load":
			p0, p1 := mustAtoi(t, f[1]), mustAtoi(t, f[2])
			fmt.Fprintf(&out, "load %d %d = \"%s\"\n", p0, p1, string(tm.load(r, p0, p1)))
		case "missing":
			p0, p1 := mustAtoi(t, f[1]), mustAtoi(t, f[2])
			fmt.Fprintf(&out, "missing %d %d = %d\n", p0, p1, r.missing(p0, p1))
		case "contig":
			p0, p1, tf := mustAtoi(t, f[1]), mustAtoi(t, f[2]), mustAtoi(t, f[3])
			fmt.Fprintf(&out, "contig %d %d %d = %d\n", p0, p1, tf, r.contig(p0, p1, tf != 0))
		case "sects":
			fmt.Fprintf(&out, "sects =")
			for s := r.sect; s != nil; s = s.next {
				if s.text != nil {
					fmt.Fprintf(&out, " +%d", s.nrunes)
				} else {
					fmt.Fprintf(&out, " -%d", s.nrunes)
				}
			}
			fmt.Fprintf(&out, " (nrunes %d)\n", r.nrunes)
		default:
			t.Fatalf("unknown command %q", line)
		}
	}
	return out.String()
}

// runRaspScriptC runs the script through the C reference implementation.
func runRaspScriptC(t *testing.T, script string) string {
	t.Helper()
	exe := filepath.Join(plan9root, "src/cmd/osamterm/rasptest")
	if _, err := os.Stat(exe); err != nil {
		t.Fatalf("%v\nhint: cd %s/src/cmd/osamterm && mk rasptest", err, plan9root)
	}
	cmd := exec.Command(exe)
	cmd.Stdin = strings.NewReader(script)
	out, err := cmd.CombinedOutput()
	if err != nil {
		t.Fatalf("rasptest: %v\n%s", err, out)
	}
	return string(out)
}
