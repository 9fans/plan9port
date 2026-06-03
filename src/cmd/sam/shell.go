package main

import (
	"bytes"
	"io"
	"os"
	"os/exec"
	"os/user"
	"path/filepath"
	"slices"
)

const (
	SH     = "sh"
	SHPATH = "/bin/sh"
)

func (s *Sam) setname(c *exec.Cmd, f *File) {
	var name string
	if f != nil {
		name = string(f.name)
	}
	c.Env = append(os.Environ(), "samfile="+name, "%="+name)
}

// getuser is unix.c getuser(): $USER if set, else the login name, else "nobody".
func getuser() string {
	if u := os.Getenv("USER"); u != "" {
		return u
	}
	// untested: $USER is always set in the harness, so the fallbacks are unused
	if u, err := user.Current(); err == nil && u.Username != "" {
		// untested
		return u.Username
	}
	// untested
	return "nobody"
}

// samerr is unix.c samerr(): the per-user file where a downloaded sam collects a
// shell command's diagnostic output for checkerrs to display.
func (s *Sam) samerr() string {
	return filepath.Join("/tmp", "sam."+getuser()+".err")
}

func (s *Sam) shell(f *File, typ rune, str []rune, nest int) int {
	if len(str) == 0 && len(s.shellcmd) == 0 {
		s.error(Enocmd)
	} else if len(str) != 0 {
		s.shellcmd = slices.Clone(str)
	}

	// In downloaded (GUI) mode the command's diagnostics must not reach the
	// protocol stream on s.out, so C collects the child's stderr (and, for !
	// and >, its stdout) into an error file and shows it with checkerrs.
	var errf *os.File
	errfile := ""
	if s.downloaded {
		errfile = s.samerr()
		os.Remove(errfile)
		var err error
		if errf, err = os.Create(errfile); err != nil {
			// untested: /tmp is writable in the harness
			errf, _ = os.Create(os.DevNull)
		}
	}

	if typ == '|' {
		s.snarf(f, s.addr.r.p1, s.addr.r.p2, &s.plan9buf, true)
	}

	cmdline := string(s.shellcmd)
	c := &exec.Cmd{Path: SHPATH, Args: []string{SH, "-c", cmdline}}
	s.setname(c, f)

	var retcode int
	var cstderr bytes.Buffer
	switch typ {
	case '!':
		if s.downloaded {
			c.Stdout = errf
			c.Stderr = errf
		} else {
			c.Stdout = s.out
			c.Stderr = s.err
		}
		_ = c.Run()
		retcode = exitcode(c)

	case '>':
		if s.downloaded {
			c.Stdout = errf
			c.Stderr = errf
		} else {
			c.Stdout = s.out
			c.Stderr = s.err
		}
		stdin, _ := c.StdinPipe()
		if err := c.Start(); err != nil {
			// untested: fork/exec of /bin/sh does not fail in tests
			s.error(Efork)
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
		// Downloaded mode routes stderr to the error file (a separate fd, no
		// race with the readio below). Otherwise capture it and flush to s.err
		// after Wait, so it can't interleave with readio's own s.err writes.
		if s.downloaded {
			c.Stderr = errf
		} else {
			c.Stderr = &cstderr
		}
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
			s.error(Efork)
		}
		var nulls bool
		if s.downloaded && s.addr.r.p1 != s.addr.r.p2 {
			s.outTl(Hsnarflen, s.addr.r.p2-s.addr.r.p1)
		}
		s.snarf(f, s.addr.r.p1, s.addr.r.p2, &s.snarfbuf, false)
		s.logdelete(f, s.addr.r.p1, s.addr.r.p2)
		s.ioR = stdout
		s.ioC = nil
		f.tdot.p1 = -1
		f.ndot.r.p2 = s.addr.r.p2 + s.readio(f, &nulls, false, false)
		f.ndot.r.p1 = s.addr.r.p2
		s.ioR = nil
		_ = c.Wait()
		if !s.downloaded {
			s.err.Write(cstderr.Bytes())
		}
		retcode = exitcode(c)
	}

	if errf != nil {
		errf.Close()
	}
	if typ == '|' || typ == '<' {
		if retcode != 0 {
			s.warn(Wbadstatus)
		}
	}
	if s.downloaded {
		s.checkerrs(errfile)
	}
	if nest == 0 {
		s.dprint("!\n")
	}
	return retcode
}

// checkerrs shows the error file in the command window (shell.c checkerrs): the
// first 25 lines, capped at BLOCKSIZE-11 bytes, then a "(sam: more in ...)" note
// if there is more. An empty (or missing) file is removed.
func (s *Sam) checkerrs(errfile string) {
	fi, err := os.Stat(errfile)
	if err != nil || fi.Size() == 0 {
		os.Remove(errfile)
		return
	}
	fd, err := os.Open(errfile)
	if err != nil {
		// untested: the file we just stat'd opens
		return
	}
	defer fd.Close()
	buf := make([]byte, BLOCKSIZE-10)
	n, _ := fd.Read(buf[:len(buf)-1])
	if n <= 0 {
		// untested: a non-empty file yields a read
		return
	}
	nl, p := 0, 0
	for nl < 25 && p < n {
		if buf[p] == '\n' {
			nl++
		}
		p++
	}
	s.dprint("%s", string(buf[:p]))
	if int64(p) < fi.Size()-1 {
		s.dprint("(sam: more in %s)\n", errfile)
	}
}

func (s *Sam) pipeplan9buf(w io.Writer) {
	for l := Posn(0); l < s.plan9buf.nc(); {
		m := min(s.plan9buf.nc()-l, BLOCKSIZE-1)
		s.bufread(&s.plan9buf, l, s.genbuf[:m])
		c := string(s.genbuf[:m])
		io.WriteString(w, c)
		l += m
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
