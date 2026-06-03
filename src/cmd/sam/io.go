package main

import (
	"fmt"
	"io/fs"
	"os"
	"os/exec"
	"unicode/utf8"
)

func (s *Sam) checkqid(f *File) {
	w := s.whichmenu(f)
	for i := 1; i < len(s.file); i++ {
		g := s.file[i]
		if w == i {
			continue
		}
		if f.dev == g.dev && f.qidpath == g.qidpath {
			s.warn_SS(Wdupfile, f.name, g.name)
		}
	}
}

func (s *Sam) writef(f *File) {
	newfile := false
	samename := runesCompare(s.genstr, f.name) == 0
	name := string(f.name)
	dev, qid, mtime, _, ok := statfile(name)
	if !ok {
		newfile = true
	} else if samename && (f.dev != dev || f.qidpath != qid || f.mtime < mtime) {
		f.dev = dev
		f.qidpath = qid
		f.mtime = mtime
		s.warn_S(Wdate, s.genstr)
		return
	}
	s.genc = string(s.genstr)
	fd, err := os.OpenFile(s.genc, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0666)
	if err != nil {
		s.error_r(Ecreate, s.genc, s.oserr(err))
	}
	s.ioW = fd
	s.ioC = fd
	s.dprint("%s: ", s.genc)
	if fi, statErr := fd.Stat(); statErr == nil && fi.Mode()&fs.ModeAppend != 0 {
		// untested: writing a file with the append-only attribute set
		s.error(Eappend)
	}
	n := s.writeio(f)
	if len(f.name) == 0 || samename {
		if s.addr.r.p1 == 0 && s.addr.r.p2 == f.b.nc() {
			f.cleanseq = f.seq
		}
		if f.cleanseq == f.seq {
			s.state(f, Clean)
		} else {
			s.state(f, Dirty)
		}
	}
	if newfile {
		s.dprint("(new file) ")
	}
	if s.addr.r.p2 > 0 && filereadc(f, s.addr.r.p2-1) != '\n' {
		s.warn(Wnotnewline)
	}
	s.closeio(n)
	if len(f.name) == 0 || samename {
		if dev, qid, mtime, _, ok := statfile(s.genc); ok {
			f.dev = dev
			f.qidpath = qid
			f.mtime = mtime
			s.checkqid(f)
		}
	}
}

func (s *Sam) readio(f *File, nulls *bool, setdate, toterm bool) Posn {
	nt := Posn(0)
	p := s.addr.r.p2

	*nulls = false
	if f.unread {
		nt = s.fileload(f, 0, s.ioR, nulls)
		if toterm {
			s.raspload(f)
		}
	} else {
		buf := make([]byte, BLOCKSIZE)
		b := 0
		for {
			n, _ := s.ioR.Read(buf[b:])
			if n <= 0 {
				break
			}
			n += b
			b = 0
			var runes []rune
			// Convert as many complete runes as possible, keeping a partial
			// rune for the next read.
			i := 0
			for i < n {
				if buf[i] < utf8.RuneSelf {
					if buf[i] != 0 {
						runes = append(runes, rune(buf[i]))
					} else {
						*nulls = true
					}
					i++
					continue
				}
				if !utf8.FullRune(buf[i:n]) {
					break
				}
				// Note: NUL already handled above.
				r, w := utf8.DecodeRune(buf[i:n])
				runes = append(runes, r)
				i += w
			}
			b = n - i
			copy(buf, buf[i:n])
			s.loginsert(f, p, runes)
			nt += Posn(len(runes))
		}
		if b != 0 {
			// TODO: Why does this set nulls?
			*nulls = true
		}
	}
	if *nulls {
		s.warn(Wnulls)
	}
	if setdate {
		if dev, qid, mtime, _, ok := statfd(s.ioR); ok {
			f.dev = dev
			f.qidpath = qid
			f.mtime = mtime
			s.checkqid(f)
		}
	}
	return nt
}

func (s *Sam) writeio(f *File) Posn {
	p := s.addr.r.p1
	for p < s.addr.r.p2 {
		var n Posn
		n = min(s.addr.r.p2-p, BLOCKSIZE)
		s.bufread(&f.b, p, s.genbuf[:n])
		c := []byte(string(s.genbuf[:n]))
		// Note: C checked s.Write return value,
		// but the only way it can return short is if
		// there is already a different I/O error being
		// handled, and if so things are already hosed.
		s.Write(s.ioW, c)
		p += n
	}
	return p - s.addr.r.p1
}

func (s *Sam) closeio(p Posn) {
	if s.ioC != nil {
		s.Close(s.ioC)
	}
	s.ioR = nil
	s.ioW = nil
	s.ioC = nil
	if p >= 0 {
		s.dprint("#%d\n", p)
	}
}

func (s *Sam) startup(machine string, Rflag bool, termargs, files []string) {
	if machine != "" {
		s.connectto(machine, files)
	}
	if !Rflag {
		s.bootterm(machine)
	}
	s.downloaded = true
	s.outTs(Hversion, VERSION)
}

// RX is the remote-execution command (unix.c RXNAME). A var so tests can point
// it at a harmless stand-in instead of a real ssh.
var RX = "ssh"

// stderrFile returns s.err as an *os.File for a child's Stderr, falling back to
// os.Stderr when s.err is not a file (e.g. a test buffer).
func (s *Sam) stderrFile() *os.File {
	if f, ok := s.err.(*os.File); ok {
		return f
	}
	return os.Stderr
}

// connectArgv builds the command that runs sam -R on the remote machine
// (io.c connectto): ssh <machine> <rsamname> -R <files...>.
func connectArgv(machine, rsamname string, files []string) []string {
	av := []string{RX, machine, rsamname, "-R"}
	return append(av, files...)
}

// connectto starts `ssh machine sam -R files...` and wires a pair of pipes so
// the protocol can flow to the remote sam: remotefd0 reads ssh's stdout (the
// remote sam's output) and remotefd1 writes ssh's stdin (io.c connectto).
// bootterm then hands these to samterm.
func (s *Sam) connectto(machine string, files []string) {
	p1r, p1w, err := os.Pipe() // ssh stdout -> us (remotefd0)
	if err != nil {
		// untested: os.Pipe does not fail in tests
		s.dprint("can't pipe\n")
		s.exits("pipe")
	}
	p2r, p2w, err := os.Pipe() // us (remotefd1) -> ssh stdin
	if err != nil {
		// untested: os.Pipe does not fail in tests
		s.dprint("can't pipe\n")
		s.exits("pipe")
	}
	av := connectArgv(machine, s.rsamname, files)
	cmd := exec.Command(av[0], av[1:]...)
	cmd.Stdin = p2r
	cmd.Stdout = p1w
	cmd.Stderr = s.stderrFile()
	if err := cmd.Start(); err != nil {
		s.dprint("can't exec %s\n", RX)
		s.exits("exec")
	}
	// Close our copies of ssh's ends; keep the host ends for samterm.
	p2r.Close()
	p1w.Close()
	go cmd.Wait()
	s.remotefd0 = p1r
	s.remotefd1 = p2w
}

// bootterm launches samterm. In the local case it connects samterm to the
// in-process sam by a pair of pipes (rewiring s.in/s.out). In the remote case
// (machine != "") it instead hands samterm the ssh pipes connectto set up and
// waits for it: the local process is only a launcher, so it exits when samterm
// does (io.c bootterm).
func (s *Sam) bootterm(machine string) {
	prog := s.termprog
	if prog == "" {
		prog = "samterm"
	}
	if machine != "" {
		cmd := exec.Command(prog, s.termargs...)
		cmd.Stdin = s.remotefd0
		cmd.Stdout = s.remotefd1
		cmd.Stderr = s.stderrFile()
		if err := cmd.Start(); err != nil {
			fmt.Fprintf(s.err, "can't exec %s: %v\n", prog, err)
			s.exits("damn")
		}
		s.remotefd0.Close()
		s.remotefd1.Close()
		cmd.Wait()
		s.exits("0")
	}
	h2tR, h2tW, err := os.Pipe() // sam -> samterm
	if err != nil {
		// untested: os.Pipe does not fail in tests
		s.panic("pipe")
	}
	t2hR, t2hW, err := os.Pipe() // samterm -> sam
	if err != nil {
		// untested: os.Pipe does not fail in tests
		s.panic("pipe")
	}
	cmd := exec.Command(prog, s.termargs...)
	cmd.Stdin = h2tR
	cmd.Stdout = t2hW
	cmd.Stderr = s.stderrFile()
	if err := cmd.Start(); err != nil {
		fmt.Fprintf(s.err, "can't exec %s: %v\n", prog, err)
		s.exits("damn")
	}

	// Close our copies of the child's ends; keep the host ends.
	h2tR.Close()
	t2hW.Close()
	s.in = t2hR
	s.out = h2tW

	go cmd.Wait()
}
