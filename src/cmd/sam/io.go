package main

import (
	"errors"
	"fmt"
	"io"
	"os"
	"os/exec"
	"syscall"
)

// File I/O (io.c). The C global fd `io` becomes s.ioR/s.ioW/s.ioC.

func (s *Sam) checkqid(f *File) {
	w := s.whichmenu(f)
	for i := 1; i < len(s.file); i++ {
		g := s.file[i]
		if w == i {
			continue
		}
		if f.dev == g.dev && f.qidpath == g.qidpath {
			s.warn_SS(Wdupfile, &f.name, &g.name)
		}
	}
}

func (s *Sam) writef(f *File) {
	newfile := false
	samename := Strcmp(&s.genstr, &f.name) == 0
	name := Strtoc(&f.name)
	dev, qid, mtime, _, ok := statfile(name)
	if !ok {
		newfile = true
	} else if samename && (f.dev != dev || f.qidpath != qid || f.mtime < mtime) {
		f.dev = dev
		f.qidpath = qid
		f.mtime = mtime
		s.warn_S(Wdate, &s.genstr)
		return
	}
	s.genc = Strtoc(&s.genstr)
	fd, err := os.OpenFile(s.genc, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0666)
	if err != nil {
		fail_r(Ecreate, s.genc, s.oserr(err))
	}
	s.ioW = fd
	s.ioC = fd
	s.dprint("%s: ", s.genc)
	//FIX: Lost C's append-only check here. Put it back, checking for fs.ModeAppend.
	n := s.writeio(f)
	if f.name.s[0] == 0 || samename {
		if s.addr.r.p1 == 0 && s.addr.r.p2 == Posn(f.b.nc()) {
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
	if f.name.s[0] == 0 || samename {
		if dev, qid, mtime, _, ok := statfile(s.genc); ok {
			f.dev = dev
			f.qidpath = qid
			f.mtime = mtime
			s.checkqid(f)
		}
	}
}

func (s *Sam) readio(f *File, nulls *bool, setdate, toterm bool) Posn {
	var nt Posn
	p := s.addr.r.p2

	*nulls = false
	if f.unread {
		//FIX: C says bufload, let's keep it that way for now
		nt = Posn(s.fileload(f, 0, s.ioR, nulls))
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
			var runes []Rune
			// Convert as many complete runes as possible, keeping a partial
			// rune for the next read.
			i := 0
			for i < n {
				if buf[i] < Runeself {
					if buf[i] != 0 {
						runes = append(runes, Rune(buf[i]))
					} else {
						*nulls = true
					}
					i++
					continue
				}
				if !fullRune(buf[i:n]) {
					break
				}
				// Note: NUL already handled above.
				r, w := decodeRune(buf[i:n])
				runes = append(runes, r)
				i += w
			}
			b = n - i
			copy(buf, buf[i:n])
			s.loginsert(f, p, runes, len(runes))
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
		var n int
		if s.addr.r.p2-p > BLOCKSIZE {
			n = BLOCKSIZE
		} else {
			n = int(s.addr.r.p2 - p)
		}
		bufread(&f.b, p, s.genbuf[:], n)
		c := Strtoc(tmprstr(s.genbuf[:], n))
		m := len(c)
		if nn, _ := io.WriteString(s.ioW, c); nn != m {
			// untested: a short write only happens on a broken pipe mid-write
			if p > 0 {
				// untested: a short write only happens on a broken pipe mid-write
				p += Posn(n)
			}
			// untested: a short write only happens on a broken pipe mid-write
			break
		}
		p += Posn(n)
	}
	return p - s.addr.r.p1
}

func (s *Sam) closeio(p Posn) {
	if s.ioC != nil {
		s.ioC.Close()
	}
	s.ioR = nil
	s.ioW = nil
	s.ioC = nil
	if p >= 0 {
		s.dprint("#%d\n", p)
	}
}

func (s *Sam) startup(machine string, Rflag bool, termargs, files []string) {
	if !Rflag {
		s.bootterm()
	}
	s.downloaded = true
	s.outTs(Hversion, VERSION)
}

// bootterm launches samterm connected to sam by a pair of pipes and rewires
// sam's protocol streams (s.in/s.out) to talk to it. Port of io.c bootterm's
// local (non-remote) case; remote operation (machine != "") is out of scope.
func (s *Sam) bootterm() {
	prog := s.termprog
	if prog == "" {
		prog = "samterm"
	}
	h2tR, h2tW, err := os.Pipe() // sam -> samterm
	if err != nil {
		s.panicf("pipe")
	}
	t2hR, t2hW, err := os.Pipe() // samterm -> sam
	if err != nil {
		s.panicf("pipe")
	}
	cmd := exec.Command(prog, s.termargs...)
	cmd.Stdin = h2tR
	cmd.Stdout = t2hW
	if ef, ok := s.err.(*os.File); ok {
		cmd.Stderr = ef
	} else {
		cmd.Stderr = os.Stderr
	}
	if err := cmd.Start(); err != nil {
		fmt.Fprintf(s.err, "can't exec %s: %v\n", prog, err)
		s.exits("damn")
	}

	// Close our copies of the child's ends; keep the host ends.
	h2tR.Close()
	t2hW.Close()
	s.in = t2hR
	s.out = h2tW

	//FIX: is this cmd.Wait important?
	go cmd.Wait() // reap when it exits (e.g. on EOF after sam quits)
}

//FIX: move the rest of this file into io_unix.go so that we can put different code in on Windows or Plan 9 (no need to handle those yet)

func statfile(name string) (dev uint32, qid uint64, mtime uint, length int64, ok bool) {
	fi, err := os.Stat(name)
	if err != nil {
		return 0, 0, 0, 0, false
	}
	return statFromInfo(fi)
}

func statfd(r io.Reader) (dev uint32, qid uint64, mtime uint, length int64, ok bool) {
	f, isFile := r.(*os.File)
	if !isFile {
		// unreachable: s.ioR is always an *os.File (os.Open or exec StdoutPipe)
		return 0, 0, 0, 0, false
	}
	fi, err := f.Stat()
	if err != nil {
		// unreachable: Stat of an already-open fd does not fail
		return 0, 0, 0, 0, false
	}
	return statFromInfo(fi)
}

func statFromInfo(fi os.FileInfo) (dev uint32, qid uint64, mtime uint, length int64, ok bool) {
	st, isStat := fi.Sys().(*syscall.Stat_t)
	if !isStat {
		// unreachable: on Unix os.FileInfo.Sys() is always *syscall.Stat_t
		return 0, 0, uint(fi.ModTime().Unix()), fi.Size(), true
	}
	return uint32(st.Dev), uint64(st.Ino), uint(fi.ModTime().Unix()), fi.Size(), true
}

// oserr renders err for an error message. Go's syscall.Errno strings are
// libc's strerror with a lowercased first letter; in compat mode we restore
// the capital so the output matches the C sam (Plan 9 %r) byte-for-byte, but
// in ordinary use we keep Go's nicer lowercase form.
func (s *Sam) oserr(err error) string {
	var errno syscall.Errno
	if errors.As(err, &errno) {
		m := errno.Error()
		if s.compat && len(m) > 0 && 'a' <= m[0] && m[0] <= 'z' {
			return string(m[0]-'a'+'A') + m[1:]
		}
		return m
	}
	if pe, ok := err.(*os.PathError); ok {
		return pe.Err.Error()
	}
	return err.Error()
}
