package main

import (
	"bytes"
	"flag"
	"io"
	"log"
	"maps"
	"os"
	"os/exec"
	"path/filepath"
	"slices"
	"strconv"
	"strings"
	"sync"
	"testing"
	"testing/synctest"

	"9fans.net/plan9port/internal/llvmcover"
	"9fans.net/plan9port/internal/testar"
)

// Script tests checking sam -d and sam -R behavior.
//
// Each testdata/d/*.txtar archive describes a test case.
//
//  - The comment documents the point of the case.
//  - The args file, if present, specifies the command arguments.
//     If omitted, the args are -d <list of inputs> or -R <list of inputs>,
//     depending on whether the proto file is present.
//  - The stdin file, if present, specifies the input to send to sam.
//  - The stdout file, if present, specifies the stdout expected from sam.
//    If missing, no stdout is expected.
//  - The proto file, if present, contains a samterm wire protocol dump.
//    This replaces stdin and stdout and changes the default args to use -R.
//    The protocol sequence is replayed, sending T messages and expecting H messages.
//  - The stderr file, if present, specifies the stderr expected from sam.
//    If missing, no stderr is expected.
//  - Files named <base>.out specify the final content of the file named <base>
//    when sam finishes.
//  - Other files name input files to write before starting sam.
//
// The files are lightly encoded in the txtar format: ^@ for a NUL byte, ^J to mark
// an end of a line actually ending in space or tab (to avoid losing them in editors),
// and ^D at the end of the last line if the output does not end in a newline.
// See internal/testar for details.
//
// The -update flag regenerates the testdata with stdout, stderr, and output files
// matching the current sam behavior.
//
// By convention, the -d tests are in testdata/d/*.txtar and the -R tests are in testdata/r/*.txtar.

var (
	plan9, _  = filepath.Abs("/usr/local/plan9") // plan9port root
	oldSam    = flag.String("old", filepath.Join(plan9, "bin/osam"), "test against old sam binary `exe`")
	update    = flag.Bool("update", false, "regenerate golden files from the old C sam")
	osamcover = filepath.Join(plan9, "src/cmd/osam/o.osam.cover")
	cov       = llvmcover.New(osamcover, "")
)

// oldExe returns the old sam executable to use:
// either the -old flag or osamcover if -uncoverc is enabled.
func oldExe() string {
	if cov.Enabled() {
		// TODO: Change -old to default empty and reject use of uncoverc with old.
		return osamcover
	}
	return *oldSam
}

func TestMain(m *testing.M) {
	code := m.Run()
	if err := cov.Finish(); err != nil {
		log.Fatal(err)
	}
	os.Exit(code)
}

func TestStream(t *testing.T) {
	runScripts(t, "testdata/d/*.txtar")
}

func TestProto2(t *testing.T) {
	runScripts(t, "testdata/r/*.txtar")
}

func runScripts(t *testing.T, glob string) {
	files, err := filepath.Glob(glob)
	if err != nil {
		t.Fatal(err)
	}
	if len(files) == 0 {
		t.Skipf("no %s cases", glob)
	}
	for _, file := range files {
		t.Run(strings.TrimSuffix(filepath.Base(file), ".txtar"), func(t *testing.T) {
			dir := t.TempDir()
			raw := runScript(t, dir, file, runLocal, *update, nil)
			if oldExe() != "" {
				t.Run("old", func(t *testing.T) {
					runScript(t, dir, file, runOld, false, raw)
				})
			}
			if t.Failed() {
				t.Logf("script: %s", file)
			}
		})
	}
}

func runScript(t *testing.T, dir, file string, run func(t *testing.T, dir string, args []string, stdin io.Reader, stdout, stderr io.Writer), update bool, raw []byte) []byte {
	ar := testar.Open(t, file)

	// Write and collect names of inputs.
	os.RemoveAll(dir)
	os.Mkdir(dir, 0777)
	t.Setenv("HOME", dir)       // for writing sam.save
	t.Setenv("PLAN9", "/plan9") // for writing into sam.save
	t.Setenv("USER", "tester")  // for consistent 'more in ...' file names
	args := []string{"-d"}
	for _, f := range ar.Files {
		if f.Name == "proto" {
			args[0] = "-R"
			continue
		}
		if f.Name == "args" || f.Name == "stdin" || f.Name == "stdout" || f.Name == "stderr" || strings.HasSuffix(f.Name, ".out") {
			continue
		}
		args = append(args, f.Name)
		if err := os.WriteFile(filepath.Join(dir, f.Name), f.Data, 0666); err != nil {
			t.Fatal(err)
		}
	}

	// Override command-line if args file is present.
	if data := ar.ReadFile("args"); data != nil {
		args = strings.Fields(string(data))
	}

	// Run sam.
	output := make(map[string][]byte)
	switch args[0] {
	default:
		t.Fatalf("unknown mode %s (want -d or -R)", args[0])

	case "-d":
		var stdout, stderr bytes.Buffer
		run(t, dir, args, bytes.NewReader(ar.ReadFile("stdin")), &stdout, &stderr)
		if stdout.Len() > 0 {
			output["stdout"] = stdout.Bytes()
		}
		if stderr.Len() > 0 {
			output["stderr"] = stderr.Bytes()
		}

	case "-R":
		synctest.Test(t, func(t *testing.T) {
			inR, inW := io.Pipe()   // term -> host stdin
			outR, outW := io.Pipe() // host stdout -> term
			term := NewTerm(outR, inW)
			done := make(chan bool, 1)
			go func() {
				_, proto := ar.Find("proto")
				if proto == nil {
					t.Fatalf("missing proto file in txtar")
				}
				runProto(t, term, proto, update)
				outR.Close()
				inW.Close()
				done <- true
			}()
			var stderr bytes.Buffer
			run(t, dir, args, inR, outW, &stderr)
			outW.Close()
			inR.Close()
			if stderr.Len() > 0 {
				output["stderr"] = stderr.Bytes()
			}
			<-done
			if raw == nil {
				raw = term.Raw()
			}
			if !t.Failed() {
				have := term.Raw()
				if !bytes.Equal(have, raw) {
					t.Fatalf("raw protocol messages differ")
				}
			}
		})
	}

	// Read outputs.
	infos, err := os.ReadDir(dir)
	if err != nil {
		t.Fatal(err)
	}
	for _, info := range infos {
		name := info.Name()
		data, err := os.ReadFile(filepath.Join(dir, name))
		if err != nil {
			t.Fatal(err)
		}
		if data == nil {
			data = []byte{}
		}
		if input := ar.ReadFile(name); input == nil || !bytes.Equal(data, input) {
			output[name+".out"] = data
		}
	}

	// Update golden if -update is specified.
	if update {
		for _, f := range slices.Clone(ar.Files) {
			if f.Name == "stdout" || f.Name == "stderr" || strings.HasSuffix(f.Name, ".out") {
				ar.Delete(f.Name)
			}
		}
		if output["stdout"] != nil {
			ar.WriteFile("stdout", output["stdout"])
		}
		if output["stderr"] != nil {
			ar.WriteFile("stderr", output["stderr"])
			if !bytes.Equal(ar.ReadFile("stderr"), output["stderr"]) {
				panic("readwrite")
			}
		}
		for _, name := range slices.Sorted(maps.Keys(output)) {
			ar.WriteFile(name, output[name])
		}
		ar.Save(t)
	}

	// Check outputs against golden.
	for _, name := range slices.Sorted(maps.Keys(output)) {
		have := output[name]
		want := ar.ReadFile(name)
		if want == nil {
			t.Errorf("unexpected output %s; have:\n«%s»", name, testar.Encode(have))
			continue
		}
		if !bytes.Equal(have, want) {
			t.Errorf("incorrect output %s; have:\n«%s»\nwant:\n«%s»", name, testar.Encode(have), testar.Encode(want))
		}
	}
	for _, f := range ar.Files {
		if f.Name == "stdout" || f.Name == "stderr" || strings.HasSuffix(f.Name, ".out") {
			if output[f.Name] == nil {
				t.Errorf("missing output %s; want:\n«%s»", f.Name, testar.Encode(f.Data))
			}
		}
	}
	return raw
}

func runLocal(t *testing.T, dir string, args []string, stdin io.Reader, stdout, stderr io.Writer) {
	wd, err := os.Getwd()
	if err != nil {
		t.Fatal(err)
	}
	defer func() {
		if err := os.Chdir(wd); err != nil {
			t.Fatal(err)
		}
	}()
	if err := os.Chdir(dir); err != nil {
		t.Fatal(err)
	}
	Run(args, stdin, stdout, stderr)
}

func runOld(t *testing.T, dir string, args []string, stdin io.Reader, stdout, stderr io.Writer) {
	old := runOldSam(t, dir, args)
	var wg sync.WaitGroup
	wg.Go(func() {
		io.Copy(old.stdin, stdin)
		old.stdin.Close()
	})
	wg.Go(func() {
		io.Copy(stdout, old.stdout)
		if c, ok := stdout.(io.Closer); ok {
			c.Close()
		}
	})
	wg.Wait()
	old.wait()
	io.Copy(stderr, old.stderr)
}

// An OldSam is a running instance of an old sam binary.
type OldSam struct {
	cmd    *exec.Cmd
	stdin  *os.File // pipe
	stdout *os.File // pipe
	stderr *bytes.Buffer
}

// runOldSam starts an old sam binary in the given directory with the given args.
// The caller must call old.wait and then can access old.stderr.
func runOldSam(t *testing.T, dir string, args []string) *OldSam {
	cmd := exec.Command(oldExe(), args...)
	cmd.Env = append(os.Environ(), cov.Env(t))
	cmd.Dir = dir
	stdinR, stdinW, err := os.Pipe()
	if err != nil {
		t.Fatal(err)
	}
	stdoutR, stdoutW, err := os.Pipe()
	if err != nil {
		t.Fatal(err)
	}
	cmd.Stdin = stdinR
	cmd.Stdout = stdoutW
	stderr := new(bytes.Buffer)
	cmd.Stderr = stderr
	if err := cmd.Start(); err != nil {
		t.Fatal(err)
	}
	stdinR.Close()
	stdoutW.Close()
	return &OldSam{
		cmd:    cmd,
		stdin:  stdinW,
		stdout: stdoutR,
		stderr: stderr,
	}
}

func (o *OldSam) wait() error {
	o.stdin.Close()
	return o.cmd.Wait()
}

func runProto(t *testing.T, term *ScriptTerm, proto *testar.File, update bool) {
	if update {
		var out []string
		out = append(out, term.Wait()...)
		for line := range strings.Lines(string(proto.Data)) {
			line = strings.TrimSpace(line)
			if strings.HasPrefix(line, "H") {
				continue
			}
			runProtoLine(t, term, line)
			out = append(out, line)
			out = append(out, term.Wait()...)
		}
		proto.Data = []byte(strings.Join(out, "\n") + "\n")
		return
	}

	for line := range strings.Lines(string(proto.Data)) {
		line = strings.TrimSpace(line)
		if strings.HasPrefix(line, "H") {
			if err := term.Expect(line); err != nil {
				t.Error(err)
				return
			}
		} else {
			runProtoLine(t, term, line)
		}
	}
	if extra := term.ExpectEOF(); len(extra) > 0 {
		t.Errorf("unexpected trailing host messages: %v", extra)
	}
}

func runProtoLine(t *testing.T, term *ScriptTerm, a string) {
	verb, rest, _ := strings.Cut(a, " ")
	f := strings.Fields(rest)
	num := func(i int) int {
		if i < len(f) {
			n, _ := strconv.Atoi(f[i])
			return n
		}
		return 0
	}
	n := -1
	short := func() uint16 {
		n++
		return uint16(num(n))
	}
	long := func() uint32 {
		n++
		return uint32(num(n))
	}

	switch verb {
	default:
		t.Fatalf("unknown proto verb %s", verb)

	case "cmd":
		term.Cmd(unescape(rest))
	case "crash":
		term.Send(0xFF) // protocol error
	case "shutdown":
		term.Shutdown()

	case "Tack":
		term.Send(Tack)
	case "Tclose":
		term.Send(Tclose, short())
	case "Tcheck":
		term.Send(Tcheck, short())
	case "Tcut":
		term.Send(Tcut, short(), long(), long())
	case "Tdclick":
		term.Send(Tdclick, short(), long())
	case "Texit":
		term.Send(Texit)
	case "Tlook":
		term.Send(Tlook, short(), long(), long())
	case "Torigin":
		term.Send(Torigin, short(), long(), long())
	case "Tpaste":
		term.Send(Tpaste, short(), long())
	case "Tplumb":
		term.Send(Tplumb, short(), long(), long())
	case "Trequest":
		term.Send(Trequest, short(), long(), long())
	case "Tsearch":
		term.Send(Tsearch, uint16(0), uint32(0), uint32(0))
	case "Tsend":
		term.Send(Tsend, uint16(0), long(), long())
	case "Tsetsnarf":
		snarf := unescape(rest)
		term.Send(Tsetsnarf, uint16(len(snarf)))
		term.WriteRaw([]byte(snarf))
	case "Tsnarf":
		term.Send(Tsnarf, short(), long(), long())
	case "Tstartcmdfile":
		term.Send(Tstartcmdfile, uint64(CmdFile))
	case "Tstartfile":
		tag := short()
		term.Send(Tstartfile, tag, 0x2000+uint64(tag))
	case "Tstartnewfile":
		term.Send(Tstartnewfile, uint64(0x3000))
	case "Tstartsnarf":
		term.Send(Tstartsnarf)
	case "Ttype":
		term.Send(Ttype, short(), long(), unescape(strings.Join(f[2:], " ")))
	case "Tversion":
		term.Send(Tversion, short())
	case "Tworkfile":
		term.Send(Tworkfile, short(), long(), long())
	case "Twrite":
		term.Send(Twrite, short())
	}
}

func unescape(s string) string {
	r := strings.NewReplacer(`\n`, "\n", `\t`, "\t", `\\`, `\`)
	return r.Replace(s)
}
