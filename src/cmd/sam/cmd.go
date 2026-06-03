package main

// Addr is a parsed address.
type Addr struct {
	typ  int     // # l / ? . $ + - , ; " *
	are  *String // regexp (g.re)
	left *Addr   // left side of , and ; (g.aleft)
	num  Posn
	next *Addr // or right side of , and ;
}

// Cmd is a parsed command.
type Cmd struct {
	addr  *Addr
	re    *String
	ccmd  *Cmd    // target of x, g, {, etc. (g.cmd)
	ctext *String // text of a, c, i; rhs of s (g.text)
	caddr *Addr   // address for m, t (g.addr)
	next  *Cmd
	num   int
	flag  int
	cmdc  int // command character
}

type cmdFn func(*Sam, *File, *Cmd) bool

// Cmdtab is one command-table entry.
type Cmdtab struct {
	cmdc    int
	text    bool
	regexp  bool
	addr    bool
	defcmd  int
	defaddr int
	count   int
	token   string
	fn      cmdFn
}

// Default addresses.
const (
	aNo = iota
	aDot
	aAll
)

const linex = "\n"
const wordx = " \t\n"

var cmdtab []Cmdtab

func init() {
	cmdtab = cmdtabInit // avoid init cycle
}

var cmdtabInit = []Cmdtab{
	{'\n', false, false, false, 0, aDot, 0, "", (*Sam).nl_cmd},
	{'a', true, false, false, 0, aDot, 0, "", (*Sam).a_cmd},
	{'b', false, false, false, 0, aNo, 0, linex, (*Sam).b_cmd},
	{'B', false, false, false, 0, aNo, 0, linex, (*Sam).b_cmd},
	{'c', true, false, false, 0, aDot, 0, "", (*Sam).c_cmd},
	{'d', false, false, false, 0, aDot, 0, "", (*Sam).d_cmd},
	{'D', false, false, false, 0, aNo, 0, linex, (*Sam).D_cmd},
	{'e', false, false, false, 0, aNo, 0, wordx, (*Sam).e_cmd},
	{'f', false, false, false, 0, aNo, 0, wordx, (*Sam).f_cmd},
	{'g', false, true, false, 'p', aDot, 0, "", (*Sam).g_cmd},
	{'i', true, false, false, 0, aDot, 0, "", (*Sam).i_cmd},
	{'k', false, false, false, 0, aDot, 0, "", (*Sam).k_cmd},
	{'m', false, false, true, 0, aDot, 0, "", (*Sam).m_cmd},
	{'n', false, false, false, 0, aNo, 0, "", (*Sam).n_cmd},
	{'p', false, false, false, 0, aDot, 0, "", (*Sam).p_cmd},
	{'q', false, false, false, 0, aNo, 0, "", (*Sam).q_cmd},
	{'r', false, false, false, 0, aDot, 0, wordx, (*Sam).e_cmd},
	{'s', false, true, false, 0, aDot, 1, "", (*Sam).s_cmd},
	{'t', false, false, true, 0, aDot, 0, "", (*Sam).m_cmd},
	{'u', false, false, false, 0, aNo, 2, "", (*Sam).u_cmd},
	{'v', false, true, false, 'p', aDot, 0, "", (*Sam).g_cmd},
	{'w', false, false, false, 0, aAll, 0, wordx, (*Sam).w_cmd},
	{'x', false, true, false, 'p', aDot, 0, "", (*Sam).x_cmd},
	{'y', false, true, false, 'p', aDot, 0, "", (*Sam).x_cmd},
	{'X', false, true, false, 'f', aNo, 0, "", (*Sam).X_cmd},
	{'Y', false, true, false, 'f', aNo, 0, "", (*Sam).X_cmd},
	{'!', false, false, false, 0, aNo, 0, linex, (*Sam).plan9_cmd},
	{'>', false, false, false, 0, aDot, 0, linex, (*Sam).plan9_cmd},
	{'<', false, false, false, 0, aDot, 0, linex, (*Sam).plan9_cmd},
	{'|', false, false, false, 0, aDot, 0, linex, (*Sam).plan9_cmd},
	{'=', false, false, false, 0, aDot, 0, linex, (*Sam).eq_cmd},
	{'c' | 0x100, false, false, false, 0, aNo, 0, wordx, (*Sam).cd_cmd},
}

func (s *Sam) resetcmd() {
	s.linep = 0
	s.line[0] = 0
	s.terminp = 0
	s.termoutp = 0
}

func (s *Sam) inputc() int {
Again:
	if s.downloaded {
		for s.termoutp == s.terminp {
			s.cmdupdate()
			if s.patset {
				s.tellpat()
			}
			for s.termlocked > 0 {
				s.outT0(Hunlock)
				s.termlocked--
			}
			if s.rcv() == 0 {
				return -1
			}
		}
		r := s.termline[s.termoutp]
		s.termoutp++
		if s.termoutp == s.terminp {
			s.terminp = 0
			s.termoutp = 0
		}
		if r == 0 {
			// untested
			s.warn(Wnulls)
			goto Again
		}
		return int(r)
	}
	r, _, err := s.inbr.ReadRune()
	if err != nil {
		return -1
	}
	if r == 0 {
		s.warn(Wnulls)
		goto Again
	}
	return int(r)
}

func (s *Sam) inputline() int {
	// Could set linep = 0 and i = 0 here and just error(Etoolong) below,
	// but this way we keep old input buffer history around for a while.
	// This is useful only for debugging.
	i := s.linep
	for {
		c := s.inputc()
		if c <= 0 {
			return -1
		}
		if i == len(s.line)-1 {
			if s.linep == 0 {
				fail(Etoolong)
			}
			start := s.linep
			copy(s.line[0:], s.line[start:i])
			i -= start
			s.linep = 0
		}
		s.line[i] = Rune(c)
		i++
		if c == '\n' {
			break
		}
	}
	s.line[i] = 0
	return 1
}

func (s *Sam) getch() int {
	if s.eof {
		return -1
	}
	if s.line[s.linep] == 0 && s.inputline() < 0 {
		s.eof = true
		return -1
	}
	c := s.line[s.linep]
	s.linep++
	return int(c)
}

func (s *Sam) nextc() int {
	if s.line[s.linep] == 0 {
		// untested: downloaded-mode input edge (EOF mid-parse / oversized command window)
		return -1
	}
	return int(s.line[s.linep])
}

func (s *Sam) ungetch() {
	if s.linep--; s.linep < 0 {
		// unreachable
		s.panicf("ungetch")
	}
}

func (s *Sam) getnum(signok int) Posn {
	var n Posn
	sign := Posn(1)
	if signok > 1 && s.nextc() == '-' {
		sign = -1
		s.getch()
	}
	if c := s.nextc(); c < '0' || '9' < c { // no number defaults to 1
		return sign
	}
	for {
		c := s.getch()
		if !('0' <= c && c <= '9') {
			break
		}
		n = n*10 + Posn(c-'0')
	}
	s.ungetch()
	return sign * n
}

func (s *Sam) skipbl() int {
	var c int
	for {
		c = s.getch()
		if c != ' ' && c != '\t' {
			break
		}
	}
	if c >= 0 {
		s.ungetch()
	}
	return c
}

func (s *Sam) termcommand() {
	for p := s.cmdpt; p < Posn(s.cmd.b.nc()); p++ {
		if s.terminp >= len(s.termline) {
			// untested: downloaded-mode input edge (EOF mid-parse / oversized command window)
			s.cmdpt = Posn(s.cmd.b.nc())
			fail(Etoolong)
		}
		s.termline[s.terminp] = Rune(filereadc(s.cmd, p))
		s.terminp++
	}
	s.cmdpt = Posn(s.cmd.b.nc())
}

func (s *Sam) cmdloop() {
	for {
		if !s.downloaded && s.curfile != nil && s.curfile.unread {
			s.load(s.curfile)
		}
		cmdp := s.parsecmd(0)
		if cmdp == nil {
			if s.downloaded {
				s.rescue()
				s.exits("eof")
			}
			break
		}
		ocurfile := s.curfile
		loaded := s.curfile != nil && !s.curfile.unread
		if !s.cmdexec(s.curfile, cmdp) {
			break
		}
		s.cmdupdate()
		s.update()
		if s.downloaded && s.curfile != nil &&
			(ocurfile != s.curfile || (!loaded && !s.curfile.unread)) {
			s.outTs(Hcurrent, s.curfile.tag)
		}
		// don't allow type ahead on files that aren't bound
		if s.downloaded && s.curfile != nil && !s.curfile.hasrasp {
			s.terminp = s.termoutp
		}
	}
}

func newcmd() *Cmd   { return new(Cmd) }
func newaddr() *Addr { return new(Addr) }
func newre() *String { return newstring() }
func newstring() *String {
	p := new(String)
	Strinit(p)
	return p
}

func lookup(c int) int {
	for i := range cmdtab {
		if cmdtab[i].cmdc == c {
			return i
		}
	}
	return -1
}

func (s *Sam) okdelim(c int) {
	if c == '\\' || ('a' <= c && c <= 'z') ||
		('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') {
		fail_c(Edelim, c)
	}
}

func (s *Sam) atnl() {
	s.skipbl()
	if s.getch() != '\n' {
		fail(Enewline)
	}
}

func (s *Sam) getrhs(str *String, delim, cmd int) {
	for {
		c := s.getch()
		if c <= 0 || c == delim || c == '\n' {
			break
		}
		if c == '\\' {
			if c = s.getch(); c <= 0 {
				// unreachable: getch returns <= 0 only at EOF,
				// but a line with no terminating newline never completes,
				// so getrhs is never reached at EOF.
				fail(Ebadrhs)
			}
			if c == '\n' {
				s.ungetch()
				c = '\\'
			} else if c == 'n' {
				c = '\n'
			} else if c != delim && (cmd == 's' || c != '\\') { // s does its own
				Straddc(str, '\\')
			}
		}
		Straddc(str, c)
	}
	s.ungetch() // let client read whether delimiter, '\n' or whatever
}

func (s *Sam) collecttoken(end string) *String {
	str := newstring()
	for {
		c := s.nextc()
		if c != ' ' && c != '\t' {
			break
		}
		Straddc(str, s.getch()) // blanks significant for getname()
	}
	var c int
	for {
		c = s.getch()
		if c <= 0 || runeIn(end, c) {
			break
		}
		Straddc(str, c)
	}
	Straddc(str, 0)
	if c != '\n' {
		s.atnl()
	}
	return str
}

func (s *Sam) collecttext() *String {
	str := newstring()
	if s.skipbl() == '\n' {
		s.getch()
		i := 0
		for {
			begline := i
			var c int
			for {
				c = s.getch()
				if c <= 0 || c == '\n' {
					break
				}
				i++
				Straddc(str, c)
			}
			i++
			Straddc(str, '\n')
			if c < 0 {
				goto Return
			}
			if str.s[begline] == '.' && str.s[begline+1] == '\n' {
				break
			}
		}
		Strdelete(str, str.n()-2, str.n())
	} else {
		delim := s.getch()
		s.okdelim(delim)
		s.getrhs(str, delim, 'a')
		if s.nextc() == delim {
			s.getch()
		}
		s.atnl()
	}
Return:
	Straddc(str, 0) // JUST FOR CMDPRINT
	return str
}

func (s *Sam) parsecmd(nest int) *Cmd {
	cmd := &Cmd{addr: s.compoundaddr()}
	if s.skipbl() == -1 {
		return nil
	}
	c := s.getch()
	if c == -1 {
		// untested: downloaded-mode input edge (EOF mid-parse / oversized command window)
		return nil
	}
	cmd.cmdc = c
	if cmd.cmdc == 'c' && s.nextc() == 'd' { // sleazy two-character case
		s.getch() // the 'd'
		cmd.cmdc = 'c' | 0x100
	}
	i := lookup(cmd.cmdc)
	if i >= 0 {
		if cmd.cmdc == '\n' {
			return cmd // let nl_cmd work it all out
		}
		ct := &cmdtab[i]
		if ct.defaddr == aNo && cmd.addr != nil {
			fail(Enoaddr)
		}
		if ct.count != 0 {
			cmd.num = int(s.getnum(ct.count))
		}
		if ct.regexp {
			// x without pattern -> .*\n, indicated by cmd.re==nil
			// X without pattern is all files
			if (ct.cmdc != 'x' && ct.cmdc != 'X') || !runeIn(" \t\n", s.nextc()) {
				s.skipbl()
				c := s.getch()
				if c == '\n' || c < 0 {
					fail(Enopattern)
				}
				s.okdelim(c)
				cmd.re = s.getregexp(c)
				if ct.cmdc == 's' {
					cmd.ctext = newstring()
					s.getrhs(cmd.ctext, c, 's')
					if s.nextc() == c {
						if s.getch(); s.nextc() == 'g' {
							cmd.flag = s.getch()
						}
					}
				}
			}
		}
		if ct.addr {
			if cmd.caddr = s.simpleaddr(); cmd.caddr == nil {
				fail(Eaddress)
			}
		}
		if ct.defcmd != 0 {
			if s.skipbl() == '\n' {
				s.getch()
				cmd.ccmd = newcmd()
				cmd.ccmd.cmdc = ct.defcmd
			} else if cmd.ccmd = s.parsecmd(nest); cmd.ccmd == nil {
				// unreachable
				s.panicf("defcmd")
			}
		} else if ct.text {
			cmd.ctext = s.collecttext()
		} else if ct.token != "" {
			cmd.ctext = s.collecttoken(ct.token)
		} else {
			s.atnl()
		}
	} else {
		switch cmd.cmdc {
		case '{':
			var cp *Cmd
			for {
				if s.skipbl() == '\n' {
					s.getch()
				}
				ncp := s.parsecmd(nest + 1)
				if cp != nil {
					cp.next = ncp
				} else {
					cmd.ccmd = ncp
				}
				cp = ncp
				if cp == nil {
					break
				}
			}
		case '}':
			s.atnl()
			if nest == 0 {
				fail(Enolbrace)
			}
			return nil
		default:
			fail_c(Eunk, cmd.cmdc)
		}
	}
	return cmd
}

// getregexp reads a regular expression up to delim.
// NOTE: The original C code was marked /* BUGGERED */.
func (s *Sam) getregexp(delim int) *String {
	r := newre()
	var c int
	Strzero(&s.genstr)
	for {
		c = s.getch()
		if c == '\\' {
			if s.nextc() == delim {
				c = s.getch()
			} else if s.nextc() == '\\' {
				Straddc(&s.genstr, c)
				c = s.getch()
			}
		} else if c == delim || c == '\n' {
			break
		}
		Straddc(&s.genstr, c)
	}
	if c != delim && c != 0 {
		s.ungetch()
	}
	if s.genstr.n() > 0 {
		s.patset = true
		Strduplstr(&s.lastpat, &s.genstr)
		Straddc(&s.lastpat, 0)
	}
	if s.lastpat.n() <= 1 {
		fail(Epattern)
	}
	Strduplstr(r, &s.lastpat)
	return r
}

func (s *Sam) simpleaddr() *Addr {
	var addr Addr
	switch s.skipbl() {
	case '#':
		addr.typ = s.getch()
		addr.num = s.getnum(1)
	case '0', '1', '2', '3', '4', '5', '6', '7', '8', '9':
		addr.num = s.getnum(1)
		addr.typ = 'l'
	case '/', '?', '"':
		addr.typ = s.getch()
		addr.are = s.getregexp(addr.typ)
	case '.', '$', '+', '-', '\'':
		addr.typ = s.getch()
	default:
		return nil
	}
	if addr.next = s.simpleaddr(); addr.next != nil {
		switch addr.next.typ {
		case '.', '$', '\'':
			if addr.typ == '"' {
				break
			}
			fallthrough
		case '"':
			fail(Eaddress)
		case 'l', '#':
			if addr.typ == '"' {
				break
			}
			fallthrough
		case '/', '?':
			if addr.typ != '+' && addr.typ != '-' {
				// insert the missing '+'
				nap := newaddr()
				nap.typ = '+'
				nap.next = addr.next
				addr.next = nap
			}
		case '+', '-':
			// ok
		default:
			// unreachable
			s.panicf("simpleaddr")
		}
	}
	ap := newaddr()
	*ap = addr
	return ap
}

func (s *Sam) compoundaddr() *Addr {
	var addr Addr
	addr.left = s.simpleaddr()
	if addr.typ = s.skipbl(); addr.typ != ',' && addr.typ != ';' {
		return addr.left
	}
	s.getch()
	next := s.compoundaddr()
	addr.next = next
	if next != nil && (next.typ == ',' || next.typ == ';') && next.left == nil {
		fail(Eaddress)
	}
	ap := newaddr()
	*ap = addr
	return ap
}

func runeIn(set string, c int) bool {
	for _, r := range set {
		if int(r) == c {
			return true
		}
	}
	return false
}
