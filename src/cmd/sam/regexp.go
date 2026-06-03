package main

import (
	"slices"
	"unicode/utf8"
)

// Faithful port of regexp.c: a Thompson NFA compiled in both forward and
// backward directions, executed once per character with wrap-around so that a
// search starting mid-file finds the next match anywhere.

const (
	NPROG   = 1024
	NLIST   = 127
	NSTACK  = 20
	NSUBEXP = 10

	// Actions and tokens. 0x10000xx are operators (value == precedence),
	// 0x20000xx are operands.
	OPERATOR = 0x1000000    // bit set in all operators
	START    = OPERATOR + 0 // start, used for marker on stack
	RBRA     = OPERATOR + 1 // right bracket, )
	LBRA     = OPERATOR + 2 // left bracket, (
	OR       = OPERATOR + 3 // alternation, |
	CAT      = OPERATOR + 4 // concatenation, implicit operator
	STAR     = OPERATOR + 5 // closure, *
	PLUS     = OPERATOR + 6 // a+ == aa*
	QUEST    = OPERATOR + 7 // a? == a|nothing, i.e. 0 or 1 a's
	ANY      = 0x2000000    // any character but newline, .
	NOP      = ANY + 1      // no operation, internal use only
	BOL      = ANY + 2      // beginning of line, ^
	EOL      = ANY + 3      // end of line, $
	CCLASS   = ANY + 4      // character class, []
	NCCLASS  = ANY + 5      // negated character class, [^]
	END      = ANY + 0x77   // terminate: match found

	ISATOR = OPERATOR
	ISAND  = ANY

	QUOTED = 0x4000000 // bit set for \-ed lex characters

	DCLASS = 10 // class allocation increment (unused size detail in Go)
)

// Rangeset holds a match plus its subexpression matches.
type Rangeset struct {
	p [NSUBEXP]Range
}

// Inst is one instruction of a compiled regular expression (regexp.c).
// In the C struct, left and next share a union (the closure construction in
// evaluntil relies on it: a STAR's OR gets its fall-through ".left" set later
// when a following operand assigns ".next"). So they are one field, next, here.
// subid/class/alt share the other C union but are never used together on a
// single instruction, so they stay separate.
type Inst struct {
	typ   rune // < OPERATOR ==> literal, otherwise action
	subid int
	class int
	next  *Inst
	alt   *Inst
}

// Ilist is one thread of the regexp NFA (regexp.c).
type Ilist struct {
	inst   *Inst // instruction of the thread
	se     Rangeset
	startp Posn // first char of match
}

// Node is the regexp parser's operand stack node.
type Node struct {
	first *Inst
	last  *Inst
}

func (s *Sam) reginit() {
	s.program = make([]Inst, NPROG)
	s.relist[0] = make([]Ilist, NLIST+2)
	s.relist[1] = make([]Ilist, NLIST+2)
	s.andstack = make([]Node, NSTACK)
	s.atorstack = make([]int, NSTACK)
	s.subidstack = make([]int, NSTACK)
}

func (s *Sam) regerror(e Err) {
	s.lastregexp = nil
	s.error(e)
}

func (s *Sam) regerror_c(e Err, c rune) {
	s.lastregexp = nil
	s.error_c(e, c)
}

func (s *Sam) newinst(t int) *Inst {
	if s.progp >= NPROG {
		s.regerror(Etoolong)
	}
	p := &s.program[s.progp]
	p.typ = rune(t)
	p.next = nil
	p.alt = nil
	p.subid = 0
	p.class = 0
	s.progp++
	return p
}

// realcompile compiles the rune source and returns the start instruction.
func (s *Sam) realcompile(src []rune) *Inst {
	s.startlex(src)
	s.atorp = 0
	s.andp = 0
	s.subidp = 0
	s.cursubid = 0
	s.lastwasand = false
	// prime parser with a low-priority operator
	s.pushator(START - 1)
	for {
		token := s.lex()
		if token == END {
			break
		}
		if token&ISATOR == OPERATOR {
			s.operator(token)
		} else {
			s.operand(token)
		}
	}
	// close with a low-priority operator
	s.evaluntil(START)
	// force END
	s.operand(END)
	s.evaluntil(START)
	if s.nbra != 0 {
		s.regerror(Eleftpar)
	}
	s.andp-- // points to first and only operand
	return s.andstack[s.andp].first
}

func (s *Sam) compile(str []rune) {
	if runesCompare(str, s.lastregexp) == 0 {
		return
	}
	s.class = s.class[:0]
	s.progp = 0
	s.backwards = false
	s.startinst = s.realcompile(str)
	s.optimize(0)
	oprogp := s.progp
	s.backwards = true
	s.bstartinst = s.realcompile(str)
	s.optimize(oprogp)
	s.lastregexp = slices.Clone(str)
}

func (s *Sam) operand(t int) {
	if s.lastwasand {
		s.operator(CAT) // catenate is implicit
	}
	i := s.newinst(t)
	if t == CCLASS {
		if s.negateclass {
			i.typ = NCCLASS
		}
		i.class = len(s.class) - 1
	}
	s.pushand(i, i)
	s.lastwasand = true
}

func (s *Sam) operator(t int) {
	if t == RBRA {
		if s.nbra--; s.nbra < 0 {
			s.regerror(Erightpar)
		}
	}
	if t == LBRA {
		s.cursubid++ // silently ignored if >= NSUBEXP
		s.nbra++
		if s.lastwasand {
			s.operator(CAT)
		}
	} else {
		s.evaluntil(t)
	}
	if t != RBRA {
		s.pushator(t)
	}
	s.lastwasand = false
	if t == STAR || t == QUEST || t == PLUS || t == RBRA {
		s.lastwasand = true // these look like operands
	}
}

func (s *Sam) cant(msg string) {
	// unreachable
	s.panic("regexp: can't happen: " + msg)
}

func (s *Sam) pushand(f, l *Inst) {
	if s.andp >= NSTACK {
		// unreachable
		s.cant("operand stack overflow")
	}
	s.andstack[s.andp].first = f
	s.andstack[s.andp].last = l
	s.andp++
}

func (s *Sam) pushator(t int) {
	if s.atorp >= NSTACK {
		// unreachable
		s.cant("operator stack overflow")
	}
	s.atorstack[s.atorp] = t
	s.atorp++
	if s.cursubid >= NSUBEXP {
		s.subidstack[s.subidp] = -1
	} else {
		s.subidstack[s.subidp] = s.cursubid
	}
	s.subidp++
}

func (s *Sam) popand(op rune) *Node {
	if s.andp <= 0 {
		if op != 0 {
			s.regerror_c(Emissop, op)
		} else {
			// untested
			s.regerror(Ebadregexp)
		}
	}
	s.andp--
	return &s.andstack[s.andp]
}

func (s *Sam) popator() int {
	if s.atorp <= 0 {
		// unreachable
		s.cant("operator stack underflow")
	}
	s.subidp--
	s.atorp--
	return s.atorstack[s.atorp]
}

func (s *Sam) evaluntil(pri int) {
	for pri == RBRA || s.atorstack[s.atorp-1] >= pri {
		switch s.popator() {
		case LBRA:
			op1 := s.popand('(')
			inst2 := s.newinst(RBRA)
			inst2.subid = s.subidstack[s.subidp]
			op1.last.next = inst2
			inst1 := s.newinst(LBRA)
			inst1.subid = s.subidstack[s.subidp]
			inst1.next = op1.first
			s.pushand(inst1, inst2)
			return // must have been RBRA
		default:
			// unreachable
			s.panic("unknown regexp operator")
		case OR:
			op2 := s.popand('|')
			op1 := s.popand('|')
			inst2 := s.newinst(NOP)
			op2.last.next = inst2
			op1.last.next = inst2
			inst1 := s.newinst(OR)
			inst1.alt = op1.first
			inst1.next = op2.first
			s.pushand(inst1, inst2)
		case CAT:
			op2 := s.popand(0)
			op1 := s.popand(0)
			if s.backwards && op2.first.typ != END {
				op1, op2 = op2, op1
			}
			op1.last.next = op2.first
			s.pushand(op1.first, op2.last)
		case STAR:
			op2 := s.popand('*')
			inst1 := s.newinst(OR)
			op2.last.next = inst1
			inst1.alt = op2.first
			s.pushand(inst1, inst1)
		case PLUS:
			op2 := s.popand('+')
			inst1 := s.newinst(OR)
			op2.last.next = inst1
			inst1.alt = op2.first
			s.pushand(op2.first, inst1)
		case QUEST:
			op2 := s.popand('?')
			inst1 := s.newinst(OR)
			inst2 := s.newinst(NOP)
			inst1.next = inst2
			inst1.alt = op2.first
			op2.last.next = inst2
			s.pushand(inst1, inst2)
		}
	}
}

func (s *Sam) optimize(start int) {
	for i := start; s.program[i].typ != END; i++ {
		inst := &s.program[i]
		target := inst.next
		for target.typ == NOP {
			target = target.next
		}
		inst.next = target
	}
}

func (s *Sam) startlex(src []rune) {
	s.exprp = src
	s.exprpos = 0
	s.nbra = 0
}

func (s *Sam) lex() int {
	if s.exprpos >= len(s.exprp) {
		return END
	}
	c := int(s.exprp[s.exprpos])
	s.exprpos++
	switch c {
	case '\\':
		if s.exprpos < len(s.exprp) {
			c = int(s.exprp[s.exprpos])
			s.exprpos++
			if c == 'n' {
				c = '\n'
			}
		}
	case '*':
		c = STAR
	case '?':
		c = QUEST
	case '+':
		c = PLUS
	case '|':
		c = OR
	case '.':
		c = ANY
	case '(':
		c = LBRA
	case ')':
		c = RBRA
	case '^':
		c = BOL
	case '$':
		c = EOL
	case '[':
		c = CCLASS
		s.bldcclass()
	}
	return c
}

func (s *Sam) nextrec() rune {
	if s.exprpos >= len(s.exprp) || (s.exprp[s.exprpos] == '\\' && s.exprpos+1 >= len(s.exprp)) {
		s.regerror(Ebadclass)
	}
	if s.exprp[s.exprpos] == '\\' {
		s.exprpos++
		if s.exprp[s.exprpos] == 'n' {
			s.exprpos++
			return '\n'
		}
		c := s.exprp[s.exprpos] | QUOTED
		s.exprpos++
		return c
	}
	c := s.exprp[s.exprpos]
	s.exprpos++
	return c
}

func (s *Sam) bldcclass() {
	var classp []rune
	// we have already seen the '['
	if s.exprpos < len(s.exprp) && s.exprp[s.exprpos] == '^' {
		classp = append(classp, '\n') // don't match newline in negate case
		s.negateclass = true
		s.exprpos++
	} else {
		s.negateclass = false
	}
	for {
		c1 := s.nextrec()
		if c1 == ']' {
			break
		}
		if c1 == '-' {
			s.regerror(Ebadclass)
		}
		if s.exprpos < len(s.exprp) && s.exprp[s.exprpos] == '-' {
			s.exprpos++ // eat '-'
			c2 := s.nextrec()
			if c2 == ']' {
				s.regerror(Ebadclass)
			}
			classp = append(classp, utf8.MaxRune, c1, c2)
		} else {
			classp = append(classp, c1&^QUOTED)
		}
	}
	s.class = append(s.class, classp)
}

func (s *Sam) classmatch(classno int, c rune, negate bool) bool {
	p := s.class[classno]
	i := 0
	for i < len(p) {
		if p[i] == utf8.MaxRune {
			if p[i+1] <= c && c <= p[i+2] {
				return !negate
			}
			i += 3
		} else if p[i] == c {
			return !negate
		} else {
			i++
		}
	}
	return negate
}

// addinst adds inst to thread list l (a slice starting at the relevant point),
// coalescing duplicate threads. It returns true if inst is newly added.
//
// Note the optimization: l must be pending when addinst is called;
// if l has already been looked at, the optimization is a bug.
func addinst(l []Ilist, inst *Inst, sep *Rangeset) bool {
	i := 0
	for l[i].inst != nil {
		if l[i].inst == inst {
			if sep.p[0].p1 < l[i].se.p[0].p1 {
				l[i].se = *sep // this would be bug
			}
			return false // already there
		}
		i++
	}
	l[i].inst = inst
	l[i].se = *sep
	l[i+1].inst = nil
	return true
}

func (s *Sam) execute(f *File, startp, eof Posn) bool {
	flag := 0
	p := startp
	nnl, ntl := 0, 0
	wrapped := 0
	startchar := rune(0)
	if s.startinst.typ < OPERATOR {
		startchar = s.startinst.typ
	}

	s.relist[0][0].inst = nil
	s.relist[1][0].inst = nil
	s.sel.p[0].p1 = -1
	// execute machine once for each character
	for ; ; p++ {
	doloop:
		c := filereadc(f, p)
		if p >= eof || c < 0 {
			switch wrapped {
			case 0, 2: // let loop run one more click / expired marker
				wrapped++
			case 1: // expired; wrap to beginning
				wrapped++
				if s.sel.p[0].p1 >= 0 || eof != INFINITY {
					goto Return
				}
				s.relist[0][0].inst = nil
				s.relist[1][0].inst = nil
				p = 0
				goto doloop
			default:
				goto Return
			}
		} else if ((wrapped != 0 && p >= startp) || s.sel.p[0].p1 > 0) && nnl == 0 {
			break
		}
		// fast check for first char
		if startchar != 0 && nnl == 0 && c != startchar {
			continue
		}
		tl := s.relist[flag]
		flag ^= 1
		nl := s.relist[flag]
		nl[0].inst = nil
		ntl = nnl
		nnl = 0
		if s.sel.p[0].p1 < 0 && (wrapped == 0 || p < startp || startp == eof) {
			// add first instruction to this list
			s.sempty.p[0].p1 = p
			if addinst(tl, s.startinst, &s.sempty) {
				if ntl++; ntl >= NLIST {
					// untested: the check in case OR below stops first
					s.error(Eoverflow)
				}
			}
		}
		// execute machine until this list is empty
		for j := 0; tl[j].inst != nil; j++ {
			inst := tl[j].inst
		Switchstmt:
			switch inst.typ {
			default: // regular character
				if inst.typ == c {
					goto Addinst
				}
			case LBRA:
				if inst.subid >= 0 {
					tl[j].se.p[inst.subid].p1 = p
				}
				inst = inst.next
				goto Switchstmt
			case RBRA:
				if inst.subid >= 0 {
					tl[j].se.p[inst.subid].p2 = p
				}
				inst = inst.next
				goto Switchstmt
			case ANY:
				if c != '\n' {
					goto Addinst
				}
			case BOL:
				if p == 0 || filereadc(f, p-1) == '\n' {
					inst = inst.next
					goto Switchstmt
				}
			case EOL:
				if c == '\n' {
					inst = inst.next
					goto Switchstmt
				}
			case CCLASS:
				if c >= 0 && s.classmatch(inst.class, c, false) {
					goto Addinst
				}
			case NCCLASS:
				if c >= 0 && s.classmatch(inst.class, c, true) {
					goto Addinst
				}
			case OR:
				// evaluate right choice later
				if addinst(tl, inst.alt, &tl[j].se) {
					if ntl++; ntl >= NLIST {
						s.error(Eoverflow)
					}
				}
				// efficiency: advance and re-evaluate
				inst = inst.next // left
				goto Switchstmt
			case END: // Match!
				tl[j].se.p[0].p2 = p
				s.newmatch(&tl[j].se)
			}
			continue
		Addinst:
			if addinst(nl, inst.next, &tl[j].se) {
				if nnl++; nnl >= NLIST {
					// untested: the check in case OR above stops first
					s.error(Eoverflow)
				}
			}
		}
	}
Return:
	return s.sel.p[0].p1 >= 0
}

func (s *Sam) newmatch(sp *Rangeset) {
	if s.sel.p[0].p1 < 0 || sp.p[0].p1 < s.sel.p[0].p1 ||
		(sp.p[0].p1 == s.sel.p[0].p1 && sp.p[0].p2 > s.sel.p[0].p2) {
		s.sel = *sp
	}
}

func (s *Sam) bexecute(f *File, startp Posn) bool {
	flag := 0
	p := startp
	nnl, ntl := 0, 0
	wrapped := 0
	startchar := rune(0)
	if s.bstartinst.typ < OPERATOR {
		startchar = s.bstartinst.typ
	}

	s.relist[0][0].inst = nil
	s.relist[1][0].inst = nil
	s.sel.p[0].p1 = -1
	// execute machine once for each character, including terminal NUL
	for ; ; p-- {
	doloop:
		c := filereadc(f, p-1)
		if c == -1 {
			switch wrapped {
			case 0, 2:
				wrapped++
			case 1: // expired; wrap to end
				wrapped++
				if s.sel.p[0].p1 >= 0 {
					// untested
					goto Return
				}
				s.relist[0][0].inst = nil
				s.relist[1][0].inst = nil
				p = f.b.nc()
				goto doloop
			default:
				// untested
				goto Return
			}
		} else if ((wrapped != 0 && p <= startp) || s.sel.p[0].p1 > 0) && nnl == 0 {
			break
		}
		if startchar != 0 && nnl == 0 && c != startchar {
			continue
		}
		tl := s.relist[flag]
		flag ^= 1
		nl := s.relist[flag]
		nl[0].inst = nil
		ntl = nnl
		nnl = 0
		if s.sel.p[0].p1 < 0 && (wrapped == 0 || p > startp) {
			// Add first instruction to this list.
			// The minus is so the optimizations in addinst work
			s.sempty.p[0].p1 = -p
			if addinst(tl, s.bstartinst, &s.sempty) {
				if ntl++; ntl >= NLIST {
					// untested: the check in case OR below stops first
					s.error(Eoverflow)
				}
			}
		}
		for j := 0; tl[j].inst != nil; j++ {
			inst := tl[j].inst
		Switchstmt:
			switch inst.typ {
			default:
				if inst.typ == c {
					goto Addinst
				}
			case LBRA:
				if inst.subid >= 0 {
					tl[j].se.p[inst.subid].p1 = p
				}
				inst = inst.next
				goto Switchstmt
			case RBRA:
				if inst.subid >= 0 {
					tl[j].se.p[inst.subid].p2 = p
				}
				inst = inst.next
				goto Switchstmt
			case ANY:
				if c != '\n' {
					goto Addinst
				}
			case BOL:
				if c == '\n' || p == 0 {
					inst = inst.next
					goto Switchstmt
				}
			case EOL:
				if p == f.b.nc() || filereadc(f, p) == '\n' {
					inst = inst.next
					goto Switchstmt
				}
			case CCLASS:
				if c >= 0 && s.classmatch(inst.class, c, false) {
					goto Addinst
				}
			case NCCLASS:
				if c >= 0 && s.classmatch(inst.class, c, true) {
					goto Addinst
				}
			case OR:
				// evaluate right choice later
				if addinst(tl[j:], inst.alt, &tl[j].se) {
					if ntl++; ntl >= NLIST {
						s.error(Eoverflow)
					}
				}
				// efficiency: advance and re-evaluate
				inst = inst.next
				goto Switchstmt
			case END: // Match!
				tl[j].se.p[0].p1 = -tl[j].se.p[0].p1 // minus sign
				tl[j].se.p[0].p2 = p
				s.bnewmatch(&tl[j].se)
			}
			continue
		Addinst:
			if addinst(nl, inst.next, &tl[j].se) {
				if nnl++; nnl >= NLIST {
					// untested: the check in case OR above stops first
					s.error(Eoverflow)
				}
			}
		}
	}
Return:
	return s.sel.p[0].p1 >= 0
}

func (s *Sam) bnewmatch(sp *Rangeset) {
	if s.sel.p[0].p1 < 0 || sp.p[0].p1 > s.sel.p[0].p2 ||
		(sp.p[0].p1 == s.sel.p[0].p2 && sp.p[0].p2 < s.sel.p[0].p1) {
		for i := range NSUBEXP { // note the reversal; p1<=p2
			s.sel.p[i].p1 = sp.p[i].p2
			s.sel.p[i].p2 = sp.p[i].p1
		}
	}
}
