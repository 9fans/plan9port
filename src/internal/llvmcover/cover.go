// Package llvmcover provides helpers for gathering and interpreting
// coverage from C programs built with LLVM's coverage support.
package llvmcover

import (
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"sync"
	"sync/atomic"
	"testing"
)

// An Uncover tracks uncovered source lines.
type Uncover struct {
	file string
	prog string
	src  string
	skip string
	tmp  func() (string, error)
	seq  atomic.Int64
}

// New returns a new Uncover registered as the -uncoverc flag.
// Each time the C program with coverage is executed, the result of
// a new call to [Uncover.Env] should be added to its environment.
// When all the C programs have been run, call [Uncover.Finish] to
// remove the created temporary files and write the file specified by
// the -uncoverc flag, if any.
func New(prog, skip string) *Uncover {
	c := &Uncover{
		prog: prog,
		src:  filepath.Dir(prog),
		skip: skip,
		tmp: sync.OnceValues(func() (string, error) {
			return os.MkdirTemp("", "llvmcover-*")
		}),
	}
	flag.Var((*uncoverFlag)(c), "uncoverc", "write uncovered C source report to `file`")
	return c
}

// uncoverFlag is the Uncover implementation of flag.Value.
type uncoverFlag Uncover

func (c *uncoverFlag) String() string {
	return c.file
}

func (c *uncoverFlag) Set(s string) error {
	c.file = s
	return nil
}

// Enabled reports whether coverage has been enabled
// by passing the -uncoverc flag on the command line.
func (c *Uncover) Enabled() bool {
	return c.file != ""
}

// Env returns a fresh environment variable pattern for a single run.
// Each time the C program with coverage is executed, the result of
// a new call to [Uncover.Env] should be added to its environment.
func (c *Uncover) Env(t *testing.T) string {
	if c.file == "" {
		return "LLVMCOVER=0" // anything harmless
	}
	dir, err := c.tmp()
	if err != nil {
		t.Helper()
		t.Fatal(err)
	}
	return fmt.Sprintf("LLVM_PROFILE_FILE=%s%s%d-%%p.profraw", dir, string(filepath.Separator), c.seq.Add(1))
}

// Finish writes uncover output to the file specified on the command line
// using the -uncoverc flag and then removes any temporary files and
// directories that have been created during C execution.
func (c *Uncover) Finish() error {
	if c.file == "" {
		return nil
	}
	dir, err := c.tmp()
	if err != nil {
		return err
	}
	profiles, err := filepath.Glob(filepath.Join(dir, "*.profraw"))
	if err != nil {
		return err
	}
	args := []string{"-d=" + c.src, "-skip=" + c.skip, "-x=" + c.prog}
	args = append(args, profiles...)
	out, err := exec.Command("uncover", args...).Output()
	if err != nil {
		if ee, ok := err.(*exec.ExitError); ok {
			return fmt.Errorf("uncover: %v\n%s", err, ee.Stderr)
		}
		return fmt.Errorf("uncover: %v", err)
	}
	return os.WriteFile(c.file, out, 0666)
}
