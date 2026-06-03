package main

import (
	"bufio"
	"flag"
	"io"
	"os"

	"rsc.io/getopt"
)

func main() {
	// untested
	status := Run(os.Args[1:], os.Stdin, os.Stdout, os.Stderr)
	if status == "0" || status == "" {
		// untested
		os.Exit(0)
	}
	// untested
	os.Exit(1)
}

// If compatMode is true, every Sam created by Run sets s.compat (Unix C-style error formatting).
// The flag stays false in the real binary; the test binary sets it via an init in a _test.go file.
var compatMode bool

// newSam constructs a Sam with fresh state, wired to the given streams.
func newSam(in io.Reader, out, err io.Writer) *Sam {
	s := &Sam{
		in:     in,
		out:    out,
		err:    err,
		quitok: true,
		compat: compatMode,
	}
	s.inbr = bufio.NewReader(in)
	s.line = make([]rune, BLOCKSIZE)
	s.termline = make([]rune, BLOCKSIZE)
	s.cmdstr = nil
	s.lastpat = nil
	s.lastregexp = nil
	s.genstr = nil
	s.rhs = nil
	s.curwd = nil
	s.shellcmd = nil
	s.reginit()
	s.mesginit()
	return s
}

// machineName returns the -r remote machine ("" for local operation).
func (s *Sam) machineName() string { return s.machine }

// termFlag is a flag.Value that accumulates samterm pass-through options into
// s.termargs in the order they appear on the command line, mirroring sam.c's
// `*ap++ = ...` in ARGBEGIN. opt is the literal option ("-a" or "-W"); when
// bool is false the flag's value is appended after it ("-W" size).
type termFlag struct {
	s    *Sam
	opt  string
	bool bool
}

func (t *termFlag) String() string   { return "" }
func (t *termFlag) IsBoolFlag() bool { return t.bool }
func (t *termFlag) Set(v string) error {
	t.s.termargs = append(t.s.termargs, t.opt)
	if !t.bool {
		t.s.termargs = append(t.s.termargs, v)
	}
	return nil
}

// parseArgs handles sam's flags (sam.c ARGBEGIN), returning the file arguments.
// It uses rsc.io/getopt so a single dash combines single-letter flags (-dR) and
// value flags take the rest of the arg or the next arg (EARGF), exactly as
// ARGBEGIN does.
func (s *Sam) parseArgs(args []string) []string {
	fs := getopt.NewFlagSet("sam", flag.ContinueOnError)
	fs.SetOutput(s.err)
	fs.BoolVar(&s.dflag, "d", false, "download nothing (no samterm)")
	fs.BoolVar(&s.Rflag, "R", false, "read-only")
	fs.StringVar(&s.termprog, "t", s.termprog, "samterm program")
	fs.StringVar(&s.machine, "r", "", "remote machine")
	fs.StringVar(&s.rsamname, "s", "sam", "remote sam name")
	fs.Var(&termFlag{s: s, opt: "-a", bool: true}, "a", "samterm: enable autoindent")
	fs.Var(&termFlag{s: s, opt: "-W"}, "W", "samterm: window size")
	if err := fs.Parse(args); err != nil {
		s.exits("usage")
	}
	return fs.Args()
}

// Run executes sam over the given args and streams, returning the exit status
// string ("0" on success).
func Run(args []string, in io.Reader, out, err io.Writer) (status string) {
	s := newSam(in, out, err)
	return s.run(args)
}

func (s *Sam) run(args []string) (status string) {
	defer func() {
		if r := recover(); r != nil {
			if e, ok := r.(samExit); ok {
				status = e.status
				return
			}
			// untested
			panic(r)
		}
	}()

	files := s.parseArgs(args)
	if !s.dflag {
		s.startup(s.machineName(), s.Rflag, nil, files)
	}
	s.getcurwd()

	for _, arg := range files {
		s.protect(func() {
			s.genstr = []rune(arg)
			s.genstr = s.fixname(s.genstr)
			s.logsetname(s.newfile(), s.genstr)
		})
	}
	if len(files) == 0 && !s.downloaded {
		s.newfile()
	}
	s.seq++
	if len(s.file) > 0 {
		s.current(s.file[0])
	}

	for {
		if s.protect(func() {
			s.cmdloop()
			s.trytoquit() // if we already q'ed, quitok will be TRUE
		}) {
			break
		}
	}
	return "0"
}

// protect runs body at the command-loop boundary (C's setjmp(mainloop)). An
// error deep in body calls s.error -> s.hiccough, which rolls the command back
// in place and then panics mainloopJump to unwind here -- C's
// longjmp(mainloop, 1) returning nonzero into setjmp. protect catches it and
// reports completed=false so the caller re-runs; it returns true if body
// completed normally. samExit and other panics propagate to run's recover.
func (s *Sam) protect(body func()) (completed bool) {
	defer func() {
		switch r := recover().(type) {
		case nil:
			// body completed normally
		case mainloopJump:
			completed = false
		default:
			panic(r)
		}
	}()
	body()
	return true
}
