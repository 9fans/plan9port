package main

import (
	"bufio"
	"io"
	"os"
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

// compatMode, when set, makes every Sam created by Run use s.compat
// (Unix C-style error formatting).
// It stays false in the real binary; the test binary sets it via an init in a _test.go file.
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
	s.line = make([]Rune, BLOCKSIZE)
	s.termline = make([]Rune, BLOCKSIZE)
	Strinit(&s.cmdstr)
	Strinit0(&s.lastpat)
	Strinit0(&s.lastregexp)
	Strinit0(&s.genstr)
	Strinit0(&s.rhs)
	Strinit0(&s.curwd)
	Strinit0(&s.plan9cmd)
	s.reginit()
	s.meginit()
	return s
}

// machineName returns "" (remote operation is out of scope).
func (s *Sam) machineName() string { return "" }

//FIX: use rsc.io/getopt instead of custom flag parsing
// parseArgs handles sam's flags (ARGBEGIN), returning the file arguments.
func (s *Sam) parseArgs(args []string) []string {
	i := 0
	for i < len(args) {
		a := args[i]
		if len(a) < 2 || a[0] != '-' {
			break
		}
		if a == "--" {
			i++
			break
		}
		j := 1
		for j < len(a) {
			c := a[j]
			switch c {
			case 'd':
				s.dflag = true
			case 'R':
				s.Rflag = true
			case 'a':
				// samterm option: pass through.
				s.termargs = append(s.termargs, "-a")
			case 'r', 't', 's', 'W':
				// these take an argument (EARGF): the rest of this arg, else
				// the next arg.
				var val string
				if j+1 < len(a) {
					val = a[j+1:]
					j = len(a)
				} else {
					i++
					if i < len(args) {
						val = args[i]
					}
				}
				switch c {
				case 't':
					s.termprog = val
				case 'W':
					s.termargs = append(s.termargs, "-W", val)
				case 'r', 's':
					// remote operation is out of scope; ignore.
				}
			default:
				s.dprint("sam: unknown flag %c\n", c)
				s.exits("usage")
			}
			j++
		}
		i++
	}
	return args[i:]
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
			t := tmpcstr(arg)
			Straddc(t, 0)
			Strduplstr(&s.genstr, t)
			freetmpstr(t)
			s.fixname(&s.genstr)
			s.logsetname(s.newfile(), &s.genstr)
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
		if s.mainOnce(func() {
			s.cmdloop()
			s.trytoquit() // if we already q'ed, quitok will be TRUE
		}) {
			break
		}
	}
	return "0"
}

// protect runs body, turning a samErr panic into a hiccough (mirroring a
// per-file setjmp(mainloop)). samExit propagates to run's recover.
func (s *Sam) protect(body func()) {
	defer func() {
		if r := recover(); r != nil {
			// untested
			if e, ok := r.(samErr); ok {
				// untested: a non-samErr panic reaching the mainloop boundary
				s.dohiccough(e.msg, e.quiet)
				return
			}
			// untested
			panic(r)
		}
	}()
	body()
}

// mainOnce runs body at the mainloop boundary. It returns true if body
// completed normally, false if a samErr was handled (so the caller re-runs).
func (s *Sam) mainOnce(body func()) (completed bool) {
	defer func() {
		if r := recover(); r != nil {
			if e, ok := r.(samErr); ok {
				s.dohiccough(e.msg, e.quiet)
				completed = false
				return
			}
			panic(r)
		}
	}()
	body()
	return true
}
