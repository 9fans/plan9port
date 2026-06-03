package main

import (
	"bytes"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"testing"

	"9fans.net/plan9port/cmd/sam/internal/samproto"
	"golang.org/x/tools/txtar"
)

// Protocol (-R) differential tests. Each testdata/r/*.txtar scenario has:
//   - a comment documenting it and carrying an "args:" line (must include -R);
//   - a "script" member: one terminal action per line (see drive);
//   - input files (any other non-".out" member) written to a temp dir;
//   - a golden "host" member: the rendered Hmesg transcript.
//
// The in-process Go sam and (when present) the C sam are driven through the
// same samproto.Term; the raw Hmesg byte streams must be identical, and the Go
// transcript must match the golden. -update regenerates the golden from C.

func TestProto(t *testing.T) {
	files, _ := filepath.Glob("testdata/r/*.txtar")
	if len(files) == 0 {
		t.Skip("no testdata/r/*.txtar scenarios")
	}
	for _, file := range files {
		t.Run(strings.TrimSuffix(filepath.Base(file), ".txtar"), func(t *testing.T) {
			runProtoCase(t, file)
		})
	}
}

func runProtoCase(t *testing.T, file string) {
	data, err := os.ReadFile(file)
	if err != nil {
		t.Fatal(err)
	}
	ar := txtar.Parse(data)
	args := parseArgsLine(ar.Comment)

	var script []byte
	wantHost, haveHost := []byte(nil), false
	inputs := map[string][]byte{}
	for _, f := range ar.Files {
		data := txtarDecode(f.Data)
		switch f.Name {
		case "script":
			script = data
		case "host":
			wantHost, haveHost = data, true
		default:
			inputs[f.Name] = data
		}
	}
	actions := splitActions(script)

	setup := func(dir string) {
		for name, content := range inputs {
			if err := os.WriteFile(filepath.Join(dir, name), content, 0644); err != nil {
				t.Fatal(err)
			}
		}
	}

	hasC := haveOracle()

	if *update {
		if !hasC {
			t.Skipf("-update needs the C sam at %s", *samBin)
		}
		dir := t.TempDir()
		setup(dir)
		_, transcript := runProtoC(t, dir, args, actions)
		newFiles := []txtar.File{{Name: "script", Data: txtarEncode(script)}}
		for name, content := range inputs {
			newFiles = append(newFiles, txtar.File{Name: name, Data: txtarEncode(content)})
		}
		newFiles = append(newFiles, txtar.File{Name: "host", Data: txtarEncode([]byte(transcript))})
		ar.Files = newFiles
		if err := os.WriteFile(file, txtar.Format(ar), 0644); err != nil {
			t.Fatal(err)
		}
		return
	}

	dir := t.TempDir()
	// Resolve symlinks so the cwd the Go run sees (via os.Getwd/$PWD) matches
	// what the C oracle's getwd() reports; some messages (Tplumb) embed it.
	if r, err := filepath.EvalSymlinks(dir); err == nil {
		dir = r
	}
	setup(dir)
	t.Chdir(dir)
	goRaw, goTranscript := runProtoGo(t, args, actions)

	if haveHost && goTranscript != string(wantHost) {
		t.Errorf("transcript mismatch:\n--- got:\n%s\n--- want:\n%s", goTranscript, wantHost)
	}

	if hasC {
		// Run the oracle in the same directory so any absolute paths it emits
		// match the Go run; restore the inputs first in case the Go run changed
		// them on disk.
		setup(dir)
		cRaw, cTranscript := runProtoC(t, dir, args, actions)
		if !bytes.Equal(goRaw, cRaw) {
			// The transcript is a 1:1 human-readable rendering of the Hmesg
			// frames, so a line diff shows the divergence clearly.
			d := diffLines("go", filepath.Base(*samBin), goTranscript, cTranscript)
			if d == "" {
				// Transcripts matched but raw bytes did not (a difference only
				// in fields the renderer omits); fall back to hex.
				d = fmt.Sprintf("(transcripts equal; raw bytes differ)\n--- go:\n%s\n--- %s:\n%s",
					hexdump(goRaw), filepath.Base(*samBin), hexdump(cRaw))
			}
			t.Errorf("Hmesg streams differ:\n%s", d)
		}
	}
}

// diffLines returns a unified-style line diff of a vs b (empty if equal),
// labeling lines unique to each side. nameA/nameB label the two sources.
func diffLines(nameA, nameB, a, b string) string {
	al := strings.Split(strings.TrimRight(a, "\n"), "\n")
	bl := strings.Split(strings.TrimRight(b, "\n"), "\n")
	// Longest common subsequence of the two line slices.
	na, nb := len(al), len(bl)
	lcs := make([][]int, na+1)
	for i := range lcs {
		lcs[i] = make([]int, nb+1)
	}
	for i := na - 1; i >= 0; i-- {
		for j := nb - 1; j >= 0; j-- {
			if al[i] == bl[j] {
				lcs[i][j] = lcs[i+1][j+1] + 1
			} else if lcs[i+1][j] >= lcs[i][j+1] {
				lcs[i][j] = lcs[i+1][j]
			} else {
				lcs[i][j] = lcs[i][j+1]
			}
		}
	}
	var sb strings.Builder
	differ := false
	i, j := 0, 0
	for i < na && j < nb {
		switch {
		case al[i] == bl[j]:
			fmt.Fprintf(&sb, "  %s\n", al[i])
			i++
			j++
		case lcs[i+1][j] >= lcs[i][j+1]:
			fmt.Fprintf(&sb, "- %s: %s\n", nameA, al[i])
			i++
			differ = true
		default:
			fmt.Fprintf(&sb, "+ %s: %s\n", nameB, bl[j])
			j++
			differ = true
		}
	}
	for ; i < na; i++ {
		fmt.Fprintf(&sb, "- %s: %s\n", nameA, al[i])
		differ = true
	}
	for ; j < nb; j++ {
		fmt.Fprintf(&sb, "+ %s: %s\n", nameB, bl[j])
		differ = true
	}
	if !differ {
		return ""
	}
	return sb.String()
}

// splitActions returns the non-empty lines of the script.
func splitActions(script []byte) []string {
	var out []string
	for _, line := range strings.Split(string(script), "\n") {
		if strings.TrimSpace(line) != "" {
			out = append(out, line)
		}
	}
	return out
}

func drive(term *samproto.Term, actions []string) {
	term.Start()
	exited := false
	for _, a := range actions {
		verb, rest, _ := strings.Cut(a, " ")
		f := strings.Fields(rest)
		num := func(i int) int {
			if i < len(f) {
				n, _ := strconv.Atoi(f[i])
				return n
			}
			return 0
		}
		switch verb {
		case "type":
			term.Type(unescape(rest))
		case "startfile":
			term.StartFile(num(0))
		case "startnewfile":
			term.Startnewfile()
		case "origin":
			term.Origin(num(0), num(1), num(2))
		case "check":
			term.Check(num(0))
		case "noauto":
			term.SetAutoRequest(false)
		case "request":
			term.Request(num(0), num(1), num(2))
		case "search":
			term.Search()
		case "workfile":
			term.Workfile(num(0), num(1), num(2))
		case "cut":
			term.Cut(num(0), num(1), num(2))
		case "snarf":
			term.Snarf(num(0), num(1), num(2))
		case "plumb":
			term.Plumb(num(0), num(1), num(2))
		case "paste":
			term.Paste(num(0), num(1))
		case "startsnarf":
			term.Startsnarf()
		case "setsnarf":
			term.Setsnarf(unescape(rest))
		case "send":
			term.Send(num(0), num(1))
		case "sendtext":
			term.SendText(unescape(rest))
		case "typeat":
			term.TypeAt(num(0), num(1), unescape(strings.Join(f[2:], " ")))
		case "dclick":
			term.Dclick(num(0), num(1))
		case "look":
			term.Look(num(0), num(1), num(2))
		case "write":
			term.Write(num(0))
		case "close":
			term.Close(num(0))
		case "closeconn":
			term.CloseConn()
			exited = true
		case "exit":
			term.Exit()
			exited = true
		}
	}
	if !exited {
		term.Exit()
	}
}

func runProtoGo(t *testing.T, args, actions []string) ([]byte, string) {
	inR, inW := io.Pipe()   // term -> host stdin
	outR, outW := io.Pipe() // host stdout -> term
	var errb bytes.Buffer
	done := make(chan struct{})
	go func() {
		defer close(done)
		defer outW.Close()
		// Close the host's stdin when it exits so a terminal still writing (e.g.
		// the raw bytes after Tsetsnarf) unblocks instead of deadlocking on the
		// synchronous pipe; a real sam exiting closes its stdin the same way.
		defer inR.Close()
		Run(args, inR, outW, &errb)
	}()
	term := samproto.New(outR, inW)
	drive(term, actions)
	<-done
	return term.Raw(), term.Transcript()
}

func runProtoC(t *testing.T, dir string, args, actions []string) ([]byte, string) {
	cmd := newOracleCmd(args)
	cmd.Dir = dir
	stdin, _ := cmd.StdinPipe()
	stdout, _ := cmd.StdoutPipe()
	var errb bytes.Buffer
	cmd.Stderr = &errb
	if err := cmd.Start(); err != nil {
		t.Fatal(err)
	}
	term := samproto.New(stdout, stdin)
	drive(term, actions)
	cmd.Wait()
	return term.Raw(), term.Transcript()
}

func unescape(s string) string {
	r := strings.NewReplacer(`\n`, "\n", `\t`, "\t", `\\`, `\`)
	return r.Replace(s)
}

func hexdump(b []byte) string {
	var sb strings.Builder
	for i, c := range b {
		if i > 0 && i%16 == 0 {
			sb.WriteByte('\n')
		}
		sb.WriteString(strconv.Itoa(int(c)))
		sb.WriteByte(' ')
	}
	return sb.String()
}
