package main

// The edit-command language (sam-style): parser (edit.c) plus command
// implementations (ecmd.c). The C version tracks every allocated Cmd/Addr/
// String in Lists so freecmd can release them; Go's GC makes that machinery
// unnecessary, so it is dropped. Edit-language "String"s become plain []rune.

// Addr is one address element (edit.h Addr). typ is one of
// '#','l','/','?','.','$','+','-',',',';'. re holds a regexp; left/next link
// the two sides of , and ;.
type Addr struct {
	typ  byte
	re   []rune // for / and ?
	left *Addr  // left side of , and ;
	num  int
	next *Addr // or right side of , and ;
}

// Address is an evaluated address: a range within a file (edit.h Address).
type Address struct {
	r Range
	f *File
}

// Cmd is a parsed command (edit.h Cmd).
type Cmd struct {
	addr   *Addr  // address (range of text)
	re     []rune // regexp for e.g. 'x'
	cmd    *Cmd   // target of x, g, {, etc.
	text   []rune // text of a, c, i; rhs of s
	mtaddr *Addr  // address for m, t
	next   *Cmd   // next element in {}
	num    int
	flag   int
	cmdc   rune // command character
}

// default addresses (edit.h Defaddr)
const (
	aNo = iota
	aDot
	aAll
)

// cmdEntry is one row of the command table (edit.h cmdtab).
type cmdEntry struct {
	cmdc    rune
	text    bool   // takes a textual argument?
	regexp  bool   // takes a regular expression?
	addr    bool   // takes an address (m or t)?
	defcmd  rune   // default command; 0==none
	defaddr int    // default address
	count   int    // takes a count, e.g. s2///
	token   string // takes text terminated by one of these
	fn      func(*Acme, *Text, *Cmd) int
}

const (
	tokWord = " \t\n" // wordx
	tokLine = "\n"    // linex
)

// cmdtab is the edit-command table (edit.c). The fn fields reference methods
// that transitively reference cmdtab (via cmdexec), so it is filled in init()
// to avoid an initialization cycle (as in the sam port).
var cmdtab []cmdEntry

func init() {
	cmdtab = []cmdEntry{
		{'\n', false, false, false, 0, aDot, 0, "", (*Acme).nl_cmd},
		{'a', true, false, false, 0, aDot, 0, "", (*Acme).a_cmd},
		{'c', true, false, false, 0, aDot, 0, "", (*Acme).c_cmd},
		{'d', false, false, false, 0, aDot, 0, "", (*Acme).d_cmd},
		{'i', true, false, false, 0, aDot, 0, "", (*Acme).i_cmd},
		{'p', false, false, false, 0, aDot, 0, "", (*Acme).p_cmd},
		{'s', false, true, false, 0, aDot, 1, "", (*Acme).s_cmd},
		{'b', false, false, false, 0, aNo, 0, tokLine, (*Acme).b_cmd},
		{'e', false, false, false, 0, aNo, 0, tokWord, (*Acme).e_cmd},
		{'f', false, false, false, 0, aNo, 0, tokWord, (*Acme).f_cmd},
		{'g', false, true, false, 'p', aDot, 0, "", (*Acme).g_cmd},
		{'v', false, true, false, 'p', aDot, 0, "", (*Acme).g_cmd},
		{'m', false, false, true, 0, aDot, 0, "", (*Acme).m_cmd},
		{'t', false, false, true, 0, aDot, 0, "", (*Acme).m_cmd},
		{'r', false, false, false, 0, aDot, 0, tokWord, (*Acme).e_cmd},
		{'u', false, false, false, 0, aNo, 2, "", (*Acme).u_cmd},
		{'x', false, true, false, 'p', aDot, 0, "", (*Acme).x_cmd},
		{'y', false, true, false, 'p', aDot, 0, "", (*Acme).x_cmd},
		{'w', false, false, false, 0, aAll, 0, tokWord, (*Acme).w_cmd},
		{'=', false, false, false, 0, aDot, 0, tokLine, (*Acme).eq_cmd},
		{'B', false, false, false, 0, aNo, 0, tokLine, (*Acme).B_cmd},
		{'D', false, false, false, 0, aNo, 0, tokLine, (*Acme).D_cmd},
		{'X', false, true, false, 'f', aNo, 0, "", (*Acme).X_cmd},
		{'Y', false, true, false, 'f', aNo, 0, "", (*Acme).X_cmd},
		{'<', false, false, false, 0, aDot, 0, tokLine, (*Acme).pipe_cmd},
		{'|', false, false, false, 0, aDot, 0, tokLine, (*Acme).pipe_cmd},
		{'>', false, false, false, 0, aDot, 0, tokLine, (*Acme).pipe_cmd},
	}
}

func cmdlookup(c rune) int {
	for i := range cmdtab {
		if cmdtab[i].cmdc == c {
			return i
		}
	}
	return -1
}

// resetxec resets per-command execution state (edit.c resetxec): the X/Y
// nesting guard, the loop flag, and the pending <-collection. Resetting
// glooping/collection here (not just nest) keeps a prior command aborted via
// editerror from leaving glooping stuck non-zero (a wrong "can't nest").
func (a *Acme) resetxec() {
	a.glooping = 0
	a.nest = 0
	a.collection = nil
}

// editcmd parses and runs the edit-command string r against text ct, then
// applies the accumulated log (edit.c editcmd, without the helper thread).
func (a *Acme) editcmd(ct *Text, r []rune) {
	if len(r) == 0 {
		return
	}
	if 2*len(r) > RBUFSIZE {
		// untested: -D never issues an edit command longer than RBUFSIZE/2 (~4K)
		a.warning(nil, "string too long\n")
		return
	}
	// edit commands operate on the body, even when invoked from the tag.
	if ct.w != nil {
		a.curtext = &ct.w.body
	} else {
		// untested: every -D editcmd runs on a windowed text.
		a.curtext = nil
	}
	target := ct
	if a.curtext != nil {
		target = a.curtext
	}
	a.alleditinit()
	eloginit(target.file)
	a.cmdbuf = append([]rune(nil), r...)
	if a.cmdbuf[len(a.cmdbuf)-1] != '\n' {
		a.cmdbuf = append(a.cmdbuf, '\n')
	}
	a.cmdp = 0
	a.resetxec()
	defer func() {
		if e := recover(); e != nil {
			ee, ok := e.(editErr)
			if !ok {
				// re-raises a non-edit-language panic; the well-formed command
				// stream never produces one.
				// unreachable
				panic(e)
			}
			// Surface the message in the +Errors window, as edit.c's editcmd
			// does on receiving from editerrc.
			if ee != "" {
				a.warning(nil, "Edit: %s\n", string(ee))
			}
			// On error, discard every window's pending edit log so a
			// partially-built command applies nothing (edit.c editerror's
			// allwindows(allelogterm), run before the error is reported).
			a.allediterror(target)
		}
		// edit.c's editcmd calls allwindows(allupdate) once the edit thread
		// returns, error or not: on success it applies the logs; on error the
		// logs were just discarded, so this only redraws the selection/tag.
		a.editupdate(target)
	}()
	for {
		cp := a.parsecmd(0)
		if cp == nil {
			break
		}
		if a.cmdexec(a.curtext, cp) == 0 {
			// only g_cmd's internal addr-mismatch returns 0, and the top-level
			// command's address always matches its own file.
			// unreachable
			break
		}
	}
}

// alleditinit commits every window's tag and body and clears editclean before an
// edit runs, so the command operates on committed text and a clean flag left over
// from a previous edit's whole-file reload (the `e` command's editclean=true)
// does not wrongly survive into this edit's editupdate (edit.c allwindows(alleditinit)).
func (a *Acme) alleditinit() {
	for _, c := range a.row.col {
		for _, w := range c.w {
			a.textcommit(&w.tag, true)
			a.textcommit(&w.body, true)
			w.body.file.editclean = false
		}
	}
}

// allediterror discards every window's pending edit log after an edit error, so
// a command that failed partway applies nothing (edit.c editerror's
// allwindows(allelogterm) -> elogterm). Mirror editupdate's window walk.
func (a *Acme) allediterror(target *Text) {
	if target.w == nil || target.w.col == nil {
		// untested: headless unit-test edit (no laid-out window)
		a.elogterm(target.file)
		return
	}
	for _, c := range a.row.col {
		for _, w := range c.w {
			a.elogterm(w.body.file)
		}
	}
}

// editupdate applies each window's pending edit log and refreshes its tag
// (edit.c allwindows(allupdate)). Every laid-out window is refreshed -- not just
// the ones this command edited -- so e.g. another window's modified button is
// redrawn to reflect a change made before the command.
func (a *Acme) editupdate(target *Text) {
	if target.w == nil || target.w.col == nil {
		// untested: headless unit-test edit (no laid-out window)
		a.elogapply(target.file)
		return
	}
	for _, c := range a.row.col {
		for _, w := range c.w {
			t := &w.body
			f := t.file
			if f.curtext != t { // do curtext only
				continue
			}
			if f.elog.typ == Null {
				a.elogterm(f)
			} else if f.elog.typ != Empty {
				a.elogapply(f)
				if f.editclean {
					f.mod = false
					for _, u := range f.text {
						u.w.dirty = false
					}
				}
			}
			a.textsetselect(t, t.q0, t.q1)
			a.textscrdraw(t)
			a.winsettag(w)
		}
	}
}

// parsecmd parses one command (and its address, argument, and nested commands)
// from the stream (edit.c).
func (a *Acme) parsecmd(nest int) *Cmd {
	cmd := new(Cmd)
	cmd.addr = a.compoundaddr()
	if a.cmdskipbl() == -1 {
		return nil
	}
	c := a.getch()
	if c == -1 {
		// cmdskipbl above already returns -1 at end of input and ungets any
		// non-blank it peeked, so this getch never sees EOF.
		// unreachable
		return nil
	}
	cmd.cmdc = rune(c)
	if cmd.cmdc == 'c' && a.nextc() == 'd' { // sleazy two-character case (cd)
		a.getch()
		cmd.cmdc = 'c' | 0x100
	}
	i := cmdlookup(cmd.cmdc)
	if i >= 0 {
		if cmd.cmdc == '\n' {
			return cmd // let nl_cmd work it all out
		}
		ct := &cmdtab[i]
		if ct.defaddr == aNo && cmd.addr != nil {
			a.editerror("command takes no address")
		}
		if ct.count != 0 {
			cmd.num = a.getnum(ct.count)
		}
		if ct.regexp {
			// x without pattern -> .*\n (cmd.re==nil); X without pattern is all files
			if (ct.cmdc != 'x' && ct.cmdc != 'X') ||
				func() bool { c := a.nextc(); return c != ' ' && c != '\t' && c != '\n' }() {
				a.cmdskipbl()
				c := a.getch()
				if c == '\n' || c < 0 {
					a.editerror("no address")
				}
				a.okdelim(c)
				cmd.re = a.getregexp(c)
				if ct.cmdc == 's' {
					cmd.text = a.getrhs(c, 's')
					if a.nextc() == c {
						a.getch()
						if a.nextc() == 'g' {
							cmd.flag = a.getch()
						}
					}
				}
			}
		}
		if ct.addr {
			if cmd.mtaddr = a.simpleaddr(); cmd.mtaddr == nil {
				a.editerror("bad address")
			}
		}
		switch {
		case ct.defcmd != 0:
			if a.cmdskipbl() == '\n' {
				a.getch()
				cmd.cmd = new(Cmd)
				cmd.cmd.cmdc = ct.defcmd
			} else if cmd.cmd = a.parsecmd(nest); cmd.cmd == nil {
				// unreachable
				bug("defcmd")
			}
		case ct.text:
			cmd.text = a.collecttext()
		case ct.token != "":
			cmd.text = a.collecttoken(ct.token)
		default:
			a.atnl()
		}
		return cmd
	}
	switch cmd.cmdc {
	case '{':
		var cp *Cmd
		for {
			if a.cmdskipbl() == '\n' {
				a.getch()
			}
			ncp := a.parsecmd(nest + 1)
			if cp != nil {
				cp.next = ncp
			} else {
				cmd.cmd = ncp
			}
			cp = ncp
			if cp == nil {
				break
			}
		}
	case '}':
		a.atnl()
		if nest == 0 {
			a.editerror("right brace with no left brace")
		}
		return nil
	default:
		// %c truncates to a byte in C's print; the sleazy "cd" case sets cmdc to
		// 'c'|0x100, so mask to match (0x163 -> 'c'), not Go's full-rune 'ţ'.
		a.editerror("unknown command %c", rune(byte(cmd.cmdc)))
	}
	return cmd
}

// collecttext collects the text argument of a/c/i: either dot-terminated lines
// after a newline, or a delimited string (edit.c).
func (a *Acme) collecttext() []rune {
	var s []rune
	if a.cmdskipbl() == '\n' {
		a.getch()
		for {
			begline := len(s)
			var c int
			for {
				c = a.getch()
				if c <= 0 || c == '\n' {
					break
				}
				s = append(s, rune(c))
			}
			s = append(s, '\n') // the '\n' is appended even at end-of-input
			if c < 0 {
				return s
			}
			if len(s)-begline == 2 && s[begline] == '.' {
				// the ".\n" terminator: drop it
				s = s[:len(s)-2]
				break
			}
		}
	} else {
		delim := a.getch()
		a.okdelim(delim)
		s = a.getrhs(delim, 'a')
		if a.nextc() == delim {
			a.getch()
		}
		a.atnl()
	}
	return s
}

// collecttoken collects text terminated by one of end's characters (edit.c).
func (a *Acme) collecttoken(end string) []rune {
	var s []rune
	for a.nextc() == ' ' || a.nextc() == '\t' {
		s = append(s, rune(a.getch())) // blanks significant for getname()
	}
	for {
		c := a.getch()
		if c <= 0 || runeIn(rune(c), end) {
			if c != '\n' {
				a.atnl()
			}
			break
		}
		s = append(s, rune(c))
	}
	return s
}

// --- lexer over the edit-command stream (edit.c) ---

func (a *Acme) getch() int {
	if a.cmdp >= len(a.cmdbuf) {
		return -1
	}
	c := a.cmdbuf[a.cmdp]
	a.cmdp++
	return int(c)
}

func (a *Acme) nextc() int {
	if a.cmdp >= len(a.cmdbuf) {
		// Every nextc peek happens mid-parse, before the trailing newline that
		// editcmd appends is consumed, so the buffer is never empty here.
		// unreachable
		return -1
	}
	return int(a.cmdbuf[a.cmdp])
}

func (a *Acme) ungetch() {
	a.cmdp--
	if a.cmdp < 0 {
		// unreachable
		bug("ungetch")
	}
}

// getnum reads an optional signed (if signok>1) integer; no digits defaults to
// the sign (i.e. 1) (edit.c).
func (a *Acme) getnum(signok int) int {
	n := 0
	sign := 1
	if signok > 1 && a.nextc() == '-' {
		// No command table entry has count>1 and addresses call getnum(1), so
		// the signed form is never requested.
		// untested
		sign = -1
		a.getch()
	}
	if c := a.nextc(); c < '0' || '9' < c { // no number defaults to 1
		return sign
	}
	for {
		c := a.getch()
		if c < '0' || c > '9' {
			break
		}
		n = n*10 + (c - '0')
	}
	a.ungetch()
	return sign * n
}

// cmdskipbl skips blanks and returns the next character without consuming it
// (edit.c).
func (a *Acme) cmdskipbl() int {
	var c int
	for {
		c = a.getch()
		if c != ' ' && c != '\t' {
			break
		}
	}
	if c >= 0 {
		a.ungetch()
	}
	return c
}

func (a *Acme) okdelim(c int) {
	if c == '\\' || 'a' <= c && c <= 'z' || 'A' <= c && c <= 'Z' || '0' <= c && c <= '9' {
		a.editerror("bad delimiter %c\n", rune(c))
	}
}

func (a *Acme) atnl() {
	a.cmdskipbl()
	c := a.getch()
	if c != '\n' {
		a.editerror("newline expected (saw %c)", rune(c))
	}
}

// getrhs reads the replacement text of s/c/a/i up to delim or newline,
// applying \ escapes (edit.c).
func (a *Acme) getrhs(delim int, cmd int) []rune {
	var s []rune
	for {
		c := a.getch()
		if c <= 0 || c == delim || c == '\n' {
			break
		}
		if c == '\\' {
			c = a.getch()
			if c <= 0 {
				// unreachable
				bug("bad right hand side")
			}
			if c == '\n' {
				a.ungetch()
				c = '\\'
			} else if c == 'n' {
				c = '\n'
			} else if c != delim && (cmd == 's' || c != '\\') { // s does its own
				s = append(s, '\\')
			}
		}
		s = append(s, rune(c))
	}
	a.ungetch() // let the caller see the delimiter/newline
	return s
}

// getregexp reads a delim-delimited regexp, remembering it as lastpat; an empty
// regexp reuses lastpat (edit.c).
func (a *Acme) getregexp(delim int) []rune {
	var buf []rune
	c := 0
	for i := 0; ; i++ {
		c = a.getch()
		if c == '\\' {
			if a.nextc() == delim {
				c = a.getch()
			} else if a.nextc() == '\\' {
				buf = append(buf, rune(c))
				c = a.getch()
			}
		} else if c == delim || c == '\n' {
			break
		}
		if i >= RBUFSIZE {
			// A regexp longer than a buffer block can't be delivered through the
			// -D command tag (the NPROG "expression too long" limit is hit first
			// for compilable patterns; see TestRegexpDiff).
			// untested
			a.editerror("regular expression too long")
		}
		buf = append(buf, rune(c))
	}
	if c != delim && c != 0 {
		a.ungetch()
	}
	if len(buf) > 0 {
		a.patset = true
		a.lastpat = buf
	}
	if len(a.lastpat) == 0 {
		a.editerror("no regular expression defined")
	}
	return append([]rune(nil), a.lastpat...)
}

// simpleaddr parses one address element, recursively absorbing a following
// element (inserting an implicit '+' where needed) (edit.c).
func (a *Acme) simpleaddr() *Addr {
	addr := new(Addr)
	switch a.cmdskipbl() {
	case '#':
		addr.typ = byte(a.getch())
		addr.num = a.getnum(1)
	case '0', '1', '2', '3', '4', '5', '6', '7', '8', '9':
		addr.num = a.getnum(1)
		addr.typ = 'l'
	case '/', '?', '"':
		addr.typ = byte(a.getch())
		addr.re = a.getregexp(int(addr.typ))
	case '.', '$', '+', '-', '\'':
		addr.typ = byte(a.getch())
	default:
		return nil
	}
	if addr.next = a.simpleaddr(); addr.next != nil {
		switch addr.next.typ {
		case '.', '$', '\'':
			if addr.typ == '"' {
				break
			}
			a.editerror("bad address syntax")
		case '"':
			a.editerror("bad address syntax")
		case 'l', '#':
			if addr.typ == '"' {
				break
			}
			fallthrough
		case '/', '?':
			if addr.typ != '+' && addr.typ != '-' {
				// insert the missing '+'
				nap := new(Addr)
				nap.typ = '+'
				nap.next = addr.next
				addr.next = nap
			}
		case '+', '-':
			// ok
		default:
			// unreachable
			bug("simpleaddr")
		}
	}
	return addr
}

// compoundaddr parses an address possibly joined by , or ; (edit.c).
func (a *Acme) compoundaddr() *Addr {
	addr := new(Addr)
	addr.left = a.simpleaddr()
	addr.typ = byte(a.cmdskipbl())
	if addr.typ != ',' && addr.typ != ';' {
		return addr.left
	}
	a.getch()
	addr.next = a.compoundaddr()
	if next := addr.next; next != nil && (next.typ == ',' || next.typ == ';') && next.left == nil {
		a.editerror("bad address syntax")
	}
	return addr
}
