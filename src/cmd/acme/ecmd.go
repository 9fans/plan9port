package main

import (
	"fmt"
	"io"
	"os"
	"os/exec"
	"strings"
	"sync"
	"syscall"
)

// Edit-command implementations (ecmd.c). This file currently holds the address
// evaluator; the command functions (a/c/d/i/s/x/g/...) are ported on top of it.

// edittext appends r to the edit output destination (ecmd.c edittext): the
// editout/editout files are writable only while an edit command's pipe is
// running, which the -D surface never starts, so editing is always Inactive.
func (a *Acme) edittext(w *Window, q int, r []rune) string {
	switch a.editing {
	case Inactive:
		return "permission denied"
	case Inserting:
		// untested: -D cannot start an edit pipe, so editing stays Inactive
		a.eloginsert(w.body.file, q, r)
		return ""
	case Collecting:
		// untested: -D cannot start an edit pipe, so editing stays Inactive
		a.collection = append(a.collection, r...)
		return ""
	default:
		// untested: editing only takes the three states above
		return "unknown state in edittext"
	}
}

// --- command implementations (ecmd.c) ---

// appendCmd inserts cp's text at p (a_cmd/i_cmd helper, C append).
func (a *Acme) appendCmd(f *File, cp *Cmd, p int) int {
	if len(cp.text) > 0 {
		a.eloginsert(f, p, cp.text)
	}
	f.curtext.q0 = p
	f.curtext.q1 = p
	return 1
}

func (a *Acme) a_cmd(t *Text, cp *Cmd) int {
	return a.appendCmd(t.file, cp, a.editAddr.r.q1)
}

func (a *Acme) i_cmd(t *Text, cp *Cmd) int {
	return a.appendCmd(t.file, cp, a.editAddr.r.q0)
}

func (a *Acme) c_cmd(t *Text, cp *Cmd) int {
	a.elogreplace(t.file, a.editAddr.r.q0, a.editAddr.r.q1, cp.text)
	t.q0 = a.editAddr.r.q0
	t.q1 = a.editAddr.r.q1
	return 1
}

func (a *Acme) d_cmd(t *Text, cp *Cmd) int {
	if a.editAddr.r.q1 > a.editAddr.r.q0 {
		a.elogdelete(t.file, a.editAddr.r.q0, a.editAddr.r.q1)
	}
	t.q0 = a.editAddr.r.q0
	t.q1 = a.editAddr.r.q0
	return 1
}

func (a *Acme) p_cmd(t *Text, cp *Cmd) int {
	return a.pdisplay(t.file)
}

// pdisplay prints the addressed text as a warning (ecmd.c).
func (a *Acme) pdisplay(f *File) int {
	p1 := a.editAddr.r.q0
	p2 := a.editAddr.r.q1
	if p2 > f.b.nc() {
		// addresses are constrained before pdisplay, so this clamp is a
		// defensive backstop not reached via -D.
		// untested
		p2 = f.b.nc()
	}
	buf := make([]rune, p2-p1)
	f.b.Read(p1, buf)
	a.warning(nil, "%s", string(buf))
	f.curtext.q0 = a.editAddr.r.q0
	f.curtext.q1 = a.editAddr.r.q1
	return 1
}

// s_cmd performs substitution over the address range (ecmd.c).
func (a *Acme) s_cmd(t *Text, cp *Cmd) int {
	n := cp.num
	op := -1
	if !a.re.rxcompile(cp.re) {
		a.editerror("bad regexp in s command")
	}
	var rp []Rangeset
	delta := 0
	didsub := false
	for p1 := a.editAddr.r.q0; p1 <= a.editAddr.r.q1 && a.re.rxexecute(t, nil, p1, a.editAddr.r.q1, &a.re.sel); {
		if a.re.sel.r[0].q0 == a.re.sel.r[0].q1 { // empty match?
			if a.re.sel.r[0].q0 == op {
				p1++
				continue
			}
			p1 = a.re.sel.r[0].q1 + 1
		} else {
			p1 = a.re.sel.r[0].q1
		}
		op = a.re.sel.r[0].q1
		n--
		if n > 0 {
			continue
		}
		rp = append(rp, a.re.sel)
	}
	for m := 0; m < len(rp); m++ {
		var buf []rune
		sel := rp[m]
		text := cp.text
		for i := 0; i < len(text); i++ {
			c := text[i]
			if c == '\\' && i < len(text)-1 {
				i++
				c = text[i]
				if '1' <= c && c <= '9' {
					j := int(c - '0')
					nr := sel.r[j].q1 - sel.r[j].q0
					if nr > RBUFSIZE {
						// untested: -D never matches a backref group longer than RBUFSIZE
						a.editerror("replacement string too long")
					}
					rbuf := make([]rune, nr)
					t.file.b.Read(sel.r[j].q0, rbuf)
					buf = append(buf, rbuf...)
				} else {
					buf = append(buf, c)
				}
			} else if c != '&' {
				buf = append(buf, c)
			} else {
				nr := sel.r[0].q1 - sel.r[0].q0
				if nr > RBUFSIZE {
					// untested: -D never matches text longer than RBUFSIZE for &
					a.editerror("right hand side too long in substitution")
				}
				rbuf := make([]rune, nr)
				t.file.b.Read(sel.r[0].q0, rbuf)
				buf = append(buf, rbuf...)
			}
		}
		a.elogreplace(t.file, sel.r[0].q0, sel.r[0].q1, buf)
		delta -= sel.r[0].q1 - sel.r[0].q0
		delta += len(buf)
		didsub = true
		if cp.flag == 0 {
			break
		}
	}
	if !didsub && a.nest == 0 {
		a.editerror("no substitution")
	}
	t.q0 = a.editAddr.r.q0
	t.q1 = a.editAddr.r.q1
	return 1
}

func (a *Acme) g_cmd(t *Text, cp *Cmd) int {
	if t.file != a.editAddr.f {
		// g/v always run on the addressed file's text, so the mismatch is an
		// internal-error guard not reached via -D.
		// untested
		a.warning(nil, "internal error: g_cmd f!=addr.f\n")
		return 0
	}
	if !a.re.rxcompile(cp.re) {
		a.editerror("bad regexp in g command")
	}
	matched := a.re.rxexecute(t, nil, a.editAddr.r.q0, a.editAddr.r.q1, &a.re.sel)
	if matched != (cp.cmdc == 'v') { // xor: g runs on match, v on no-match
		t.q0 = a.editAddr.r.q0
		t.q1 = a.editAddr.r.q1
		return a.cmdexec(t, cp.cmd)
	}
	return 1
}

func (a *Acme) x_cmd(t *Text, cp *Cmd) int {
	if cp.re != nil {
		a.looper(t.file, cp, cp.cmdc == 'x')
	} else {
		a.linelooper(t.file, cp)
	}
	return 1
}

// X_cmd runs cp.cmd on every file whose window matches (X) or doesn't match (Y)
// cp.re (ecmd.c). The window locking/refcounting is dropped (single-threaded -D).
func (a *Acme) X_cmd(t *Text, cp *Cmd) int {
	a.filelooper(t, cp, cp.cmdc == 'X')
	return 1
}

func (a *Acme) filelooper(t *Text, cp *Cmd, xy bool) {
	if a.glooping != 0 {
		// Reachable (nested X/Y), but the editerror longjmp unwinds past the C
		// -D driver's row lock and aborts the oracle, so it can't be matched.
		// untested
		a.editerror("can't nest %c command", cp.cmdc)
	}
	a.glooping++
	a.nest++
	defer func() { a.glooping--; a.nest-- }()

	var ws []*Window
	for _, c := range a.row.col {
		for _, w := range c.w {
			tb := &w.body
			// only use this window if it's the current window for the file
			if tb.file.curtext != tb {
				continue
			}
			// no auto-execute on files without names
			if cp.re == nil && tb.file.nname() == 0 {
				continue
			}
			if cp.re == nil || a.filematch(tb.file, cp.re) == xy {
				ws = append(ws, w)
			}
		}
	}

	// Add a ref to every window so that a window torn down by a looped command
	// (e.g. X/.../D othername, which deletes a window other than the one being
	// visited) stays alive until the loop finishes; globalincref makes windows
	// created during the loop start with the extra ref too. (ecmd.c filelooper
	// alllocker / globalincref.) winclose never mutates row.col (only colclose
	// does), so iterating it live during the decref pass is safe. The defer mirrors
	// C's allwindows(alllocker, 0) running after the loop, but also on the editerror
	// unwind path (C relies on the longjmp + resetxec resetting Glooping/nest).
	a.allwindows(func(w *Window) { w.ref.Add(1) })
	a.globalincref = true
	defer func() {
		a.globalincref = false
		a.allwindows(func(w *Window) { a.winclose(w) })
	}()

	// Unlock the X/Y window so each target can be locked in turn, re-locking it on
	// the way out (ecmd.c filelooper: winunlock(t->w) ... winlock(t->w)). The defer
	// restores the lock even if a target's cmdexec editerrors (a panic recovered in
	// editcmd); C relies on the longjmp for the same effect. The window is held
	// locked on entry by every execute path (mouse/keyboard winlock, the -D b2
	// verb, and xfideventwrite's winlock).
	if t != nil && t.w != nil {
		a.winunlock(t.w)
		defer a.winlock(t.w, int(cp.cmdc))
	}

	for _, w := range ws {
		// winlock each target around its cmdexec (the inner func's defer unlocks it
		// even on an editerror unwind), as C does per target.
		func() {
			a.winlock(w, int(cp.cmdc))
			defer a.winunlock(w)
			a.cmdexec(&w.body, cp.cmd)
		}()
	}
}

// allwindows calls f for every window in every column (rows.c allwindows).
func (a *Acme) allwindows(f func(*Window)) {
	for _, c := range a.row.col {
		for _, w := range c.w {
			f(w)
		}
	}
}

// looper collects the ranges matched (x) or unmatched (y) by cp.re and runs
// cp.cmd on each (ecmd.c).
func (a *Acme) looper(f *File, cp *Cmd, xy bool) {
	r := a.editAddr.r
	op := r.q0
	if xy {
		op = -1
	}
	a.nest++
	if !a.re.rxcompile(cp.re) {
		a.editerror("bad regexp in %c command", cp.cmdc)
	}
	var rp []Range
	for p := r.q0; p <= r.q1; {
		var tr Range
		if !a.re.rxexecute(f.curtext, nil, p, r.q1, &a.re.sel) { // no match; y still runs
			if xy || op > r.q1 {
				break
			}
			tr.q0 = op
			tr.q1 = r.q1
			p = r.q1 + 1 // exit next loop
		} else {
			if a.re.sel.r[0].q0 == a.re.sel.r[0].q1 { // empty match?
				if a.re.sel.r[0].q0 == op {
					p++
					continue
				}
				p = a.re.sel.r[0].q1 + 1
			} else {
				p = a.re.sel.r[0].q1
			}
			if xy {
				tr = a.re.sel.r[0]
			} else {
				tr.q0 = op
				tr.q1 = a.re.sel.r[0].q0
			}
		}
		op = a.re.sel.r[0].q1
		rp = append(rp, tr)
	}
	a.loopcmd(f, cp.cmd, rp)
	a.nest--
}

// linelooper runs cp.cmd on each line in the address range (ecmd.c).
func (a *Acme) linelooper(f *File, cp *Cmd) {
	a.nest++
	var rp []Range
	r := a.editAddr.r
	var a3 Address
	a3.f = f
	a3.r.q0 = r.q0
	a3.r.q1 = r.q0
	linesel := a.lineaddr(0, a3, 1).r
	for p := r.q0; p < r.q1; p = a3.r.q1 {
		a3.r.q0 = a3.r.q1
		if p != r.q0 || linesel.q1 == p {
			linesel = a.lineaddr(1, a3, 1).r
		}
		if linesel.q0 >= r.q1 {
			// lineaddr(1) from position p returns a line starting at p < r.q1, so
			// linesel.q0 is always inside the range here.
			// unreachable
			break
		}
		if linesel.q1 >= r.q1 {
			linesel.q1 = r.q1
		}
		if linesel.q1 > linesel.q0 && linesel.q0 >= a3.r.q1 && linesel.q1 > a3.r.q1 {
			a3.r = linesel
			rp = append(rp, linesel)
			continue
		}
		// each in-range line advances a3 (lineaddr always returns [p, line-end]
		// with line-end > p), so the loop never falls through to here.
		// unreachable
		break
	}
	a.loopcmd(f, cp.cmd, rp)
	a.nest--
}

func (a *Acme) loopcmd(f *File, cp *Cmd, rp []Range) {
	for _, r := range rp {
		f.curtext.q0 = r.q0
		f.curtext.q1 = r.q1
		a.cmdexec(f.curtext, cp)
	}
}

func (a *Acme) m_cmd(t *Text, cp *Cmd) int {
	var dot Address
	mkaddr(&dot, t.file)
	addr2 := a.cmdaddress(cp.mtaddr, dot, 0)
	if cp.cmdc == 'm' {
		a.move(t.file, addr2)
	} else {
		a.copyText(t.file, addr2)
	}
	return 1
}

// move relocates the addressed text to addr2 (ecmd.c move).
func (a *Acme) move(f *File, addr2 Address) {
	if a.editAddr.f != addr2.f || a.editAddr.r.q1 <= addr2.r.q0 {
		a.elogdelete(f, a.editAddr.r.q0, a.editAddr.r.q1)
		a.copyText(f, addr2)
	} else if a.editAddr.r.q0 >= addr2.r.q1 {
		a.copyText(f, addr2)
		a.elogdelete(f, a.editAddr.r.q0, a.editAddr.r.q1)
	} else if a.editAddr.r.q0 == addr2.r.q0 && a.editAddr.r.q1 == addr2.r.q1 {
		// move to self; no-op
	} else {
		a.editerror("move overlaps itself")
	}
}

// copyText copies the addressed text to addr2 (ecmd.c copy).
func (a *Acme) copyText(f *File, addr2 Address) {
	buf := make([]rune, a.editAddr.r.q1-a.editAddr.r.q0)
	f.b.Read(a.editAddr.r.q0, buf)
	a.eloginsert(addr2.f, addr2.r.q1, buf)
}

// nl_cmd handles a bare address or a lone newline command: it shows the line
// containing dot, or the addressed range (ecmd.c).
func (a *Acme) nl_cmd(t *Text, cp *Cmd) int {
	f := t.file
	addr := a.editAddr // the address cmdexec already evaluated, if any
	if cp.addr == nil {
		// no address: first put it on newline boundaries
		var ad Address
		mkaddr(&ad, f)
		addr = a.lineaddr(0, ad, -1)
		ad = a.lineaddr(0, ad, 1)
		addr.r.q1 = ad.r.q1
		if addr.r.q0 == t.q0 && addr.r.q1 == t.q1 {
			// already exactly this line: advance to the next one
			mkaddr(&ad, f)
			addr = a.lineaddr(1, ad, 1)
		}
	}
	a.editAddr = addr
	a.textshow(t, addr.r.q0, addr.r.q1, true)
	return 1
}

// pipe_cmd runs the <, |, > pipe commands (ecmd.c).
func (a *Acme) pipe_cmd(t *Text, cp *Cmd) int {
	a.runpipe(t, byte(cp.cmdc), cp.text, Inserting)
	return 1
}

// execChunk is one piece of a running command's output, streamed from the
// reader goroutine to the main goroutine -- the Go stand-in for C's child
// writing to the editout/cons 9P files. A done chunk marks the command exited
// and all its output read.
type execChunk struct {
	data  []rune
	isErr bool   // from the command's stderr (always routed to +Errors)
	start bool   // the command started: show name in the row tag (WCmd)
	done  bool   // the command exited: remove name from the row tag (WWait)
	name  []rune // the running-command name (with start and done)
	pid   int    // process-group id of the started command (start; for Kill)
}

// firstcmdname is the running-command label acme shows in the row tag: the
// command's first word, basename only, with a trailing blank (exec.c runproc).
func firstcmdname(s string) []rune {
	s = strings.TrimLeft(s, " \t\n")
	if i := strings.IndexAny(s, " \t\n"); i >= 0 {
		s = s[:i]
	}
	if j := strings.LastIndexByte(s, '/'); j >= 0 {
		// untested: -D button-2/pipe commands are bare names (echo, rc), no '/'
		s = s[j+1:]
	}
	return []rune(s + " ")
}

// execInsertName / execRemoveName show and hide a running command's name at the
// front of the row tag, the way acme's waitthread does on WCmd/WWait (acme.c):
// the name appears while the command runs and is removed when it exits.
func (a *Acme) execInsertName(name []rune) {
	t := &a.row.tag
	a.textcommit(t, true)
	a.textinsert(t, 0, name, true)
	a.textsetselect(t, 0, 0)
}

func (a *Acme) execRemoveName(name []rune) {
	t := &a.row.tag
	a.textcommit(t, true)
	if a.search(t, name, false) {
		a.textdelete(t, t.q0, t.q1, true)
		a.textsetselect(t, 0, 0)
	}
}

// runpipe runs an external command connected to the addressed text: | pipes dot
// through it, < replaces dot with its output, > sends dot to it (ecmd.c runpipe).
// Like C it starts the command asynchronously (a.run -> a real subprocess) and
// then blocks applying the streamed output until the command finishes -- C drops
// row.lk and does recvul(cedit) while its xfid procs apply the editout writes;
// here the same main goroutine receives the chunks and applies them inline. | and
// < send stdout to the buffer (eloginsert at q1, merged in arrival order); > and
// every command's stderr go to the +Errors window.
func (a *Acme) runpipe(t *Text, cmd byte, cr []rune, state int) {
	r := skipbl(cr)
	if len(r) == 0 {
		a.editerror("no command specified for %c", rune(cmd))
	}
	if state == Inserting {
		t.q0 = a.editAddr.r.q0
		t.q1 = a.editAddr.r.q1
		if cmd == '<' || cmd == '|' {
			a.elogdelete(t.file, t.q0, t.q1)
		}
	}
	dir := a.dirname(t, nil)
	if len(dir) == 1 && dir[0] == '.' {
		// untested: needs a pipe in a window whose directory cleans to "."
		dir = nil
	}
	a.editing = state
	// C runpipe rebuilds the command with the pipe char back on the front
	// (s[0]=cmd; runemove(s+1, r, n)) and hands the whole thing to run, which
	// strips it again in runproc. Pass it in the same form.
	if a.run(t, string(rune(cmd))+string(r), string(dir), "", "") { // edit pipes: no chord arg/addr
		for {
			ch := <-a.cexec
			if ch.start {
				a.execInsertName(ch.name)
				continue
			}
			if ch.done {
				a.nexec--
				a.execRemoveName(ch.name)
				break
			}
			if !ch.isErr && (cmd == '|' || cmd == '<') {
				if state == Collecting {
					// filelist's '<' form: gather the program's output as the
					// file-name list instead of inserting it (ecmd.c edittext
					// Collecting case).
					a.collection = append(a.collection, ch.data...)
				} else {
					a.eloginsert(t.file, t.q1, ch.data)
				}
			} else {
				a.consToErrors(ch.data)
			}
		}
	}
	a.editing = Inactive
}

// runShell reports whether command t (pipe char already stripped) must be run
// through the shell rather than exec'd directly, mirroring exec.c runproc's
// "goto Hard" decision: $acmeshell forces the shell, as does a command longer
// than C's 512-byte stack buffer, any control character, or a shell
// metacharacter. A command that is all blanks scans clean and is left to the
// direct path, where strings.Fields yields nothing and run() reports it didn't
// start (C's "goto Fail").
func (a *Acme) runShell(t string) bool {
	if a.acmeshell != "" {
		return true
	}
	if len(t) > 512-10 { // C: strlen(t) > sizeof buf-10
		return true
	}
	for _, r := range t {
		if r == ' ' || r == '\t' {
			continue
		}
		if r < ' ' {
			return true
		}
		if strings.ContainsRune("#;&|^$=`'{}()<>[]*?~/", r) {
			return true
		}
	}
	return false
}

// shellPath is the shell external commands run under on the runShell path:
// $acmeshell (PATH-searched by exec.Command) if set, else the bundled rc
// (exec.c runproc: shell = acmeshell; if(shell==nil) shell = "rc").
func (a *Acme) shellPath() string {
	if a.acmeshell != "" {
		return a.acmeshell
	}
	return os.Getenv("PLAN9") + "/bin/rc"
}

// run starts external command s asynchronously and returns whether it started
// (exec.c run/runproc). Like C's runproc, s is the WHOLE command including any
// leading pipe char: run derives the row-tag label from the first token of the
// full command (pipe char and all) before splitting the pipe char off, so runpipe
// and execute pass the command in the same form C does. A reader goroutine streams
// the child's stdout/stderr back over a.cexec; the caller (runpipe inline, or the
// -D "wait" verb) applies the chunks on the main goroutine, so acme state stays
// single-goroutine. The split-off pipe char wires stdin/stdout for an edit pipe;
// no pipe char is a button-2 command whose output goes to +Errors via cons. A
// simple command is run directly (C's execvp path); one that runShell flags goes
// through the shell. The Go -D harness cannot post a 9P service for the child to
// mount, so the child writes to ordinary pipes we read here, not editout/cons.
func (a *Acme) run(t *Text, s, dir, argaddr, xarg string) bool {
	// The row-tag label is the first token of the whole command, INCLUDING the
	// pipe char, exactly as C runproc computes c->name from s before stripping
	// pipechar (so "|tr" shows "|tr " in the tag). basename still wins for a
	// "|/bin/tr" form, as C's utfrrune does.
	name := firstcmdname(s)
	// Then skip leading whitespace and split off a leading <|> pipe char (runproc:
	// while(*t==' ')t++; if(*t=='<'||'|'||'>')pipechar=*t++). body is exec'd/shell-run.
	body := strings.TrimLeft(s, " \t\n")
	var pipechar byte
	if len(body) > 0 && (body[0] == '<' || body[0] == '|' || body[0] == '>') {
		pipechar = body[0]
		body = body[1:]
	}
	var c *exec.Cmd
	if a.runShell(body) {
		// shell path (C runproc Hard): append the 2-1 chord argument, quoted, to the
		// command line ("%s '%s'"). (C's note about an embedded quote applies.)
		cmd := body
		if xarg != "" {
			// untested: edit-command error / live edit-pipe / pids-race path not exercised by -D
			cmd = body + " '" + xarg + "'"
		}
		c = exec.Command(a.shellPath(), "-c", cmd)
	} else {
		f := strings.Fields(body)
		if len(f) == 0 {
			// untested: a blank button-2 selection runs no command
			return false
		}
		if xarg != "" {
			f = append(f, xarg) // C runproc: av[ac++] = arg (the chord argument)
		}
		c = exec.Command(f[0], f[1:]...)
	}
	if dir != "" {
		// untested: needs a command in a window with a non-"." directory on disk
		c.Dir = dir
	}
	// Command environment (exec.c runproc): clear any inherited acme-specific
	// vars (C unsetenvs them), then set winid and, for a named window, % and
	// samfile. The child inherits $NAMESPACE, so with winid it can reach its
	// window via the posted 9P service (/mnt/acme/$winid/...). acmeaddr is not
	// set yet (it needs getarg's expand path).
	var env []string
	for _, e := range os.Environ() {
		switch {
		case strings.HasPrefix(e, "winid="), strings.HasPrefix(e, "%="),
			strings.HasPrefix(e, "samfile="), strings.HasPrefix(e, "acmeaddr="):
			// untested: the test env has no winid=/%=/samfile=/acmeaddr= to drop
			continue // C runproc unsetenvs these before setting fresh ones
		default:
			env = append(env, e)
		}
	}
	winid := 0
	if t != nil && t.w != nil {
		winid = t.w.id
		// untested: edit-command error / live edit-pipe / pids-race path not exercised by -D
	} else if a.activewin != nil {
		// untested: a windowless button-2 command falls back to the active window
		winid = a.activewin.id
	}
	env = append(env, fmt.Sprintf("winid=%d", winid))
	if t != nil && t.w != nil && t.w.body.file.nname() > 0 {
		// untested: needs a command in a named window (awkward tag offset)
		name := string(t.w.body.file.name)
		env = append(env, "%="+name, "samfile="+name)
	}
	if argaddr != "" {
		// the 2-1 chord argument's address (exec.c runproc putenv acmeaddr)
		// untested: edit-command error / live edit-pipe / pids-race path not exercised by -D
		env = append(env, "acmeaddr="+argaddr)
	}
	c.Env = env
	if (pipechar == '|' || pipechar == '>') && t != nil {
		in := make([]rune, t.q1-t.q0)
		t.file.b.Read(t.q0, in)
		c.Stdin = strings.NewReader(string(in))
	}
	stdout, _ := c.StdoutPipe()
	stderr, _ := c.StderrPipe()
	// Own process group so Kill can signal the whole command (C's PNGROUP note).
	c.SysProcAttr = &syscall.SysProcAttr{Setpgid: true}
	startErr := c.Start()
	a.nexec++
	pid := 0
	if startErr == nil {
		pid = c.Process.Pid // == the new process-group id (Setpgid)
	}
	// Edit pipes (| < >) report over cexec, drained inline by runpipe; a
	// button-2 command (pipechar 0) reports its cons output over ccons, drained
	// by the live consthread or the -D wait verb -- C's editout vs cons split.
	out := a.cexec
	if pipechar == 0 {
		out = a.ccons
	}
	cmd := &Command{pid: pid, name: name}
	// C spawns an independent "run proc" (sends ccommand / WCmd) and a "wait proc"
	// (delivers the exit to cwait / WWait); the two race, so a child that exits
	// before its registration is seen lands in the pids list (handled by
	// registerCommand). Mirror that: for a button-2 command, register from a
	// separate goroutine and reap (with the exit message) from the reader/wait
	// goroutine below. Edit pipes keep the start/done markers inline on cexec,
	// drained synchronously by runpipe.
	if pipechar == 0 {
		go func() { a.ccommand <- cmd }()
	}
	go func() {
		if pipechar != 0 {
			a.cexec <- execChunk{start: true, name: name, pid: pid}
		}
		msg := ""
		if startErr != nil {
			// untested: -D commands (echo, etc.) always start successfully; a
			// failed exec streams its error to +Errors as a real child's would.
			out <- execChunk{data: []rune(fmt.Sprintf("%s: %v\n", s, startErr)), isErr: true}
		} else {
			var wg sync.WaitGroup
			read := func(rd io.Reader, isErr bool) {
				defer wg.Done()
				buf := make([]byte, 8192)
				for {
					n, err := rd.Read(buf)
					if n > 0 {
						out <- execChunk{data: []rune(string(buf[:n])), isErr: isErr}
					}
					if err != nil {
						break
					}
				}
			}
			wg.Add(2)
			go read(stdout, false)
			go read(stderr, true)
			wg.Wait()
			if werr := c.Wait(); werr != nil {
				// abnormal exit (or signal): C's Waitmsg.msg is non-empty.
				// untested: edit-command error / live edit-pipe / pids-race path not exercised by -D
				msg = werr.Error()
			}
		}
		if pipechar == 0 {
			a.cwait <- &Waitmsg{pid: pid, msg: msg}
		} else {
			a.cexec <- execChunk{done: true, name: name}
		}
	}()
	return true
}

// registerCommand adds a started button-2 command to the running-command list
// and shows its name in the row tag (acme.c waitthread WCmd). Called by
// waitthread (under row.lk) and the -D wait verb (single-threaded).
func (a *Acme) registerCommand(c *Command) {
	// Has this command already exited (its WWait beat this WCmd)? If so it is in
	// pids; report any exit message and free it without ever showing its name.
	var lastp *Pid
	for p := a.pids; p != nil; p = p.next {
		if p.pid == c.pid {
			if p.msg != "" {
				a.warning(c.md, "%s\n", p.msg)
			}
			if lastp == nil {
				a.pids = p.next
			} else {
				// untested: edit-command error / live edit-pipe / pids-race path not exercised by -D
				lastp.next = p.next
			}
			a.freecmd(c)
			return
		}
		// untested: edit-command error / live edit-pipe / pids-race path not exercised by -D
		lastp = p
	}
	c.next = a.command
	a.command = c
	a.execInsertName(c.name)
}

// reapCommand reaps a finished button-2 command (acme.c waitthread WWait): find
// it by pid, drop its name from the tag, and report an abnormal exit; if it is
// not yet registered (WWait beat WCmd), stash the status in pids for WCmd. Called
// by waitthread and the -D wait verb.
func (a *Acme) reapCommand(w *Waitmsg) {
	a.nexec--
	var c, lc *Command
	for cc := a.command; cc != nil; cc = cc.next {
		if cc.pid == w.pid {
			c = cc
			if lc == nil {
				a.command = cc.next
			} else {
				// untested: edit-command error / live edit-pipe / pids-race path not exercised by -D
				lc.next = cc.next
			}
			break
		}
		// untested: edit-command error / live edit-pipe / pids-race path not exercised by -D
		lc = cc
	}
	if c == nil {
		// exited before registerCommand saw it: stash for WCmd (acme.c pids),
		// unless it is a helper thread's status.
		if !strings.HasPrefix(w.msg, "libthread") {
			a.pids = &Pid{pid: w.pid, msg: w.msg, next: a.pids}
		}
		return
	}
	a.execRemoveName(c.name)
	if w.msg != "" {
		// "name: exit ..." to the command's +Errors (acme.c WWait warning). c.name
		// carries firstcmdname's trailing space (C's nname-1 strips it); Go's Wait
		// error already reads "exit status N".
		name := c.name
		if n := len(name); n > 0 && name[n-1] == ' ' {
			name = name[:n-1]
		}
		a.warning(c.md, "%s: %s\n", string(name), w.msg)
	}
	a.freecmd(c)
}

// freecmd releases a reaped command (acme.c waitthread Freecmd). The struct is
// GC'd; only the mount-dir id refcount needs an explicit drop. Button-2 commands
// are never iseditcmd, so there is no cedit signal here.
func (a *Acme) freecmd(c *Command) {
	a.fsysdelid(c.md)
}

// runlocal runs a Local command in acme's own namespace (exec.c run/runproc
// with newns=FALSE): the child's stdout is discarded and its stderr goes to
// acme's error output, so nothing is routed to a window. C runs it
// asynchronously in a forked file-descriptor group; the Go port runs it
// synchronously because its output never appears on the -D protocol stream, so
// sync vs async makes no observable difference there. (A live editor should run
// it async; that arrives with the run() refactor.) Direct exec for a metachar-
// free command, otherwise via rc, mirroring run().
func (a *Acme) runlocal(s, dir, xarg, argaddr string) {
	s = strings.TrimLeft(s, " \t\n")
	if s == "" {
		// untested: Local with no command is a no-op
		return
	}
	var c *exec.Cmd
	if a.runShell(s) {
		// shell path (C runproc Hard): append the chord argument, quoted, as in run().
		// untested: edit-command error / live edit-pipe / pids-race path not exercised by -D
		cmd := s
		if xarg != "" {
			// untested: edit-command error / live edit-pipe / pids-race path not exercised by -D
			cmd = s + " '" + xarg + "'"
		}
		// untested: edit-command error / live edit-pipe / pids-race path not exercised by -D
		c = exec.Command(a.shellPath(), "-c", cmd)
	} else {
		f := strings.Fields(s)
		if xarg != "" {
			// untested: edit-command error / live edit-pipe / pids-race path not exercised by -D
			f = append(f, xarg) // C runproc: av[ac++] = arg (the chord argument)
		}
		c = exec.Command(f[0], f[1:]...)
	}
	if dir != "" {
		// untested: a Local command in a window with a non-"." directory on disk
		c.Dir = dir
	}
	if argaddr != "" {
		// the 2-1 chord argument's address (exec.c runproc putenv acmeaddr)
		// untested: edit-command error / live edit-pipe / pids-race path not exercised by -D
		var env []string
		for _, e := range os.Environ() {
			// untested: edit-command error / live edit-pipe / pids-race path not exercised by -D
			if !strings.HasPrefix(e, "acmeaddr=") {
				// untested: edit-command error / live edit-pipe / pids-race path not exercised by -D
				env = append(env, e)
			}
		}
		// untested: edit-command error / live edit-pipe / pids-race path not exercised by -D
		c.Env = append(env, "acmeaddr="+argaddr)
	}
	c.Stdout = nil // discarded, like C's newns=FALSE /dev/null
	// stderr -> acme's error pipe (erroutfd) so it reaches +Errors via
	// acmeerrorproc, exactly as C's runproc dups erroutfd; before the pipe
	// exists (-D / no display) fall back to acme's own stderr.
	if a.erroutfd != nil {
		// untested: edit-command error / live edit-pipe / pids-race path not exercised by -D
		c.Stderr = a.erroutfd
	} else {
		c.Stderr = a.stderr
	}
	if err := c.Run(); err != nil {
		// untested: -D Local commands run successfully; a failed exec mirrors a
		// child whose error reaches acme's stderr, not +Errors.
		// untested: edit-command error / live edit-pipe / pids-race path not exercised by -D
		fmt.Fprintf(a.stderr, "%s: %v\n", s, err)
	}
}

// consToErrors appends r to the shared +Errors window, the destination of an
// external command's cons output (xfid.c QCons -> errorwin -> BodyTag).
func (a *Acme) consToErrors(r []rune) {
	ew := a.errorwin1(nil, nil)
	ew.owner = 'X'
	a.bodyTag(ew, &ew.body, qCons, r)
}

// errorsNc returns the rune count of the shared +Errors window's body, or 0 if
// it does not exist yet; the -D "wait" verb polls it for async command output.
func (a *Acme) errorsNc() int {
	for _, c := range a.row.col {
		for _, w := range c.w {
			nm := w.body.file.name
			if n := len(nm); n >= 7 && string(nm[n-7:]) == "+Errors" {
				return w.body.file.b.nc()
			}
		}
	}
	return 0
}

// u_cmd runs Undo (or Redo for a negative count) num times (ecmd.c).
func (a *Acme) u_cmd(t *Text, cp *Cmd) int {
	n := cp.num
	flag := true
	if n < 0 {
		// untested: a negative count (Redo via u) needs a prior undo to redo
		n = -n
		flag = false
	}
	oseq := -1
	for n > 0 && t.file.seq != oseq {
		n--
		oseq = t.file.seq
		a.undo(t, nil, nil, flag, false, nil)
	}
	return 1
}

// printposn reports the line (or character) position of the current address
// to the +Errors window (ecmd.c).
func (a *Acme) printposn(t *Text, mode int) {
	if t != nil && t.file != nil && t.file.name != nil {
		// untested: '=' on a named window prints a name prefix, but the report
		// goes to stderr and the named-window tag offset is awkward to script
		// untested: edit-command error / live edit-pipe / pids-race path not exercised by -D
		a.warning(nil, "%s:", string(t.file.name))
	}
	switch mode {
	case PosnChars:
		a.warning(nil, "#%d", a.editAddr.r.q0)
		if a.editAddr.r.q1 != a.editAddr.r.q0 {
			a.warning(nil, ",#%d", a.editAddr.r.q1)
		}
		a.warning(nil, "\n")
	case PosnLineChars:
		l1n, r1 := a.nlcount(t, 0, a.editAddr.r.q0)
		l1 := 1 + l1n
		l2n, r2 := a.nlcount(t, a.editAddr.r.q0, a.editAddr.r.q1)
		l2 := l1 + l2n
		if l2 == l1 {
			r2 += r1
		}
		a.warning(nil, "%d+#%d", l1, r1)
		if l2 != l1 {
			a.warning(nil, ",%d+#%d", l2, r2)
		}
		a.warning(nil, "\n")
	default: // PosnLine
		l1n, _ := a.nlcount(t, 0, a.editAddr.r.q0)
		l1 := 1 + l1n
		l2n, _ := a.nlcount(t, a.editAddr.r.q0, a.editAddr.r.q1)
		l2 := l1 + l2n
		// check if addr ends with '\n'
		if a.editAddr.r.q1 > 0 && a.editAddr.r.q1 > a.editAddr.r.q0 && a.textreadc(t, a.editAddr.r.q1-1) == '\n' {
			l2--
		}
		a.warning(nil, "%d", l1)
		if l2 != l1 {
			a.warning(nil, ",%d", l2)
		}
		a.warning(nil, "\n")
	}
}

// eq_cmd prints the position of dot (the = command) (ecmd.c).
func (a *Acme) eq_cmd(t *Text, cp *Cmd) int {
	mode := PosnLine
	switch {
	case len(cp.text) == 0:
		mode = PosnLine
	case len(cp.text) == 1 && cp.text[0] == '#':
		mode = PosnChars
	case len(cp.text) == 1 && cp.text[0] == '+':
		mode = PosnLineChars
	default:
		a.editerror("newline expected")
	}
	a.printposn(t, mode)
	return 1
}

// pfilename prints f's name with the dirty/current markers (ecmd.c).
func (a *Acme) pfilename(f *File) {
	w := f.curtext.w
	// same check for dirty as in settag, but we know ncache==0
	dirty := !w.isdir && !w.isscratch && f.mod
	c0 := byte(' ')
	if dirty {
		c0 = '\''
	}
	c2 := byte(' ')
	if a.curtext != nil && a.curtext.file == f {
		c2 = '.'
	}
	a.warning(nil, "%c%c%c %s\n", c0, '+', c2, string(f.name))
}

// allfilecheck warns when another window already has the name being set (ecmd.c).
func (a *Acme) allfilecheck(f *File, name []rune) {
	for _, c := range a.row.col {
		for _, w := range c.w {
			if w.body.file == f {
				continue
			}
			if runeEq(name, w.body.file.name) {
				a.warning(nil, "warning: duplicate file name \"%s\"\n", string(name))
			}
		}
	}
}

// cmdname resolves (and optionally sets) f's file name from the command's name
// argument, returning the resolved name (ecmd.c).
func (a *Acme) cmdname(f *File, str []rune, set bool) []rune {
	var r []rune
	if len(str) == 0 {
		// no name; use existing
		if f.nname() == 0 {
			return nil
		}
		return append([]rune(nil), f.name...)
	}
	s := skipbl(str)
	if len(s) > 0 {
		if s[0] == '/' {
			r = append([]rune(nil), s...)
		} else {
			r = a.dirname(f.curtext, append([]rune(nil), s...))
		}
		a.allfilecheck(f, r)
		if f.nname() == 0 {
			set = true
		}
	}
	if set && !runeEq(r, f.name) {
		a.filemark(f)
		f.mod = true
		f.curtext.w.dirty = true
		a.winsetname(f.curtext.w, r)
	}
	return r
}

// f_cmd sets or prints the file name (ecmd.c).
func (a *Acme) f_cmd(t *Text, cp *Cmd) int {
	a.cmdname(t.file, cp.text, true)
	a.pfilename(t.file)
	return 1
}

// w_cmd writes the addressed text to a file (ecmd.c).
func (a *Acme) w_cmd(t *Text, cp *Cmd) int {
	f := t.file
	if f.seq == a.seq {
		// untested: requires a multi-command edit stream that modifies the file
		// and then writes it within the same sequence
		// untested: edit-command error / live edit-pipe / pids-race path not exercised by -D
		a.editerror("can't write file with pending modifications")
	}
	r := a.cmdname(f, cp.text, false)
	if r == nil {
		a.editerror("no name specified for 'w' command")
	}
	a.putfile(f, a.editAddr.r.q0, a.editAddr.r.q1, r)
	return 1
}

// tofile finds the (unique) window whose file name equals r (ecmd.c).
func (a *Acme) tofile(r []rune) *File {
	rr := skipbl(r)
	var found *File
	for _, c := range a.row.col {
		for _, w := range c.w {
			if found != nil {
				break
			}
			if w.isscratch || w.isdir {
				// untested: -D 'b' targets ordinary windows
				continue
			}
			t := &w.body
			if t.file.curtext != t {
				// untested: -D windows are each their file's only text
				continue
			}
			if runeEq(rr, t.file.name) {
				found = t.file
			}
		}
	}
	if found == nil {
		a.editerror("no such file\"%s\"", string(rr))
	}
	return found
}

// b_cmd switches the current file to the named window (the b command) (ecmd.c).
func (a *Acme) b_cmd(t *Text, cp *Cmd) int {
	f := a.tofile(cp.text)
	if a.nest == 0 {
		a.pfilename(f)
	}
	a.curtext = f.curtext
	return 1
}

// filelist returns the file-name list argument. The '<' form runs an external
// program and uses its output as the list (ecmd.c filelist: clearcollection;
// runpipe('<', Collecting); return collection).
func (a *Acme) filelist(t *Text, r []rune) []rune {
	if len(r) == 0 {
		return nil
	}
	r = skipbl(r)
	if len(r) == 0 || r[0] != '<' {
		return append([]rune{}, r...)
	}
	a.collection = nil
	a.runpipe(t, '<', r[1:], Collecting)
	return a.collection
}

// D1 closes the window of t if it is the only text on its file or it is clean
// (ecmd.c).
func (a *Acme) D1(t *Text) {
	if len(t.w.body.file.text) > 1 || a.winclean(t.w, false) {
		a.colclose(t.col, t.w, true)
	}
}

// D_cmd closes the named windows, or the current one with no name (ecmd.c).
func (a *Acme) D_cmd(t *Text, cp *Cmd) int {
	list := a.filelist(t, cp.text)
	if list == nil {
		// untested: 'D' with no name closes the executing window, which breaks
		// the post-command elogapply on the just-freed file in this harness
		// untested: edit-command error / live edit-pipe / pids-race path not exercised by -D
		a.D1(t)
		return 1
	}
	dir := a.dirname(t, nil)
	r := skipbl(list)
	for {
		rest := findbl(r)
		name := r[:len(r)-len(rest)]
		var rs []rune
		// first time through, name could be empty (delete the empty-name file)
		if len(name) == 0 || name[0] == '/' || len(dir) == 0 {
			rs = append([]rune{}, name...)
		} else {
			// untested: needs a relative name in a window with a real directory
			n := append([]rune(nil), dir...)
			n = append(n, '/')
			n = append(n, name...)
			rs = cleanrname(n)
		}
		w := a.lookfile(rs)
		if w == nil {
			a.editerror("no such file %s", string(rs))
		}
		a.D1(&w.body)
		r = skipbl(rest)
		if len(r) == 0 {
			break
		}
	}
	return 1
}

// B_cmd opens the named files as new windows (ecmd.c).
func (a *Acme) B_cmd(t *Text, cp *Cmd) int {
	list := a.filelist(t, cp.text)
	if list == nil {
		a.editerror("no file name given")
	}
	r := skipbl(list)
	if len(r) == 0 {
		a.new(t, t, nil, false, false, r)
	} else {
		for len(r) > 0 {
			rest := findbl(r)
			a.new(t, t, nil, false, false, append([]rune{}, r[:len(r)-len(rest)]...))
			r = skipbl(rest)
		}
	}
	return 1
}

// readloader inserts loaded file text into f via the edit log (ecmd.c).
func (a *Acme) readloader(f *File, q0 int, r []rune) {
	if len(r) > 0 {
		a.eloginsert(f, q0, r)
	}
}

// e_cmd reads a file into the addressed range, replacing it (the e and r
// commands; e replaces the whole file after a clean check) (ecmd.c).
func (a *Acme) e_cmd(t *Text, cp *Cmd) int {
	f := t.file
	q0 := a.editAddr.r.q0
	q1 := a.editAddr.r.q1
	if cp.cmdc == 'e' {
		if !a.winclean(t.w, true) {
			a.editerror("") // winclean generated message already
		}
		q0 = 0
		q1 = f.b.nc()
	}
	allreplaced := q0 == 0 && q1 == f.b.nc()
	name := a.cmdname(f, cp.text, cp.cmdc == 'e')
	if name == nil {
		a.editerror("no file name given")
	}
	samename := runeEq(name, f.name)
	s := string(name)
	fd, err := os.Open(s)
	if err != nil {
		// untested: the OS error text (%v) is not byte-identical to the C
		// oracle's %r, so an open failure is not differize-tested
		// untested: edit-command error / live edit-pipe / pids-race path not exercised by -D
		a.editerror("can't open %s: %v", s, err)
	}
	if fi, _ := fd.Stat(); fi != nil && fi.IsDir() {
		// untested: -D cannot point an e/r command at a directory cleanly
		fd.Close()
		a.editerror("%s is a directory", s)
	}
	a.elogdelete(f, q0, q1)
	data, _ := io.ReadAll(fd)
	fd.Close()
	var r []rune
	nulls := false
	cvttorunes(data, len(data), &r, &nulls)
	a.readloader(f, q1, r)
	if nulls {
		a.warning(nil, "%s: NUL bytes elided\n", s)
	} else if allreplaced && samename {
		f.editclean = true
	}
	return 1
}

func mkaddr(a *Address, f *File) {
	a.r.q0 = f.curtext.q0
	a.r.q1 = f.curtext.q1
	a.f = f
}

// charaddr evaluates a #n character address relative to addr (ecmd.c).
func (a *Acme) charaddr(l int, addr Address, sign int) Address {
	if sign == 0 {
		addr.r.q0 = l
		addr.r.q1 = l
	} else if sign < 0 {
		addr.r.q0 -= l
		addr.r.q1 = addr.r.q0
	} else {
		addr.r.q1 += l
		addr.r.q0 = addr.r.q1
	}
	if addr.r.q0 < 0 || addr.r.q1 > addr.f.b.nc() {
		a.editerror("address out of range")
	}
	return addr
}

// lineaddr evaluates an l-line address relative to addr (ecmd.c).
func (a *Acme) lineaddr(l int, addr Address, sign int) Address {
	f := addr.f
	var res Address
	res.f = f
	if sign >= 0 {
		var p int
		if l == 0 {
			if sign == 0 || addr.r.q1 == 0 {
				res.r.q0 = 0
				res.r.q1 = 0
				return res
			}
			res.r.q0 = addr.r.q1
			p = addr.r.q1 - 1
		} else {
			var n int
			if sign == 0 || addr.r.q1 == 0 {
				p = 0
				n = 1
			} else {
				p = addr.r.q1 - 1
				if a.textreadc(f.curtext, p) == '\n' {
					n = 1
				}
				p++
			}
			for n < l {
				if p >= f.b.nc() {
					a.editerror("address out of range")
				}
				if a.textreadc(f.curtext, p) == '\n' {
					n++
				}
				p++
			}
			res.r.q0 = p
		}
		// advance past the line's terminating newline (C post-increments p
		// inside the condition, so the line includes its '\n')
		for p < f.b.nc() {
			c := a.textreadc(f.curtext, p)
			p++
			if c == '\n' {
				break
			}
		}
		res.r.q1 = p
	} else {
		p := addr.r.q0
		if l == 0 {
			res.r.q1 = addr.r.q0
		} else {
			for n := 0; n < l; {
				if p == 0 {
					n++
					if n != l {
						a.editerror("address out of range")
					}
				} else {
					c := a.textreadc(f.curtext, p-1)
					if c != '\n' {
						p--
					} else {
						n++
						if n != l {
							p--
						}
					}
				}
			}
			res.r.q1 = p
			if p > 0 {
				p--
			}
		}
		for p > 0 && a.textreadc(f.curtext, p-1) != '\n' { // lines start after a newline
			p--
		}
		res.r.q0 = p
	}
	return res
}

// nextmatch finds the next match of regexp r from p in direction sign, leaving
// the result in a.re.sel (ecmd.c).
func (a *Acme) nextmatch(f *File, r []rune, p, sign int) {
	if !a.re.rxcompile(r) {
		a.editerror("bad regexp in command address")
	}
	if sign >= 0 {
		if !a.re.rxexecute(f.curtext, nil, p, Infinity, &a.re.sel) {
			a.editerror("no match for regexp")
		}
		if a.re.sel.r[0].q0 == a.re.sel.r[0].q1 && a.re.sel.r[0].q0 == p {
			p++
			if p > f.b.nc() {
				// rxexecute self-wraps and never returns an empty match exactly
				// at p==nc, so the empty match here is always mid-buffer and p++
				// stays within the file.
				// unreachable
				p = 0
			}
			if !a.re.rxexecute(f.curtext, nil, p, Infinity, &a.re.sel) {
				// the same empty pattern matched at p, so the re-search from p+1
				// always matches too.
				// unreachable
				a.editerror("address")
			}
		}
	} else {
		if !a.re.rxbexecute(f.curtext, p, &a.re.sel) {
			a.editerror("no match for regexp")
		}
		if a.re.sel.r[0].q0 == a.re.sel.r[0].q1 && a.re.sel.r[0].q1 == p {
			p--
			if p < 0 {
				// rxbexecute self-wraps and never returns an empty match exactly
				// at p==0, so the empty match here is always mid-buffer.
				// unreachable
				p = f.b.nc()
			}
			if !a.re.rxbexecute(f.curtext, p, &a.re.sel) {
				// the same empty pattern matched at p, so the re-search matches too.
				// unreachable
				a.editerror("address")
			}
		}
	}
}

// cmdaddress evaluates a parsed Addr tree against Address a (ecmd.c).
func (a *Acme) cmdaddress(ap *Addr, addr Address, sign int) Address {
	f := addr.f
	for {
		switch ap.typ {
		case 'l', '#':
			if ap.typ == '#' {
				addr = a.charaddr(ap.num, addr, sign)
			} else {
				addr = a.lineaddr(ap.num, addr, sign)
			}
		case '.':
			mkaddr(&addr, f)
		case '$':
			addr.r.q0 = f.b.nc()
			addr.r.q1 = f.b.nc()
		case '\'':
			a.editerror("can't handle '")
		case '?':
			sign = -sign
			if sign == 0 {
				sign = -1
			}
			fallthrough
		case '/':
			q := addr.r.q1
			if sign < 0 {
				q = addr.r.q0
			}
			a.nextmatch(f, ap.re, q, sign)
			addr.r = a.re.sel.r[0]
		case '"':
			f = a.matchfile(ap.re)
			mkaddr(&addr, f)
		case '*':
			addr.r.q0 = 0
			addr.r.q1 = f.b.nc()
			return addr
		case ',', ';':
			var a1, a2 Address
			if ap.left != nil {
				a1 = a.cmdaddress(ap.left, addr, 0)
			} else {
				a1.f = addr.f
				a1.r.q0 = 0
				a1.r.q1 = 0
			}
			if ap.typ == ';' {
				f = a1.f
				addr = a1
				f.curtext.q0 = a1.r.q0
				f.curtext.q1 = a1.r.q1
			}
			if ap.next != nil {
				a2 = a.cmdaddress(ap.next, addr, 0)
			} else {
				a2.f = addr.f
				a2.r.q0 = f.b.nc()
				a2.r.q1 = f.b.nc()
			}
			if a1.f != a2.f {
				a.editerror("addresses in different files")
			}
			addr.f = a1.f
			addr.r.q0 = a1.r.q0
			addr.r.q1 = a2.r.q1
			if addr.r.q1 < addr.r.q0 {
				a.editerror("addresses out of order")
			}
			return addr
		case '+', '-':
			sign = 1
			if ap.typ == '-' {
				sign = -1
			}
			if ap.next == nil || ap.next.typ == '+' || ap.next.typ == '-' {
				addr = a.lineaddr(1, addr, sign)
			}
		default:
			// unreachable
			bug("cmdaddress")
		}
		ap = ap.next
		if ap == nil {
			break
		}
	}
	return addr
}

// cmdexec resolves cp's address and runs its command function (ecmd.c).
func (a *Acme) cmdexec(t *Text, cp *Cmd) int {
	var w *Window
	if t != nil {
		w = t.w
	}
	if w == nil && (cp.addr == nil || cp.addr.typ != '"') &&
		!runeIn(cp.cmdc, "bBnqUXY!") &&
		!(cp.cmdc == 'D' && cp.text != nil) {
		// untested: every -D edit runs on a windowed text.
		a.editerror("no current window")
	}
	i := cmdlookup(cp.cmdc) // -1 for '{'
	var f *File
	if t != nil && t.w != nil {
		t = &t.w.body
		f = t.file
		f.curtext = t
	}
	// A windowless text leaves f nil, so an address below uses the zero (" ")
	// Address, exactly as C (which only sets f when t && t->w).
	if i >= 0 && cmdtab[i].defaddr != aNo {
		ap := cp.addr
		if ap == nil && cp.cmdc != '\n' {
			cp.addr = new(Addr)
			ap = cp.addr
			ap.typ = '.'
			if cmdtab[i].defaddr == aAll {
				ap.typ = '*'
			}
		} else if ap != nil && ap.typ == '"' && ap.next == nil && cp.cmdc != '\n' {
			ap.next = new(Addr)
			ap.next.typ = '.'
			if cmdtab[i].defaddr == aAll {
				ap.next.typ = '*'
			}
		}
		if cp.addr != nil { // may be nil for '\n' only
			var dot Address
			if f != nil {
				mkaddr(&dot, f)
				a.editAddr = a.cmdaddress(ap, dot, 0)
			} else { // a " address
				// -D always has a current file, so a " address with no current
				// file is not reached.
				// untested
				a.editAddr = a.cmdaddress(ap, Address{}, 0)
			}
			f = a.editAddr.f
			t = f.curtext
		}
	}
	switch cp.cmdc {
	case '{':
		var dot Address
		mkaddr(&dot, f)
		if cp.addr != nil {
			dot = a.cmdaddress(cp.addr, dot, 0)
		}
		for c := cp.cmd; c != nil; c = c.next {
			if dot.r.q1 > t.file.b.nc() {
				// edits are deferred to elogapply, so the buffer is not shortened
				// mid-brace and dot cannot exceed it here.
				// unreachable
				a.editerror("dot extends past end of buffer during { command")
			}
			t.q0 = dot.r.q0
			t.q1 = dot.r.q1
			a.cmdexec(t, c)
		}
	default:
		if i < 0 {
			// parsecmd rejects unknown commands; only '{' reaches cmdexec with
			// i<0 and it is handled in the case above.
			// unreachable
			a.editerror("unknown command %c in cmdexec", cp.cmdc)
		}
		return cmdtab[i].fn(a, t, cp)
	}
	return 1
}

// matchfile finds the one file whose window matches regexp r (the " address)
// (ecmd.c).
func (a *Acme) matchfile(r []rune) *File {
	var found *File
	for _, c := range a.row.col {
		for _, w := range c.w {
			if w.isscratch || w.isdir {
				// -D cannot create a scratch (+Errors) or directory window, so
				// matchfile never skips one.
				// untested
				continue
			}
			t := &w.body
			// only use this window if it's the current window for the file
			if t.file.curtext != t {
				continue
			}
			if a.filematch(t.file, r) {
				if found != nil {
					a.editerror("too many files match \"%s\"", string(r))
				}
				found = t.file
			}
		}
	}
	if found == nil {
		a.editerror("no file matches \"%s\"", string(r))
	}
	return found
}

// filematch reports whether file f's window line (the same text the window list
// shows) matches regexp r (ecmd.c).
func (a *Acme) filematch(f *File, r []rune) bool {
	if !a.re.rxcompile(r) {
		a.editerror("bad regexp in file match")
	}
	w := f.curtext.w
	// same check for dirty as in winsettag, but we know ncache==0
	dirty := !w.isdir && !w.isscratch && f.mod
	dc := ' '
	if dirty {
		dc = '\''
	}
	dotc := ' '
	if a.curtext != nil && a.curtext.file == f {
		dotc = '.'
	}
	line := []rune{dc, '+', dotc, ' '}
	line = append(line, f.name...)
	line = append(line, '\n')
	var s Rangeset
	return a.re.rxexecute(nil, line, 0, len(line), &s)
}
