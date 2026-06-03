package main

import (
	"bytes"
	"io"
	"os"
	"os/exec"
)

// External commands (shell.c). plan9 runs the shell command stored in
// plan9cmd, optionally piping the addressed text in (> and |) and/or replacing
// it with the command's output (< and |).

const (
	SH     = "sh"
	SHPATH = "/bin/sh"
)

func (s *Sam) setname(c *exec.Cmd, f *File) {
	var name string
	if f != nil {
		name = Strtoc(&f.name) // stops at the NUL, like C putenv
	}
	c.Env = append(os.Environ(), "samfile="+name, "%="+name)
}

func (s *Sam) plan9(f *File, typ int, str *String, nest int) int {
	if str.s[0] == 0 && s.plan9cmd.s[0] == 0 {
		fail(Enocmd)
	} else if str.s[0] != 0 {
		Strduplstr(&s.plan9cmd, str)
	}

	cmdline := Strtoc(&s.plan9cmd)
	c := &exec.Cmd{Path: SHPATH, Args: []string{SH, "-c", cmdline}}
	s.setname(c, f)

	if typ == '|' {
		s.snarf(f, s.addr.r.p1, s.addr.r.p2, &s.plan9buf, true)
	}

	var retcode int
	switch typ {
	case '!':
		c.Stdout = s.out
		c.Stderr = s.err
		_ = c.Run()
		retcode = exitcode(c)

	case '>':
		c.Stdout = s.out
		c.Stderr = s.err
		stdin, _ := c.StdinPipe()
		if err := c.Start(); err != nil {
			// untested: fork/exec of /bin/sh does not fail in tests
			fail(Efork)
		}
		s.ioW = stdin
		s.ioC = nil
		s.bpipeok = true
		s.writeio(f)
		s.bpipeok = false
		stdin.Close()
		s.ioW = nil
		_ = c.Wait()
		retcode = exitcode(c)

	case '<', '|':
		// The child's stderr must not share s.err with the readio below: exec
		// copies it on a goroutine, which would race with our own writes to
		// s.err (e.g. readio's "null characters elided" warning) and corrupt the
		// buffer. Capture it separately and append once the child and that copy
		// goroutine have finished (after Wait).
		var cstderr bytes.Buffer
		c.Stderr = &cstderr
		stdout, _ := c.StdoutPipe()
		if typ == '|' {
			stdin, _ := c.StdinPipe()
			// Snapshot the piped text now, on this goroutine: C forks a separate
			// process to feed the pipe, but a Go goroutine would share s.genbuf
			// and the snarf buffers with the readio below. Collecting the bytes
			// here (sequential with the rest) leaves the writer goroutine
			// touching no Sam state.
			var piped bytes.Buffer
			s.pipeplan9buf(&piped)
			data := piped.Bytes()
			go func() {
				stdin.Write(data)
				stdin.Close()
			}()
		}
		if err := c.Start(); err != nil {
			// untested: fork/exec of /bin/sh does not fail in tests
			fail(Efork)
		}
		var nulls bool
		s.snarf(f, s.addr.r.p1, s.addr.r.p2, &s.snarfbuf, false)
		s.logdelete(f, s.addr.r.p1, s.addr.r.p2)
		s.ioR = stdout
		s.ioC = nil
		f.tdot.p1 = -1
		f.ndot.r.p2 = s.addr.r.p2 + s.readio(f, &nulls, false, false)
		f.ndot.r.p1 = s.addr.r.p2
		s.ioR = nil
		_ = c.Wait()
		s.err.Write(cstderr.Bytes())
		retcode = exitcode(c)
	}

	if typ == '|' || typ == '<' {
		if retcode != 0 {
			s.warn(Wbadstatus)
		}
	}
	if nest == 0 {
		s.dprint("!\n")
	}
	return retcode
}

func (s *Sam) pipeplan9buf(w io.Writer) {
	for l := Posn(0); l < Posn(s.plan9buf.nc()); {
		m := s.plan9buf.nc() - int(l)
		if m > BLOCKSIZE-1 {
			m = BLOCKSIZE - 1
		}
		bufread(&s.plan9buf, l, s.genbuf[:], m)
		c := Strtoc(tmprstr(s.genbuf[:], m))
		io.WriteString(w, c)
		l += Posn(m)
	}
}

func exitcode(c *exec.Cmd) int {
	if c.ProcessState == nil {
		// untested: ProcessState is never nil after Run/Wait here
		return 0
	}
	if c.ProcessState.Success() {
		return 0
	}
	return c.ProcessState.ExitCode()
}
