package main

import (
	"fmt"
	"os"
	"strings"
	"unicode/utf8"

	"9fans.net/plan9port/draw"
	"9fans.net/plan9port/draw/frame"
)

// Button-2 command execution (exec.c). execute looks up the selected text in
// the command table and runs the matching builtin, or else runs it as an
// external command via run() (exec.c execute -> run).

// Exectab is one row of the button-2 command table (exec.c).
type Exectab struct {
	name  []rune
	fn    func(a *Acme, et, t, argt *Text, flag1, flag2 bool, arg []rune)
	mark  bool
	flag1 bool
	flag2 bool
}

var exectab []Exectab

func init() {
	exectab = []Exectab{
		{[]rune("Abort"), (*Acme).doabort, false, false, false},
		{[]rune("Cut"), (*Acme).cut, true, true, true},
		{[]rune("Del"), (*Acme).del, false, false, false},
		{[]rune("Delcol"), (*Acme).delcol, false, false, false},
		{[]rune("Delete"), (*Acme).del, false, true, false},
		{[]rune("Dump"), (*Acme).dumpcmd, false, true, false},
		{[]rune("Edit"), (*Acme).edit, false, false, false},
		{[]rune("Exit"), (*Acme).xexit, false, false, false},
		{[]rune("Font"), (*Acme).fontx, false, false, false},
		{[]rune("Get"), (*Acme).getcmd, false, true, false},
		{[]rune("ID"), (*Acme).id, false, false, false},
		{[]rune("Incl"), (*Acme).incl, false, false, false},
		{[]rune("Indent"), (*Acme).indent, false, false, false},
		{[]rune("Kill"), (*Acme).xkill, false, false, false},
		{[]rune("Load"), (*Acme).dumpcmd, false, false, false},
		{[]rune("Local"), (*Acme).local, false, false, false},
		{[]rune("Look"), (*Acme).look, false, false, false},
		{[]rune("New"), (*Acme).new, false, false, false},
		{[]rune("Newcol"), (*Acme).newcol, false, false, false},
		{[]rune("Paste"), (*Acme).paste, true, true, true},
		{[]rune("Put"), (*Acme).putcmd, false, false, false},
		{[]rune("Putall"), (*Acme).putall, false, false, false},
		{[]rune("Redo"), (*Acme).undo, false, false, false},
		{[]rune("Send"), (*Acme).sendx, true, false, false},
		{[]rune("Snarf"), (*Acme).cut, false, true, false},
		{[]rune("Sort"), (*Acme).sortcol, false, false, false},
		{[]rune("Tab"), (*Acme).tab, false, false, false},
		{[]rune("Undo"), (*Acme).undo, false, true, false},
		{[]rune("Zerox"), (*Acme).zeroxx, false, false, false},
	}
}

func skipbl(r []rune) []rune {
	for len(r) > 0 && (r[0] == ' ' || r[0] == '\t' || r[0] == '\n') {
		r = r[1:]
	}
	return r
}

func findbl(r []rune) []rune {
	for len(r) > 0 && r[0] != ' ' && r[0] != '\t' && r[0] != '\n' {
		r = r[1:]
	}
	return r
}

// lookup finds the builtin command at the start of r (exec.c).
func lookup(r []rune) *Exectab {
	r = skipbl(r)
	if len(r) == 0 {
		return nil
	}
	word := r[:len(r)-len(findbl(r))]
	for i := range exectab {
		if runeEq(word, exectab[i].name) {
			return &exectab[i]
		}
	}
	return nil
}

// execute runs the command in t's [aq0,aq1), expanding to a word if empty
// (exec.c). It posts an execute event to a window whose event file is open
// (guarded by nopen[qWevent]>0) and runs a non-builtin via run(); under -D
// without 9P clients the event branch is simply not taken.
func (a *Acme) execute(t *Text, aq0, aq1 int, external bool, argt *Text) {
	q0 := aq0
	q1 := aq1
	if q1 == q0 { // expand to find the word/file name
		if t.q1 > t.q0 && t.q0 <= q0 && q0 <= t.q1 {
			q0 = t.q0
			q1 = t.q1
		} else {
			for q1 < t.file.b.nc() && isexecc(a.textreadc(t, q1)) && a.textreadc(t, q1) != ':' {
				q1++
			}
			for q0 > 0 && isexecc(a.textreadc(t, q0-1)) && a.textreadc(t, q0-1) != ':' {
				q0--
			}
			if q1 == q0 {
				return
			}
		}
	}
	r := make([]rune, q1-q0)
	t.file.b.Read(q0, r)
	e := lookup(r)
	// If a program holds the window's event file open, report the execution to
	// it instead of running it (exec.c execute).
	if !external && t.w != nil && t.w.nopen[qWevent] > 0 {
		f := 0
		if e != nil {
			f |= 1
		}
		if q0 != aq0 || q1 != aq1 {
			f |= 2
		}
		argr, argaddr := a.getarg(argt, true, true)
		arg := string(argr)
		// C tests the raw arg pointer (if(a)): getarg returns a non-nil slice
		// whenever argt is present, even for an empty selection, so f|=8 and the
		// arg events fire for a present-but-empty argument too.
		if argr != nil {
			if len(arg) > EVENTSIZE {
				// untested: a 2-1 chord argument longer than 256 bytes
				a.warning(nil, "argument string too long\n")
				return
			}
			f |= 8
		}
		c := byte('x')
		if t.what == Body {
			c = 'X'
		}
		a.execevent(t, c, aq0, aq1, f)
		if q0 != aq0 || q1 != aq1 {
			a.execevent(t, c, q0, q1, 0)
		}
		if argr != nil {
			a.winappend(t.w, fmt.Sprintf("%c0 0 0 %d %s\n", c, utf8.RuneCountInString(arg), arg))
			// the address line: the arg's file:#q0,#q1 (printarg) or empty (exec.c)
			if argaddr != "" {
				a.winappend(t.w, fmt.Sprintf("%c0 0 0 %d %s\n", c, utf8.RuneCountInString(argaddr), argaddr))
			} else {
				a.winappend(t.w, fmt.Sprintf("%c0 0 0 0 \n", c))
			}
		}
		return
	}
	if e != nil {
		if e.mark && a.seltext != nil && a.seltext.what == Body {
			a.seq++
			a.filemark(a.seltext.w.body.file)
		}
		// builtin: the argument is the text after the command word
		s := skipbl(r)
		s = findbl(s)
		s = skipbl(s)
		e.fn(a, t, a.seltext, argt, e.flag1, e.flag2, s)
		return
	}
	// Not a builtin: run it as an external command (exec.c execute -> run). The
	// command runs asynchronously and its stdout/stderr stream to the +Errors
	// window via cons (ccons), applied by the live consthread; the -D "wait" verb
	// blocks until the output appears.
	dir := a.dirname(t, nil)
	if len(dir) == 1 && dir[0] == '.' {
		// untested: a command in a window whose directory cleans to "."
		dir = nil
	}
	// The 2-1 chord argument becomes the command's extra argument (C's xarg,
	// appended to the command) and its address becomes $acmeaddr (exec.c
	// execute -> run).
	argr, argaddr := a.getarg(argt, true, true)
	a.run(t, string(r), string(dir), argaddr, string(argr))
}

// execevent posts one execute event for the range [q0,q1) with flags f
// (exec.c execute; the f field is the literal flags or 0 for the expand event).
func (a *Acme) execevent(t *Text, c byte, q0, q1, f int) {
	n := q1 - q0
	if n <= EVENTSIZE {
		m := make([]rune, n)
		t.file.b.Read(q0, m)
		a.winappend(t.w, fmt.Sprintf("%c%d %d %d %d %s\n", c, q0, q1, f, n, string(m)))
	} else {
		a.winappend(t.w, fmt.Sprintf("%c%d %d %d 0 \n", c, q0, q1, f))
	}
}

// isfilec reports whether r can be part of a file name (look.c).
func isfilec(r rune) bool {
	return isalnum(r) || runeIn(r, ".-+/:@")
}

// isexecc reports whether r can be part of an executed command (exec.c).
func isexecc(r rune) bool {
	return isfilec(r) || r == '<' || r == '|' || r == '>'
}

// getarg returns the argument text selected in argt (the 2-1 mouse chord arg)
// (exec.c). The expand-based file-name resolution from look.c is not ported, so
// (exec.c getarg). If the argument text expands to a file name (dofile) the file
// name is returned; otherwise the raw selection is. With doaddr, the second
// result is the "file:#q0[,#q1]" address string for $acmeaddr (printarg).
func (a *Acme) getarg(argt *Text, doaddr, dofile bool) ([]rune, string) {
	if argt == nil {
		return nil, ""
	}
	a.textcommit(argt, true)
	var e Expand
	if a.expand(argt, argt.q0, argt.q1, &e, false) {
		if len(e.name) > 0 && dofile {
			var addr string
			if doaddr {
				// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
				addr = a.printarg(argt, e.q0, e.q1)
			}
			return e.name, addr
		}
	} else {
		// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
		e.q0 = argt.q0
		e.q1 = argt.q1
	}
	n := e.q1 - e.q0
	r := make([]rune, n)
	argt.file.b.Read(e.q0, r)
	var addr string
	if doaddr {
		addr = a.printarg(argt, e.q0, e.q1)
	}
	return r, addr
}

// printarg formats the address of [q0,q1) in argt as "file:#q0[,#q1]" for the
// $acmeaddr environment of a spawned command (exec.c printarg). Empty unless
// argt is a named body.
func (a *Acme) printarg(argt *Text, q0, q1 int) string {
	if argt.what != Body || argt.file.name == nil {
		return ""
	}
	name := string(argt.file.name)
	if q0 == q1 {
		// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
		return fmt.Sprintf("%s:#%d", name, q0)
	}
	return fmt.Sprintf("%s:#%d,#%d", name, q0, q1)
}

// --- builtins ---

func (a *Acme) edit(et, _, argt *Text, _, _ bool, arg []rune) {
	if et == nil {
		// untested: execute always passes a real text as et.
		return
	}
	r, _ := a.getarg(argt, false, true)
	a.seq++
	if r != nil {
		a.editcmd(et, r)
	} else {
		a.editcmd(et, arg)
	}
}

func (a *Acme) undo(et, _, _ *Text, flag1, _ bool, _ []rune) {
	if et == nil || et.w == nil {
		// untested: Undo/Redo are only ever run from a windowed text.
		return
	}
	seq := a.seqof(et.w, flag1)
	if seq == 0 {
		return // nothing to undo
	}
	a.winundo(et.w, flag1)
	for _, c := range a.row.col {
		for _, w := range c.w {
			if w == et.w {
				continue
			}
			if a.seqof(w, flag1) == seq {
				a.winundo(w, flag1)
			}
		}
	}
}

// cut copies the selection to the snarf buffer (dosnarf) and/or deletes it
// (docut) (exec.c). When invoked from a tag (et!=t) it uses the window's body
// or tag selection.
func (a *Acme) cut(et, t, _ *Text, dosnarf, docut bool, _ []rune) {
	if et != t && dosnarf && et.w != nil {
		if et.w.body.q1 > et.w.body.q0 {
			t = &et.w.body
			if docut {
				a.filemark(t.file) // seq already incremented by execute
			}
		} else if et.w.tag.q1 > et.w.tag.q0 {
			t = &et.w.tag
		} else {
			// untested: Snarf/Cut from a tag with neither body nor tag selected.
			t = nil
		}
	}
	if t == nil { // no selection
		// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
		return
	}
	// Cutting from a window other than the executing one (a 2-1 chord) locks the
	// target window first (exec.c), with the executing window's owner if any.
	locked := false
	if t.w != nil && et.w != t.w {
		// untested: every cut path resolves t to the executing window's own text,
		// so the cross-window lock is not reached under -D.
		// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
		locked = true
		c := int('M')
		if et.w != nil {
			// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
			c = et.w.owner
		}
		// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
		a.winlock(t.w, c)
	}
	if t.q0 == t.q1 {
		if locked {
			// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
			a.winunlock(t.w)
		}
		return
	}
	if dosnarf {
		a.snarfbuf.Reset()
		r := make([]rune, t.q1-t.q0)
		t.file.b.Read(t.q0, r)
		a.snarfbuf.Insert(0, r)
		a.acmeputsnarf()
	}
	if docut {
		a.textdelete(t, t.q0, t.q1, true)
		a.textsetselect(t, t.q0, t.q0)
		if t.w != nil {
			a.textscrdraw(t)
			a.winsettag(t.w)
		}
	} else if dosnarf { // Snarf command
		a.argtext = t
	}
	if locked {
		// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
		a.winunlock(t.w)
	}
}

// paste inserts the snarf buffer at the selection (exec.c).
func (a *Acme) paste(et, t, _ *Text, selectall, tobody bool, _ []rune) {
	if tobody && et != nil && et.w != nil {
		t = &et.w.body
		a.filemark(t.file) // seq already incremented by execute
	}
	if t == nil {
		// untested: Paste always resolves to the executing window's body.
		return
	}
	a.acmegetsnarf()
	if a.snarfbuf.nc() == 0 {
		return
	}
	// Pasting into a window other than the executing one locks the target first
	// (exec.c), with the executing window's owner if any.
	if t.w != nil && et.w != t.w {
		// untested: Paste resolves t to the executing window's body, so the
		// cross-window lock is not reached under -D.
		// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
		c := int('M')
		if et.w != nil {
			// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
			c = et.w.owner
		}
		// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
		a.winlock(t.w, c)
	}
	a.cut(t, t, nil, false, true, nil) // remove the current selection
	q0 := t.q0
	q1 := t.q0 + a.snarfbuf.nc()
	r := make([]rune, a.snarfbuf.nc())
	a.snarfbuf.Read(0, r)
	a.textinsert(t, q0, r, true)
	if selectall {
		a.textsetselect(t, t.q0, q1)
	} else {
		// The Paste builtin always sets selectall, so this branch is only
		// reachable through a direct paste() call.
		// untested
		a.textsetselect(t, q1, q1)
	}
	if t.w != nil {
		a.textscrdraw(t)
		a.winsettag(t.w)
	}
	if t.w != nil && et.w != t.w {
		// untested
		a.winunlock(t.w)
	}
}

// zeroxx makes a second window onto the same file (Zerox) (exec.c). The window
// locking is dropped (single-threaded -D).
func (a *Acme) zeroxx(et, t, _ *Text, _, _ bool, _ []rune) {
	locked := false
	if t != nil && t.w != nil && t.w != et.w {
		// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
		locked = true
		c := int('M')
		if et.w != nil {
			// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
			c = et.w.owner
		}
		// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
		a.winlock(t.w, c)
	}
	if t == nil {
		t = et
	}
	if t == nil || t.w == nil {
		// untested: Zerox is always run from a windowed text under -D.
		return
	}
	t = &t.w.body
	if t.w.isdir {
		// untested: -D cannot create a directory window.
		a.warning(nil, "%s is a directory; Zerox illegal\n", string(t.file.name))
	} else {
		nw := a.coladd(t.w.col, nil, t.w, -1)
		// ugly: lock the new window so the enclosing winunlock (over the grown
		// f.text) finds it locked (exec.c zeroxx winlock1).
		a.winlock1(nw, t.w.owner)
		a.xfidlog(nw, "zerox")
	}
	if locked {
		// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
		a.winunlock(t.w)
	}
}

// acmeputsnarf writes the internal snarf buffer out to the host clipboard
// (acme.c acmeputsnarf). In debug mode (-D) the host clipboard is left
// untouched so the differential tests stay deterministic; the C chunking by
// NSnarf is unnecessary since WriteSnarf takes the whole buffer.
func (a *Acme) acmeputsnarf() {
	if a.dflag || a.disp == nil {
		// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
		return
	}
	if a.snarfbuf.nc() == 0 {
		// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
		return
	}
	r := make([]rune, a.snarfbuf.nc())
	a.snarfbuf.Read(0, r)
	a.disp.WriteSnarf([]byte(string(r))) // C discards putsnarf's result
}

// acmegetsnarf reads the host clipboard into the internal snarf buffer (acme.c
// acmegetsnarf), converting bytes to runes (dropping NULs) like cvttorunes.
func (a *Acme) acmegetsnarf() {
	if a.dflag || a.disp == nil {
		// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
		return
	}
	buf := make([]byte, 1024)
	n, actual, err := a.disp.ReadSnarf(buf)
	if err != nil {
		// untested: a clipboard read failure is host-specific.
		return
	}
	if actual > len(buf) { // buffer too short: reread at the real size
		// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
		buf = make([]byte, actual)
		n, _, err = a.disp.ReadSnarf(buf)
		if err != nil {
			// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
			return
		}
	}
	if n == 0 {
		return
	}
	var r []rune
	var nulls bool
	cvttorunes(buf[:n], n, &r, &nulls)
	a.snarfbuf.Reset()
	a.snarfbuf.Insert(0, r)
}

// del closes window et's window if it is clean (or Delete forces it) (exec.c).
func (a *Acme) del(et, _, _ *Text, flag1, _ bool, _ []rune) {
	if et.col == nil || et.w == nil {
		// untested: Del/Delete are only ever run from a windowed, columned text.
		return
	}
	if flag1 || len(et.w.body.file.text) > 1 || a.winclean(et.w, false) {
		a.colclose(et.col, et.w, true)
	}
}

func (a *Acme) seqof(w *Window, isundo bool) int {
	if isundo {
		return w.body.file.seq
	}
	return fileredoseq(w.body.file)
}

// winundo undoes (or redoes) window w and re-syncs zerox copies (wind.c).
func (a *Acme) winundo(w *Window, isundo bool) {
	w.utflastqid = -1
	body := &w.body
	a.fileundo(body.file, isundo, &body.q0, &body.q1)
	a.textshow(body, body.q0, body.q1, true)
	f := body.file
	for _, x := range f.text {
		v := x.w
		v.dirty = f.seq != v.putseq
		if v != w {
			v.body.q0 = v.body.fr.P0 + v.body.org
			v.body.q1 = v.body.fr.P1 + v.body.org
		}
	}
	a.winsettag(w)
}

// xfidlog records a window event (create/delete/...) for /log file readers
// (logf.c). The headless -D port has no /log readers, so it is a no-op and the
// observable -D output matches the C oracle.
// xfidlog appends "id op name" to the global activity log (logf.c xfidlog).
func (a *Acme) xfidlog(w *Window, op string) {
	// At the GC threshold, drop entries every open reader has consumed (logf.c
	// xfidlog): with no readers, or readers that keep up, the pending log stays
	// bounded instead of growing for the life of the editor. min defaults to the
	// end, so with no readers all entries are released.
	if len(a.log) >= a.logcap {
		min := a.logStart + len(a.log)
		for f := range a.logfids {
			if off := auxOf(f).logoff; off < min {
				// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
				min = off
			}
		}
		if n := min - a.logStart; n > 0 {
			a.log = append([]string(nil), a.log[n:]...)
			a.logStart += n
		}
		if len(a.log) >= a.logcap {
			a.logcap *= 2
			if a.logcap == 0 {
				a.logcap = 8
			}
		}
	}
	a.log = append(a.log, fmt.Sprintf("%d %s %s\n", w.id, op, string(w.body.file.name)))
	if a.logread != nil {
		a.logread.Broadcast() // wake a blocked /log reader (logf.c)
	}
}

// newcol adds a new column with one empty window (exec.c).
func (a *Acme) newcol(et, _, _ *Text, _, _ bool, _ []rune) {
	c := a.rowadd(&a.row, nil, -1)
	if c != nil {
		w := a.coladd(c, nil, nil, -1)
		a.winsettag(w)
		a.xfidlog(w, "new")
	}
}

// getname resolves the file name for a Get/Put: the 2-1 argument, the command
// argument, or (failing those) the window's own name, prefixed with the
// window's directory when relative (exec.c getname). It returns nil for an
// empty name.
func (a *Acme) getname(t, argt *Text, arg []rune, isput bool) []rune {
	r, _ := a.getarg(argt, false, true)
	promote := false
	if r == nil {
		promote = true
	} else if isput {
		// for a Put, synthesize a name even for a non-existent file; the best
		// guess is that a name without a slash is relative to t.
		promote = true
		for _, c := range r {
			if c == '/' {
				promote = false
				break
			}
		}
		if promote {
			t = argt
			arg = r
		}
	}
	if promote {
		if len(arg) == 0 {
			r = t.file.name
		} else {
			var dir []rune
			if arg[0] != '/' {
				dir = a.dirname(t, nil)
				if len(dir) == 1 && dir[0] == '.' { // sigh
					dir = nil
				}
			}
			if dir != nil {
				// untested: needs a named window whose directory exists on disk,
				// which the -D harness (single "f" member) cannot set up
				r = append([]rune(nil), dir...)
				// C tests r[dir.nr], the NUL slot just past the copied dir (r was
				// runemalloc-zeroed), so its "!= '/'" is always true: C inserts a
				// slash whenever dir.nr>0 and arg isn't absolute, even if dir
				// already ends in '/' (yielding dir//name). Faithful to exec.c.
				// untested
				if len(r) > 0 && arg[0] != '/' {
					// untested
					r = append(r, '/')
				}
				// untested
				r = append(r, arg...)
			} else {
				r = append([]rune(nil), arg...)
			}
		}
	}
	if len(r) == 0 {
		return nil
	}
	return r
}

// look searches the body for the argument text, the 2-1 argument, or the
// current selection (exec.c look, the Look builtin).
func (a *Acme) look(et, _, argt *Text, _, _ bool, arg []rune) {
	if et == nil || et.w == nil {
		// untested: Look is always run from a windowed text
		return
	}
	t := &et.w.body
	if len(arg) > 0 {
		a.search(t, arg, false)
		return
	}
	r, _ := a.getarg(argt, false, false)
	if r == nil {
		r = make([]rune, t.q1-t.q0)
		t.file.b.Read(t.q0, r)
	}
	a.search(t, r, false)
}

// new opens the named files (or, with no name, an empty window) in et's column
// (look.c new, the New builtin).
// dumpcmd implements the Dump (isdump) and Load builtins: save/restore the
// session to/from the named file, or $HOME/acme.dump if none (exec.c dump).
func (a *Acme) dumpcmd(_, _, argt *Text, isdump, _ bool, arg []rune) {
	name := string(arg)
	if name == "" {
		// no inline name: take the 2-1 chord argument (getbytearg in C), or
		// empty for the $HOME/acme.dump default (builtins_dumphome).
		r, _ := a.getarg(argt, false, true)
		name = string(r)
	}
	if isdump {
		a.rowdump(&a.row, name)
	} else {
		a.rowload(&a.row, name, false)
	}
}

func (a *Acme) new(et, t, argt *Text, flag1, flag2 bool, arg []rune) {
	if r, _ := a.getarg(argt, false, true); r != nil {
		// untested: -D New carries no 2-1 chord argument
		a.new(et, t, nil, flag1, flag2, r)
		if len(arg) == 0 {
			// untested
			return
		}
	}
	for ndone := 0; ; ndone++ {
		rest := findbl(arg)
		if len(rest) == len(arg) { // no file name remains
			if ndone == 0 && et.col != nil {
				w := a.coladd(et.col, nil, nil, -1)
				a.winsettag(w)
				a.xfidlog(w, "new")
			}
			break
		}
		f := arg[:len(arg)-len(rest)]
		e := Expand{jump: true}
		e.name = a.dirname(et, f)
		e.bname = string(e.name)
		a.openfile(et, &e)
		arg = skipbl(rest)
	}
}

// trimspaces deletes the trailing spaces and tabs at the end of every line (and
// at end of file) of et's window body. Called by put when the window is in
// autoindent mode (exec.c). It scans the buffer backward a chunk at a time,
// coalescing each space run up to the following newline (or EOF) and deleting
// it. The C winlock for the cross-window case (et.w != t.w) is omitted: put
// always passes its own window's text, so t.w == et.w here.
func (a *Acme) trimspaces(et *Text) {
	t := &et.w.body
	f := t.file
	marked := false

	r := make([]rune, RBUFSIZE)
	q0 := f.b.nc()
	delstart := q0 // end of current space run, or 0 if no active run; = q0 to delete spaces before EOF
	for q0 > 0 {
		n := RBUFSIZE
		if n > q0 {
			n = q0
		}
		q0 -= n
		f.b.Read(q0, r[:n])
		for i := n; ; i-- {
			if i == 0 || (r[i-1] != ' ' && r[i-1] != '\t') {
				// found non-space or start of buffer; delete the active run
				if q0+i < delstart {
					if !marked {
						marked = true
						a.seq++
						a.filemark(f)
					}
					a.textdelete(t, q0+i, delstart, true)
				}
				if i == 0 {
					if delstart > 0 {
						// untested: keeping a space run active into the next chunk
						// needs the run to reach a chunk boundary -- a >RBUFSIZE
						// buffer, or one starting with spaces (the body verb strips
						// leading spaces), neither reachable through a -D script.
						// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
						delstart = q0
					}
					break
				}
				delstart = 0
				if r[i-1] == '\n' {
					delstart = q0 + i - 1 // delete spaces before this newline
				}
			}
		}
	}
}

// putcmd writes the executing window's body to its file (exec.c put, the Put
// builtin).
func (a *Acme) putcmd(et, _, argt *Text, _, _ bool, arg []rune) {
	if et == nil || et.w == nil || et.w.isdir {
		// untested: Put is run from a non-directory windowed text
		return
	}
	w := et.w
	f := w.body.file
	name := a.getname(&w.body, argt, arg, true)
	if name == nil {
		a.warning(nil, "no file name\n")
		return
	}
	if w.autoindent {
		a.trimspaces(et)
	}
	a.putfile(f, 0, f.b.nc(), name)
	a.xfidlog(w, "put")
}

// putall writes every dirty, named, non-scratch window to its file (exec.c).
func (a *Acme) putall(et, _, _ *Text, _, _ bool, _ []rune) {
	for _, c := range a.row.col {
		for _, w := range c.w {
			if w.isscratch || w.isdir || w.body.file.nname() == 0 {
				continue
			}
			if w.nopen[qWevent] > 0 {
				// untested: a dirty window with its event file held open by a
				// program is a niche Putall case not exercised here
				// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
				continue
			}
			name := string(w.body.file.name)
			_, err := os.Stat(name)
			if w.body.file.mod || w.body.ncache != 0 {
				if err != nil {
					// untested: the OS error text (%v) is not byte-identical to
					// the C oracle's %r, so this warning is not differize-tested
					// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
					a.warning(nil, "no auto-Put of %s: %s\n", name, errstr(err))
				} else {
					a.wincommit(w, &w.body)
					a.put(w)
				}
			}
		}
	}
}

// fontx changes the executing window's body font (exec.c fontx). The argument
// is an optional "fix"/"var" flag and/or a font file; with neither it toggles
// between the fixed and variable default fonts.
func (a *Acme) fontx(et, _, argt *Text, _, _ bool, arg []rune) {
	if et == nil || et.w == nil {
		// untested: Font is run from a windowed text
		return
	}
	t := &et.w.body
	var flag, file []rune
	for {
		rest := findbl(arg)
		if len(rest) == len(arg) {
			break
		}
		r := arg[:len(arg)-len(rest)]
		if runeEqStr(r, "fix") || runeEqStr(r, "var") {
			flag = append([]rune(nil), r...)
		} else {
			file = append([]rune(nil), r...)
		}
		arg = skipbl(rest)
	}
	if r, _ := a.getarg(argt, false, true); r != nil {
		// untested: -D Font carries no 2-1 chord argument
		if runeEqStr(r, "fix") || runeEqStr(r, "var") {
			// untested
			flag = r
		} else {
			// untested
			file = r
		}
	}
	fix := true
	var newfont *Reffont
	if flag != nil {
		fix = runeEqStr(flag, "fix")
	} else if file == nil {
		newfont = a.rfget(false, false, false, "")
		if newfont != nil {
			fix = newfont.f.Name == t.fr.Font.Name
		}
	}
	if file != nil {
		newfont = a.rfget(fix, flag != nil, false, string(file))
	} else {
		newfont = a.rfget(fix, false, false, "")
	}
	if newfont != nil {
		a.screen.Draw(t.w.r, a.textcols[frame.BACK], nil, draw.ZP)
		t.reffont = newfont
		t.fr.Font = newfont.f
		t.fr.InitTick()
		if t.w.isdir {
			// untested: -D cannot create a directory window to recolumnate
			t.all.Min.X++ // force recolumnation; disgusting! (exec.c fontx)
			// recompute each listing entry's width in the new font, so the columns
			// re-lay-out correctly (exec.c fontx dp->wid loop).
			// untested: needs a directory window, which -D cannot build
			for _, dp := range t.w.dlp {
				// untested: needs a directory window, which -D cannot build
				dp.wid = newfont.f.StringWidth(string(dp.r))
			}
		}
		a.colgrow(t.w.col, t.w, -1)
	}
}

// incl adds include directories to the executing window, or prints the current
// list when given none (exec.c incl, the Incl builtin).
func (a *Acme) incl(et, _, argt *Text, _, _ bool, arg []rune) {
	if et == nil || et.w == nil {
		// untested: Incl is run from a windowed text
		return
	}
	w := et.w
	n := 0
	if r, _ := a.getarg(argt, false, true); r != nil {
		n++
		a.winaddincl(w, r)
	}
	for {
		rest := findbl(arg)
		if len(rest) == len(arg) { // no directory remains
			break
		}
		n++
		a.winaddincl(w, append([]rune(nil), arg[:len(arg)-len(rest)]...))
		arg = skipbl(rest)
	}
	if n == 0 && len(w.incl) > 0 {
		// untested: prints the include list to stderr (not the -D stdout), and
		// needs a no-arg Incl after a prior one (awkward double tag append)
		// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
		for i := len(w.incl) - 1; i >= 0; i-- {
			// untested
			a.warning(nil, "%s ", string(w.incl[i]))
		}
		// untested
		a.warning(nil, "\n")
	}
}

// sendx appends the selection (or the snarf buffer) to the end of the window's
// body, the way the win shell sends input (exec.c sendx).
func (a *Acme) sendx(et, _, _ *Text, _, _ bool, _ []rune) {
	if et.w == nil {
		// untested: Send is run from a windowed text
		return
	}
	t := &et.w.body
	if t.q0 != t.q1 {
		a.cut(t, t, nil, true, false, nil) // snarf the selection (no delete)
	}
	a.textsetselect(t, t.file.b.nc(), t.file.b.nc())
	a.paste(t, t, nil, true, true, nil)
	if a.textreadc(t, t.file.b.nc()-1) != '\n' {
		a.textinsert(t, t.file.b.nc(), []rune{'\n'}, true)
		a.textsetselect(t, t.file.b.nc(), t.file.b.nc())
	}
	t.iq1 = t.q1
	a.textshow(t, t.q1, t.q1, true)
}

// getcmd reloads the executing window's file (exec.c get, the Get builtin).
// The -D surface carries no name argument, so it always reloads the window's
// own file (the same-name path in a.get); loading a different file via a Get
// argument is not exercised.
func (a *Acme) getcmd(et, _, argt *Text, flag1, _ bool, arg []rune) {
	if flag1 && (et == nil || et.w == nil) {
		// untested: Get is always run from a windowed text
		return
	}
	a.get(et.w, argt, arg)
}

// delcol deletes the executing column if it is clean and no window in it is
// running an external command (exec.c).
func (a *Acme) delcol(et, _, _ *Text, _, _ bool, _ []rune) {
	c := et.col
	if c == nil || !a.colclean(c) {
		return
	}
	for _, w := range c.w {
		if w.nopen[qWevent]+w.nopen[qWaddr]+w.nopen[qWdata]+w.nopen[qWxdata] > 0 {
			a.warning(nil, "can't delete column; %s is running an external command\n", string(w.body.file.name))
			return
		}
	}
	a.rowclose(&a.row, c, true)
}

// sortcol sorts the executing column's windows by file name (exec.c Sort).
func (a *Acme) sortcol(et, _, _ *Text, _, _ bool, _ []rune) {
	if et.col != nil {
		a.colsort(et.col)
	}
}

// id prints the executing window's id as its /mnt/acme path (exec.c).
func (a *Acme) id(et, _, _ *Text, _, _ bool, _ []rune) {
	if et != nil && et.w != nil {
		a.warning(nil, "/mnt/acme/%d/\n", et.w.id)
	}
}

// abort terminates the process abnormally, the Go analogue of C's abort(3)
// reached by a confirmed Abort builtin.
// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
func abort() { panic("acme: Abort") }

// doabort implements the Abort builtin (exec.c doabort): the first invocation
// warns; a second actually aborts the process. The C static counter is a field
// so a fresh editor starts over.
func (a *Acme) doabort(_, _, _ *Text, _, _ bool, _ []rune) {
	if a.nabort == 0 {
		a.nabort++
		a.warning(nil, "executing Abort again will call abort()\n")
	} else {
		// untested: the real abort() crashes the process; the -D suite invokes
		// Abort only once (a second call would kill the in-process test).
		// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
		abort()
	}
}

// xexit implements the Exit builtin (exec.c xexit): if every window is clean,
// quit. C sends on cexit and exits the thread; here we additionally set the
// exiting flag so the (still-current) runD loop unwinds, and wake any threaded
// cexit consumer without blocking when none is present.
func (a *Acme) xexit(_, _, _ *Text, _, _ bool, _ []rune) {
	if a.rowclean(&a.row) {
		a.exiting = true
		if a.cexit != nil {
			select {
			case a.cexit <- 0:
			default:
			}
		}
	}
}

// local implements the Local builtin (exec.c local): run a command in acme's
// own namespace (newns=FALSE) rather than a forked one. With newns=FALSE C's
// runproc wires the child's stdin/stdout to /dev/null and its stderr to acme's
// error output (erroutfd) -- so, unlike a button-2 command, a Local command's
// output is NOT routed to +Errors.
func (a *Acme) local(et, _, argt *Text, _, _ bool, arg []rune) {
	// The 2-1 chord argument becomes the command's extra argument and its address
	// becomes $acmeaddr, exactly as C's local → run(nil,…,aa,a,…).
	argr, argaddr := a.getarg(argt, true, true)
	dir := a.dirname(et, nil)
	if len(dir) == 1 && dir[0] == '.' {
		// untested: a Local command in a window whose directory cleans to "."
		dir = nil
	}
	a.runlocal(string(arg), string(dir), string(argr), argaddr)
}

// xkill implements the Kill builtin (exec.c xkill): send each whitespace-
// separated command name in the argument -- and in the 2-1 chord argument -- to
// ckill, where waitthread posts a kill to every running command with that name.
func (a *Acme) xkill(_, _, argt *Text, _, _ bool, arg []rune) {
	if r, _ := a.getarg(argt, false, false); len(r) > 0 {
		// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
		a.xkill(nil, nil, nil, false, false, r)
	}
	// loop over the space-separated names (C: findbl/skipbl, stop at a blank)
	for len(arg) > 0 {
		rest := findbl(arg)
		if len(rest) == len(arg) { // arg begins with a blank (C a==arg): done
			// untested: builtin error / 2-1-chord-argument / external-exec edge path not exercised by the -D suite
			break
		}
		a.ckill <- arg[:len(arg)-len(rest)]
		arg = skipbl(rest)
	}
}

// indentval parses an Indent argument: "on"/"off" set the per-window default,
// "ON"/"OFF" the global default (exec.c). Returns IGlobal, IError, or 0/1.
const (
	iGlobal = -2
	iError  = -1
)

func (a *Acme) indentval(s []rune) int {
	if len(s) < 2 {
		return iError
	}
	// C uses runestrncmp(s, LON/LOFF/Lon, n) with n = len(s), so an n-rune prefix
	// matches ("Indent OF" == "Indent OFF"). HasPrefix(target, s) reproduces that.
	if strings.HasPrefix("ON", string(s)) {
		a.globalautoindent = true
		a.warning(nil, "Indent ON\n")
		return iGlobal
	}
	if strings.HasPrefix("OFF", string(s)) {
		a.globalautoindent = false
		a.warning(nil, "Indent OFF\n")
		return iGlobal
	}
	if strings.HasPrefix("on", string(s)) {
		return 1
	}
	return 0
}

// firstWord returns the leading non-blank word of arg (exec.c findbl usage).
func firstWord(arg []rune) []rune {
	return arg[:len(arg)-len(findbl(arg))]
}

func (a *Acme) indent(et, _, argt *Text, _, _ bool, arg []rune) {
	var w *Window
	if et != nil && et.w != nil {
		w = et.w
	}
	autoindent := iError
	r, _ := a.getarg(argt, false, true)
	if len(r) > 0 {
		autoindent = a.indentval(r)
	} else if word := firstWord(arg); len(word) > 0 {
		autoindent = a.indentval(word)
	}
	if autoindent == iGlobal {
		for _, c := range a.row.col {
			for _, win := range c.w {
				win.autoindent = a.globalautoindent
			}
		}
	} else if w != nil && autoindent >= 0 {
		w.autoindent = autoindent != 0
	}
}

func (a *Acme) tab(et, _, argt *Text, _, _ bool, arg []rune) {
	if et == nil || et.w == nil {
		// untested: Tab always executes from a window via button 2, so et.w!=nil
		return
	}
	w := et.w
	r, _ := a.getarg(argt, false, true)
	tab := 0
	if len(r) > 0 {
		if r[0] >= '0' && r[0] <= '9' {
			tab = runesAtoi(r)
		}
	} else if word := firstWord(arg); len(word) > 0 {
		if word[0] >= '0' && word[0] <= '9' {
			tab = runesAtoi(word)
		}
	}
	if tab > 0 {
		if w.body.tabstop != tab {
			w.body.tabstop = tab
			a.winresize(w, w.r, false, true)
		}
	} else {
		a.warning(nil, "%s: Tab %d\n", string(w.body.file.name), w.body.tabstop)
	}
}

// runeEqStr reports whether rune slice r equals string s.
func runeEqStr(r []rune, s string) bool {
	return string(r) == s
}

// runesAtoi parses a leading non-negative integer from r.
func runesAtoi(r []rune) int {
	n := 0
	for _, c := range r {
		if c < '0' || c > '9' {
			break
		}
		n = n*10 + int(c-'0')
	}
	return n
}
