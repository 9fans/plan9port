package main

import (
	"bytes"
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"sync"
	"testing"

	"golang.org/x/tools/txtar"
)

// Stream (-d) differential tests. Each testdata/d/*.txtar archive is a case:
//   - the comment documents the case and carries an "args:" line;
//   - files "stdin", "stdout", "stderr" are those streams;
//   - any "name.out" file is the expected content of "name" after the run;
//   - all other files are inputs written into the per-case temp dir.
//
// The in-process Go sam is always checked against the golden streams. When the
// oracle sam (-sam, default /usr/local/plan9/bin/sam) is installed, it is also
// run and must agree. -update regenerates the golden streams and *.out members
// from the oracle.

var update = flag.Bool("update", false, "regenerate golden files from the C sam oracle")
var samBin = flag.String("sam", "/usr/local/plan9/bin/sam", "sam binary to test against (the oracle)")
var oldsam = flag.Bool("oldsam", true, "also run the old (C) sam at -sam and require agreement")

// Coverage flags. With -uncoverc set, the oracle is the coverage build
// sam.cover (see src/cmd/sam/mkfile: "mk cover.install"); every oracle run writes an
// LLVM profile, and after the tests finish their merged coverage is fed to
// uncover -bin (rsc.io/uncover), which writes the C lines no run exercised.
var uncovercOut = flag.String("uncoverc", "", "write the C source lines not covered by the oracle to `file` (uses sam.cover)")
var samCover = flag.String("samcover", "/usr/local/plan9/bin/sam.cover", "coverage-instrumented oracle binary, used with -uncoverc")
var samSrc = flag.String("samsrc", "/usr/local/plan9/src/cmd/sam", "C sam source directory, the -root for -uncoverc")
var uncoverBin = flag.String("uncoverbin", "uncover", "uncover binary (rsc.io/uncover)")

// useOracle reports whether to run the external sam as an oracle. -oldsam
// controls it (default true for now; eventually false); passing -sam
// explicitly implies -oldsam=true; -uncoverc requires it.
func useOracle() bool {
	if cov != nil || *oldsam {
		return true
	}
	set := false
	flag.Visit(func(f *flag.Flag) {
		if f.Name == "sam" {
			set = true
		}
	})
	return set
}

// oracleBin is the external sam to run as the oracle: the coverage build when
// -uncoverc is set, otherwise -sam.
func oracleBin() string {
	if cov != nil {
		return *samCover
	}
	return *samBin
}

func haveOracle() bool {
	if !useOracle() {
		return false
	}
	_, err := os.Stat(oracleBin())
	return err == nil
}

// cov is non-nil when -uncoverc is set; it collects one LLVM profile per oracle
// run and, after all tests, merges them through uncover -x.
var cov *covCollector

type covCollector struct {
	dir string // temp directory holding the *.profraw files
	mu  sync.Mutex
	n   int
}

// profile returns a fresh LLVM_PROFILE_FILE pattern for one oracle run. The %p
// guards against any child the oracle spawns clobbering its parent's profile.
func (c *covCollector) profile() string {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.n++
	return filepath.Join(c.dir, fmt.Sprintf("%06d-%%p.profraw", c.n))
}

// report merges every collected profile and writes uncover's report of
// uncovered C lines to the -uncoverc file.
func (c *covCollector) report() error {
	profs, err := filepath.Glob(filepath.Join(c.dir, "*.profraw"))
	if err != nil {
		return err
	}
	if len(profs) == 0 {
		return fmt.Errorf("no coverage profiles collected; is %s instrumented? (run: mk sam.cover.install)", *samCover)
	}
	args := append([]string{"-root", *samSrc, "-x", *samCover}, profs...)
	out, err := exec.Command(*uncoverBin, args...).Output()
	if err != nil {
		if ee, ok := err.(*exec.ExitError); ok {
			return fmt.Errorf("uncover: %v\n%s", err, ee.Stderr)
		}
		return fmt.Errorf("uncover: %v", err)
	}
	return os.WriteFile(*uncovercOut, out, 0644)
}

// newOracleCmd builds a command running the oracle sam, routing one coverage
// profile per run when -uncoverc is set.
func newOracleCmd(args []string) *exec.Cmd {
	cmd := exec.Command(oracleBin(), args...)
	if cov != nil {
		cmd.Env = append(os.Environ(), "LLVM_PROFILE_FILE="+cov.profile())
	}
	return cmd
}

func TestMain(m *testing.M) {
	flag.Parse()
	if *uncovercOut != "" {
		dir, err := os.MkdirTemp("", "sam-cover-")
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
		cov = &covCollector{dir: dir}
	}
	code := m.Run()
	if cov != nil {
		if err := cov.report(); err != nil {
			fmt.Fprintf(os.Stderr, "uncover: %v\n", err)
			if code == 0 {
				code = 1
			}
		}
		os.RemoveAll(cov.dir)
	}
	os.Exit(code)
}

func TestStream(t *testing.T) {
	files, err := filepath.Glob("testdata/d/*.txtar")
	if err != nil {
		t.Fatal(err)
	}
	if len(files) == 0 {
		t.Skip("no testdata/d/*.txtar cases")
	}
	for _, file := range files {
		t.Run(strings.TrimSuffix(filepath.Base(file), ".txtar"), func(t *testing.T) {
			runStreamCase(t, file)
		})
	}
}

// parseArgsLine extracts the "args:" line from an archive comment.
func parseArgsLine(comment []byte) []string {
	for _, line := range strings.Split(string(comment), "\n") {
		line = strings.TrimSpace(line)
		if rest, ok := strings.CutPrefix(line, "args:"); ok {
			return strings.Fields(rest)
		}
	}
	return nil
}

func runStreamCase(t *testing.T, file string) {
	data, err := os.ReadFile(file)
	if err != nil {
		t.Fatal(err)
	}
	ar := txtar.Parse(data)
	args := parseArgsLine(ar.Comment)

	// Classify archive files.
	var stdin []byte
	wantOut, haveOut := []byte(nil), false
	wantErr, haveErr := []byte(nil), false
	inputs := map[string][]byte{} // files to write into temp dir
	outs := map[string][]byte{}   // name.out -> expected content of name
	for _, f := range ar.Files {
		data := txtarDecode(f.Data)
		switch f.Name {
		case "stdin":
			stdin = data
		case "stdout":
			wantOut, haveOut = data, true
		case "stderr":
			wantErr, haveErr = data, true
		default:
			if base, ok := strings.CutSuffix(f.Name, ".out"); ok {
				outs[base] = data
			} else {
				inputs[f.Name] = data
			}
		}
	}

	setup := func(dir string) {
		for name, content := range inputs {
			if err := os.WriteFile(filepath.Join(dir, name), content, 0644); err != nil {
				t.Fatal(err)
			}
		}
	}

	if *update {
		if _, err := os.Stat(oracleBin()); err != nil {
			t.Skipf("-update needs the C sam at %s", oracleBin())
		}
		dir := t.TempDir()
		setup(dir)
		cout, cerr := runCSam(t, dir, args, stdin)
		// Rebuild the archive: refresh stdout/stderr and any *.out members.
		newFiles := []txtar.File{
			{Name: "stdin", Data: txtarEncode(stdin)},
			{Name: "stdout", Data: txtarEncode(cout)},
			{Name: "stderr", Data: txtarEncode(cerr)},
		}
		for name, content := range inputs {
			newFiles = append(newFiles, txtar.File{Name: name, Data: txtarEncode(content)})
		}
		for base := range outs {
			got, _ := os.ReadFile(filepath.Join(dir, base))
			newFiles = append(newFiles, txtar.File{Name: base + ".out", Data: txtarEncode(got)})
		}
		ar.Files = newFiles
		if err := os.WriteFile(file, txtar.Format(ar), 0644); err != nil {
			t.Fatal(err)
		}
		return
	}

	// Run the Go sam in-process in a temp dir.
	dir := t.TempDir()
	setup(dir)
	t.Chdir(dir)
	var out, errb bytes.Buffer
	Run(args, strings.NewReader(string(stdin)), &out, &errb)

	// Exact comparison: the ^D convention (txtarDecode) recovers the precise
	// bytes from txtar despite its trailing-newline normalization.
	if haveOut && !bytes.Equal(out.Bytes(), wantOut) {
		t.Errorf("stdout mismatch:\n--- got:\n%s\n--- want:\n%s", out.Bytes(), wantOut)
	}
	if haveErr && !bytes.Equal(errb.Bytes(), wantErr) {
		t.Errorf("stderr mismatch:\n--- got:\n%s\n--- want:\n%s", errb.Bytes(), wantErr)
	}
	for base, want := range outs {
		got, err := os.ReadFile(filepath.Join(dir, base))
		if err != nil {
			t.Errorf("reading %s: %v", base, err)
			continue
		}
		if !bytes.Equal(got, want) {
			t.Errorf("file %s mismatch:\n--- got:\n%s\n--- want:\n%s", base, got, want)
		}
	}

	// Differential check against the C sam oracle, if enabled and present.
	if haveOracle() {
		cdir := t.TempDir()
		setup(cdir)
		cout, cerr := runCSam(t, cdir, args, stdin)
		if !bytes.Equal(out.Bytes(), cout) {
			t.Errorf("Go vs C stdout differ:\n--- Go:\n%s\n--- C:\n%s", out.Bytes(), cout)
		}
		if !bytes.Equal(errb.Bytes(), cerr) {
			t.Errorf("Go vs C stderr differ:\n--- Go:\n%s\n--- C:\n%s", errb.Bytes(), cerr)
		}
	}
}

// txtar can only hold printable text, so two escapes recover exact bytes:
//
//   - a trailing "^D" (caret-D) before a member's final newline means that
//     newline is not part of the content (txtar always appends one); and
//   - the literal "^@" (caret-@) anywhere stands for a NUL byte.
//
// txtarDecode turns stored text back into exact bytes; txtarEncode is the
// inverse, used when regenerating goldens.
func txtarDecode(b []byte) []byte {
	if bytes.HasSuffix(b, []byte("^D\n")) {
		b = b[:len(b)-len("^D\n")]
	}
	return bytes.ReplaceAll(b, []byte("^@"), []byte{0})
}

func txtarEncode(b []byte) []byte {
	b = bytes.ReplaceAll(b, []byte{0}, []byte("^@"))
	if len(b) > 0 && b[len(b)-1] != '\n' {
		b = append(b, "^D\n"...)
	}
	return b
}

func runCSam(t *testing.T, dir string, args []string, stdin []byte) (stdout, stderr []byte) {
	cmd := newOracleCmd(args)
	cmd.Dir = dir
	cmd.Stdin = strings.NewReader(string(stdin))
	var out, errb bytes.Buffer
	cmd.Stdout = &out
	cmd.Stderr = &errb
	_ = cmd.Run() // sam exits 0 normally; ignore status
	return out.Bytes(), errb.Bytes()
}
