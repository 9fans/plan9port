package main

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"slices"
)

const BLOCKSIZE = 2048

// Sam holds all of sam's state. In C these are global variables; gathering
// them here lets the Go sam run in-process so each test gets a fresh Sam.
type Sam struct {
	// I/O. These replace fds 0/1/2 (and the protocol connection for -R).
	in  io.Reader
	out io.Writer
	err io.Writer

	// flags / mode (sam.c)
	downloaded bool
	dflag      bool
	Rflag      bool
	quitok     bool

	// samterm launch (io.c bootterm). termprog is the samterm binary (-t),
	// default "samterm"; termargs are options passed through to it (-a, -W size).
	// Set during argument parsing, used by startup.
	termprog string
	termargs []string

	// remote operation (io.c connectto). machine (-r) is the host to ssh to;
	// rsamname (-s, default "sam") is the sam command run there. remotefd0/1 are
	// the ssh pipes connectto leaves for bootterm to hand to samterm.
	machine   string
	rsamname  string
	remotefd0 *os.File
	remotefd1 *os.File
	// compat makes error text match the C sam byte-for-byte (capitalized %r
	// strings). Off in ordinary use (Go's lowercase errors read better and
	// match Plan 9); the tests turn it on to compare against the C oracle.
	compat     bool
	panicking  int
	rescuing   int
	bpipeok    bool
	termlocked int

	// core editor state (sam.c). The C global fd `io` becomes ioR/ioW/ioC.
	genbuf   [BLOCKSIZE]rune
	ioR      io.Reader
	ioW      io.Writer
	ioC      io.Closer
	inbr     *bufio.Reader // buffered command input (non-downloaded)
	genstr   []rune
	rhs      []rune
	curwd    []rune
	cmdstr   []rune
	genc     string
	curfile  *File
	flist    *File
	cmd      *File
	lastfile *File
	seq      int

	// command list / input buffer (cmd.c)
	line     []rune // pending input line (runes), consumed via linep
	linep    int
	eof      bool
	termline []rune
	terminp  int
	termoutp int

	// addressing (address.c)
	addr    Address
	lastpat []rune
	patset  bool
	menu    *File

	// regexp machine (regexp.c)
	sel         Rangeset
	lastregexp  []rune
	program     []Inst
	progp       int
	startinst   *Inst // first inst. of program; might not be program[0]
	bstartinst  *Inst // same for backwards machine
	relist      [2][]Ilist
	sempty      Rangeset
	andstack    []Node
	andp        int
	atorstack   []int
	atorp       int
	subidstack  []int
	subidp      int
	lastwasand  bool // last token was operand
	cursubid    int
	backwards   bool
	nbra        int
	exprp       []rune // source expression
	exprpos     int    // index of next character in exprp
	class       [][]rune
	negateclass bool

	// undo merge state (file.c)
	merge Merge

	// file menu (multi.c)
	file     []*File
	tempfile []*File
	tag      int

	// executor (xec.c)
	glooping int
	nest     int

	// shell (shell.c)
	errfile  string
	shellcmd []rune
	plan9buf Buffer

	// wire protocol (mesg.c)
	indata      []byte
	outdata     []byte
	inp         int
	outp        int
	outmsg      int
	cmdpt       Posn
	cmdptadv    Posn
	snarfbuf    Buffer
	waitack     bool
	outbuffered bool
	tversion    int
	// rcvchar's 64-byte read buffer (mesg.c rcvchar static locals)
	rcvbuf [64]byte
	rcvn   int
	rcvi   int

	// rasp (rasp.c)
	growpos   Posn
	grown     Posn
	shrinkpos Posn
	shrunk    Posn

	// sys.c
	inerror bool
}

// samExit models C exits(): unwinds to the top-level Run.
type samExit struct{ status string }

func (s *Sam) exits(status string) {
	panic(samExit{status})
}

// rescue writes every unsaved, dirty, non-empty file to $HOME/sam.save as a
// shell script that pipes each file's contents back through $PLAN9/bin/samsave,
// then marks the files unusable (sam.c rescue()).
func (s *Sam) rescue() {
	if s.rescuing != 0 {
		// untested
		return
	}
	s.rescuing++
	for _, f := range s.file {
		f.rescuing = true
	}
	// C sets io = -1 here, abandoning any in-progress I/O.
	s.ioR, s.ioW, s.ioC = nil, nil, nil
	var save *os.File
	nblank := 0
	for _, f := range s.file {
		if f == s.cmd || f.b.nc() == 0 || !fileisdirty(f) {
			continue
		}
		if save == nil {
			fd, err := os.OpenFile(s.home()+"/sam.save", os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0777)
			if err != nil {
				// untested
				return
			}
			save = fd
			s.ioW = save
		}
		var name string
		if len(f.name) != 0 {
			name = string(f.name)
			if len(name) > 255 {
				// untested
				name = name[:255]
			}
		} else {
			name = fmt.Sprintf("nameless.%d", nblank)
			nblank++
		}
		root := os.Getenv("PLAN9")
		if root == "" {
			// untested
			root = "/usr/local/plan9"
		}
		fmt.Fprintf(save, "#!/bin/sh\n%s/bin/samsave '%s' $* <<'---%s'\n", root, name, name)
		s.addr.r.p1 = 0
		s.addr.r.p2 = f.b.nc()
		s.writeio(f)
		fmt.Fprintf(save, "\n---%s\n", name)
	}
	if save != nil {
		save.Close()
		s.ioW = nil
	}
}

// panic is a fatal internal error: it prints the message, rescues unsaved work,
// and exits. It is mainly for "can't happen" situations but is also reachable
// via malformed protocol messages. Mirrors sam.c panic():
//
//	if(!panicking++ && !setjmp(mainloop)){
//		wasd = downloaded; downloaded = 0;
//		dprint("sam: panic: %s: %r\n", s);
//		if(wasd) fprint(2, "sam: panic: %s: %r\n", s);
//		rescue();
//		abort();
//	}
//
// We drop C's ": %r" (Go has no global errstr) and abort() becomes exits("panic").
func (s *Sam) panic(msg string) {
	if s.panicking == 0 {
		s.panicking++
		func() {
			// C's setjmp(mainloop): the dprint below goes through Write, which
			// can fail and panic (syserror -> error_s -> hiccough). C catches
			// that longjmp here so the abort() at the end of the block is
			// skipped and panic returns instead of resuming the command loop;
			// we recover the equivalent mainloopJump for the same reason. The
			// samExit from exits (and real panics) still propagate.
			defer func() {
				if r := recover(); r != nil {
					if _, ok := r.(mainloopJump); !ok {
						panic(r)
					}
				}
			}()
			wasd := s.downloaded
			s.downloaded = false
			s.dprint("sam: panic: %s\n", msg)
			if wasd {
				// dprint's command-window target is unusable mid-panic, so C
				// also writes the message straight to stderr (fprint(2, ...)).
				fmt.Fprintf(s.err, "sam: panic: %s\n", msg)
			}
			s.rescue()
			s.exits("abort")
		}()
	}
	// A re-entrant panic (panicking != 0), or one whose body was interrupted by
	// the recovered mainloopJump, returns to its caller -- as in C, where the
	// if is false and panic() falls off the end.
}

// hiccough is error.c's hiccough(): bail out if already rescuing, print the
// message (unless quiet), reset parser/executor/sys state, close any open io,
// back out logged changes, resync, and finally unwind to the command loop.
// error*/whichfile call it directly, deep in the call stack, so the rollback
// happens at the point of the error as in C, and the closing panic(mainloopJump)
// is C's longjmp(mainloop, 1). msg=="" is C's hiccough((char*)0) (whichfile's
// quiet case): roll back without printing.
func (s *Sam) hiccough(msg string) {
	if s.rescuing != 0 {
		// untested
		s.exits("rescue")
	}
	if msg != "" {
		s.dprint("%s\n", msg)
	}
	s.resetcmd()
	s.resetxec()
	s.resetsys()
	if s.ioC != nil {
		// untested
		s.ioC.Close()
		s.ioR = nil
		s.ioW = nil
		s.ioC = nil
	}
	// back out any logged changes & restore old sequences
	for _, f := range s.file {
		if f == s.cmd {
			continue
		}
		if f.seq == s.seq {
			f.epsilon = f.epsilon[:0]
			f.seq = f.prevseq
			f.dot.r = f.prevdot
			f.mark = f.prevmark
			if f.prevmod {
				s.state(f, Dirty)
			} else {
				s.state(f, Clean)
			}
		}
	}
	s.update()
	if s.curfile != nil {
		if s.curfile.unread {
			// untested: rescue/dead/menu edge not reached by the differential cases
			s.curfile.unread = false
		} else if s.downloaded {
			s.outTs(Hcurrent, s.curfile.tag)
		}
	}
	panic(mainloopJump{})
}

func (s *Sam) trytoclose(f *File) {
	if f == s.cmd { // possible?
		// untested: rescue/dead/menu edge not reached by the differential cases
		return
	}
	if f.deleted {
		// untested: rescue/dead/menu edge not reached by the differential cases
		return
	}
	if fileisdirty(f) && !f.closeok {
		f.closeok = true
		var buf string
		if len(f.name) != 0 {
			buf = string(f.name)
		} else {
			buf = "nameless file"
		}
		s.error_s(Emodified, buf)
	}
	f.deleted = true
}

func (s *Sam) trytoquit() {
	if !s.quitok {
		for _, f := range s.file {
			if f != s.cmd && fileisdirty(f) {
				s.quitok = true
				s.eof = false
				s.error(Echanges)
			}
		}
	}
}

func (s *Sam) load(f *File) {
	s.genstr = slices.Clone(f.name)
	s.filename(f)
	if len(f.name) != 0 {
		saveaddr := s.addr
		s.edit(f, 'I')
		s.addr = saveaddr
	} else {
		f.unread = false
		f.cleanseq = f.seq
	}
	s.fileupdate(f, true, true)
}

func (s *Sam) cmdupdate() {
	if s.cmd != nil && s.cmd.seq != 0 {
		s.fileupdate(s.cmd, false, s.downloaded)
		s.cmd.dot.r.p1 = s.cmd.b.nc()
		s.cmd.dot.r.p2 = s.cmd.b.nc()
		s.telldot(s.cmd)
	}
}

func (s *Sam) delete(f *File) {
	if s.downloaded && f.rasp.init {
		s.outTs(Hclose, f.tag)
	}
	s.delfile(f)
	if f == s.curfile {
		s.current(nil)
	}
}

func (s *Sam) update() {
	s.settempfile()
	anymod := false
	for _, f := range s.tempfile {
		if f == s.cmd { // cmd gets done in main()
			continue
		}
		if f.deleted {
			s.delete(f)
			continue
		}
		if f.seq == s.seq && s.fileupdate(f, false, s.downloaded) {
			anymod = true
		}
		if f.rasp.init {
			s.telldot(f)
		}
	}
	if anymod {
		s.seq++
	}
}

func (s *Sam) current(f *File) *File {
	s.curfile = f
	return f
}

func (s *Sam) edit(f *File, cmd rune) {
	empty := true
	if cmd == 'r' {
		s.logdelete(f, s.addr.r.p1, s.addr.r.p2)
	}
	if cmd == 'e' || cmd == 'I' {
		s.logdelete(f, 0, f.b.nc())
		s.addr.r.p2 = f.b.nc()
	} else if f.b.nc() != 0 || (len(f.name) != 0 && runesCompare(s.genstr, f.name) != 0) {
		empty = false
	}
	fd, err := os.Open(s.genc)
	if err != nil {
		if s.curfile != nil && s.curfile.unread {
			s.curfile.unread = false
		}
		s.error_r(Eopen, s.genc, s.oserr(err))
	}
	s.ioR = fd
	s.ioC = fd
	var nulls bool
	p := s.readio(f, &nulls, empty, true)
	if cmd == 'e' || cmd == 'I' {
		s.closeio(-1)
	} else {
		s.closeio(p)
	}
	if cmd == 'r' {
		f.ndot.r.p1 = s.addr.r.p2
		f.ndot.r.p2 = s.addr.r.p2 + p
	} else {
		f.ndot.r.p1 = 0
		f.ndot.r.p2 = 0
	}
	f.closeok = empty
	if s.quitok {
		s.quitok = empty
	} else {
		s.quitok = false
	}
	if empty && !nulls {
		s.state(f, Clean)
	} else {
		s.state(f, Dirty)
	}
	if empty && !nulls {
		f.cleanseq = f.seq
	}
	if cmd == 'e' {
		s.filename(f)
	}
}

func (s *Sam) getname(f *File, t []rune, save bool) Posn {
	s.genstr = nil
	s.genc = ""
	if len(t) == 0 { // no name provided
		if f != nil {
			s.genstr = slices.Clone(f.name)
		}
	} else {
		if t[0] != ' ' && t[0] != '\t' {
			s.error(Eblank)
		}
		i := 0
		for i < len(t) && (t[i] == ' ' || t[i] == '\t') {
			i++
		}
		for i < len(t) && t[i] > ' ' {
			s.genstr = s.appendString(s.genstr, t[i])
			i++
		}
		if i < len(t) {
			// untested
			s.error(Enewline)
		}
		s.genstr = s.fixname(s.genstr)
		if f != nil && (save || len(f.name) == 0) {
			s.logsetname(f, s.genstr)
			if runesCompare(f.name, s.genstr) != 0 {
				s.quitok = false
				f.closeok = false
				f.qidpath = 0
				f.mtime = 0
				s.state(f, Dirty) // if it's 'e', fix later
			}
		}
	}
	s.genc = string(s.genstr)
	return Posn(len(s.genstr)) // strlen(name)
}

func (s *Sam) filename(f *File) {
	s.genc = string(s.genstr)
	mc := byte(' ')
	if f.mod {
		mc = '\''
	}
	rc := byte('-')
	if f.rasp.init {
		rc = '+'
	}
	cc := byte(' ')
	if f == s.curfile {
		cc = '.'
	}
	s.dprint("%c%c%c %s\n", mc, rc, cc, s.genc)
}

func (s *Sam) undostep(f *File, isundo bool) {
	mod := f.mod
	s.fileundo(f, isundo, true, true)
	f.ndot = f.dot
	if f.mod {
		f.closeok = false
		s.quitok = false
	} else {
		f.closeok = true
	}
	if f.mod != mod {
		f.mod = mod
		var cd int
		if mod {
			cd = Clean
		} else {
			cd = Dirty
		}
		s.state(f, cd)
	}
}

func (s *Sam) undo(isundo bool) bool {
	max := s.undoseq(s.curfile, isundo)
	if max == 0 {
		return false
	}
	s.settempfile()
	for _, f := range s.tempfile {
		if f != s.cmd && s.undoseq(f, isundo) == max {
			s.undostep(f, isundo)
		}
	}
	return true
}

func (s *Sam) readcmd(t []rune) int {
	if s.flist != nil {
		s.fileclose(s.flist)
	}
	s.flist = s.fileopen()
	s.addr.r.p1 = 0
	s.addr.r.p2 = s.flist.b.nc()
	retcode := s.shell(s.flist, '<', t, 0)
	s.fileupdate(s.flist, false, false)
	s.flist.seq = 0
	if s.flist.b.nc() > BLOCKSIZE {
		s.error(Etoolong)
	}
	s.genstr = nil
	s.extendString(s.flist.b.nc())
	s.genstr = append(s.genstr[:0], s.flist.b.c...)
	return retcode
}

func (s *Sam) getcurwd() {
	buf, _ := os.Getwd()
	s.curwd = []rune(buf)
	if len(s.curwd) == 0 {
		// untested
		s.warn(Wpwd)
	} else if s.curwd[Posn(len(s.curwd))-1] != '/' {
		s.curwd = s.appendString(s.curwd, '/')
	}
}

func (s *Sam) cd(str []rune) {
	s.getcurwd()
	var dir string
	if s.getname(nil, str, false) != 0 {
		dir = s.genc
	} else {
		dir = s.home()
	}
	if err := os.Chdir(dir); err != nil {
		s.syserror("chdir", err)
	}
	s.dprint("!\n")
	var owd []rune
	owd = nil
	owd = slices.Clone(s.curwd)
	s.getcurwd()
	s.settempfile()
	// Two passes so that if we have open /a/foo.c and /b/foo.c and cd from /b
	// to /a, we don't ever have two foo.c simultaneously.
	for _, f := range s.tempfile {
		if f != s.cmd && len(f.name) != 0 && f.name[0] != '/' {
			f.name = s.insertString(f.name, owd, 0)
			f.name = s.fixname(f.name)
			s.sortname(f)
		}
	}
	for _, f := range s.tempfile {
		if f != s.cmd && runesHasPrefix(f.name, s.curwd) {
			// untested: rescue/dead/menu edge not reached by the differential cases
			f.name = s.fixname(f.name)
			s.sortname(f)
		}
	}
	owd = nil
}

func (s *Sam) loadflist(str []rune) rune {
	var c rune
	if len(str) > 0 {
		c = str[0]
	}
	i := 0
	for i < len(str) && (str[i] == ' ' || str[i] == '\t') {
		i++
	}
	if (c == ' ' || c == '\t') && (i >= len(str) || str[i] != '\n') {
		if i < len(str) && str[i] == '<' {
			s.readcmd(str[i+1:])
		} else {
			s.genstr = nil
			for i < len(str) {
				c = str[i]
				i++
				if c == '\n' {
					break
				}
				s.genstr = s.appendString(s.genstr, c)
			}
		}
	} else {
		if c != '\n' {
			s.error(Eblank)
		}
		// untested: rescue/dead/menu edge not reached by the differential cases
		s.genstr = slices.Clone(empty)
	}
	s.genc = string(s.genstr)
	if len(s.genstr) == 0 {
		return 0
	}
	return s.genstr[0]
}

func (s *Sam) readflist(readall, delete bool) *File {
	var f *File
	var t []rune
	t = nil

	i := Posn(0)
	nameend := Posn(-1)
	for f == nil || readall || delete {
		s.genstr = slices.Delete(s.genstr, 0, int(i))
		for i = 0; i < Posn(len(s.genstr)); i++ {
			c := s.genstr[i]
			if c != ' ' && c != '\t' && c != '\n' {
				break
			}
		}
		if i >= Posn(len(s.genstr)) {
			break
		}
		s.genstr = slices.Delete(s.genstr, 0, int(i))
		for i = 0; i < Posn(len(s.genstr)); i++ {
			c := s.genstr[i]
			if c == ' ' || c == '\t' || c == '\n' {
				break
			}
		}
		if i == 0 {
			// unreachable: the preceding blank skip+delete leaves a non-blank
			// first char, so the name scan always advances past 0
			break
		}
		nameend = i
		t = slices.Clone(s.genstr[:i])
		t = s.fixname(t)
		f = s.lookfile(t)
		if delete {
			if f == nil {
				s.warn_S(Wfile, t)
			} else {
				s.trytoclose(f)
			}
		} else if f == nil && readall {
			f = s.newfile()
			s.logsetname(f, t)
		}
		if i < Posn(len(s.genstr)) {
			i++ // skip the separating blank
		}
	}
	// The C code wrote a NUL over the matched name's separating blank, so a
	// later read of genstr (e.g. via filename) yielded just that name. Without
	// NULs, truncate genstr to the matched name in the single-file case.
	if !readall && !delete && nameend >= 0 {
		s.genstr = s.genstr[:nameend]
	}
	t = nil
	return f
}

func (s *Sam) tofile(str []rune) *File {
	if len(str) == 0 || str[0] != ' ' {
		s.error(Eblank)
	}
	var f *File
	if s.loadflist(str) == 0 {
		f = s.lookfile(s.genstr) // empty string ==> nameless file
	} else {
		f = s.readflist(false, false)
	}
	if f == nil {
		s.error_s(Emenu, s.genc)
	}
	return s.current(f)
}

func (s *Sam) getfile(str []rune) *File {
	var f *File
	if s.loadflist(str) == 0 {
		f = s.newfile()
		s.logsetname(f, s.genstr)
	} else if f = s.readflist(true, false); f == nil {
		// untested: rescue/dead/menu edge not reached by the differential cases
		s.error(Eblank)
	}
	return s.current(f)
}

func (s *Sam) closefiles(f *File, str []rune) {
	if len(str) == 0 {
		if f == nil {
			s.error(Enofile)
		}
		s.trytoclose(f)
		return
	}
	if str[0] != ' ' {
		s.error(Eblank)
	}
	if s.loadflist(str) == 0 {
		s.error(Enewline)
	}
	s.readflist(false, true)
}

func (s *Sam) copy(f *File, addr2 Address) {
	for p := s.addr.r.p1; p < s.addr.r.p2; {
		ni := min(s.addr.r.p2-p, BLOCKSIZE)
		s.bufread(&f.b, p, s.genbuf[:ni])
		s.loginsert(addr2.f, addr2.r.p2, s.genbuf[:ni])
		p += ni
	}
	addr2.f.ndot.r.p2 = addr2.r.p2 + (f.dot.r.p2 - f.dot.r.p1)
	addr2.f.ndot.r.p1 = addr2.r.p2
}

func (s *Sam) move(f *File, addr2 Address) {
	if s.addr.r.p2 <= addr2.r.p2 {
		s.logdelete(f, s.addr.r.p1, s.addr.r.p2)
		s.copy(f, addr2)
	} else if s.addr.r.p1 >= addr2.r.p2 {
		s.copy(f, addr2)
		s.logdelete(f, s.addr.r.p1, s.addr.r.p2)
	} else {
		s.error(Eoverlap)
	}
}

func nlcount(f *File, p0, p1 Posn) Posn {
	nl := Posn(0)
	for p0 < p1 {
		if filereadc(f, p0) == '\n' {
			nl++
		}
		p0++
	}
	return nl
}

func (s *Sam) printposn(f *File, charsonly bool) {
	if !charsonly {
		l1 := 1 + nlcount(f, 0, s.addr.r.p1)
		l2 := l1 + nlcount(f, s.addr.r.p1, s.addr.r.p2)
		// check if addr ends with '\n'
		if s.addr.r.p2 > 0 && s.addr.r.p2 > s.addr.r.p1 && filereadc(f, s.addr.r.p2-1) == '\n' {
			l2--
		}
		s.dprint("%d", l1)
		if l2 != l1 {
			s.dprint(",%d", l2)
		}
		s.dprint("; ")
	}
	s.dprint("#%d", s.addr.r.p1)
	if s.addr.r.p2 != s.addr.r.p1 {
		s.dprint(",#%d", s.addr.r.p2)
	}
	s.dprint("\n")
}

func (s *Sam) settempfile() {
	s.tempfile = append(s.tempfile[:0], s.file...)
}

func (s *Sam) home() string {
	h := os.Getenv("HOME")
	if h == "" {
		// untested
		h = "/"
	}
	return h
}
