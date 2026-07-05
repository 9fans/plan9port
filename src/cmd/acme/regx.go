package main

// Regular-expression engine, ported from regx.c (the classic Plan 9 regexp
// with subexpression capture). The C version runs compilation in a helper
// thread purely to bound stack use; that has no observable effect, so we
// compile directly. State that the C version keeps in file-scope globals lives
// in reEngine; *Acme holds one. Matching reads characters either from a Text
// (via textreadc) or from a rune slice, exactly as in C.

const NRange = 10
const Infinity = 0x7FFFFFFF

// Actions and tokens. 0x1000000+x are operators (value == precedence);
// 0x2000000+x are tokens (operands). (regx.c)
const (
	reOPERATOR = 0x1000000
	reSTART    = reOPERATOR + 0 // marker on stack
	reRBRA     = reOPERATOR + 1 // )
	reLBRA     = reOPERATOR + 2 // (
	reOR       = reOPERATOR + 3 // |
	reCAT      = reOPERATOR + 4 // implicit concatenation
	reSTAR     = reOPERATOR + 5 // *
	rePLUS     = reOPERATOR + 6 // +
	reQUEST    = reOPERATOR + 7 // ?

	reANY     = 0x2000000    // . (any char but newline)
	reNOP     = reANY + 1    // no-op, internal
	reBOL     = reANY + 2    // ^
	reEOL     = reANY + 3    // $
	reCCLASS  = reANY + 4    // [] character class
	reNCCLASS = reANY + 5    // [^] negated class
	reEND     = reANY + 0x77 // match found

	reISATOR = reOPERATOR
	reISAND  = reANY
	reQUOTED = 0x4000000 // set for \-escaped lexical characters
)

const (
	reNPROG   = 1024
	reNLIST   = 127
	reNSTACK  = 20
	reDCLASS  = 10       // class allocation increment
	reRunemax = 0x10FFFF // sentinel for a range in a class
)

// Inst is one machine instruction. The C version unions {subid,class,right}
// (never used together on one inst) and {left,next} (which DO share storage:
// the STAR/QUEST closure trick fills an OR's left later, via a next
// assignment). We keep subid/class/right separate but a single lnext for the
// left/next union — splitting it would break *, +, ? closures.
type Inst struct {
	typ   int
	subid int
	class int
	right *Inst
	lnext *Inst
}

// Rangeset holds a match's whole span (r[0]) and subexpression spans (regx.c).
type Rangeset struct {
	r [NRange]Range
}

// Ilist is one live thread of the NFA.
type Ilist struct {
	inst *Inst
	se   Rangeset
}

type reNode struct {
	first *Inst
	last  *Inst
}

type reEngine struct {
	a *Acme

	prog       [reNPROG]Inst
	progp      int
	startinst  *Inst
	bstartinst *Inst
	class      [][]rune
	lastregexp []rune

	// parser state
	andstack    [reNSTACK]reNode
	andp        int
	atorstack   [reNSTACK]int
	atorp       int
	subidstack  [reNSTACK]int
	subidp      int
	lastwasand  bool
	cursubid    int
	nbra        int
	expr        []rune
	exprp       int
	backwards   bool
	negateclass bool

	// execution state
	sel    Rangeset
	sempty Rangeset
	list   [2][reNLIST + 1]Ilist
}

// regErr is panicked by regerror and recovered in rxcompile (the C regerror
// warns and longjmps out of the compile thread).
type regErr string

func (a *Acme) reinit() {
	a.re.a = a
	a.re.lastregexp = nil
	// External-command channels: output streams (cexec for edit pipes, ccons for
	// button-2) and the command lifecycle (ccommand register, cwait reap, handled
	// by waitthread live / the -D wait verb). Created here so every entry point
	// (threadmain, runD, the direct unit tests) has them.
	a.cexec = make(chan execChunk, 64)
	a.ccons = make(chan execChunk, 64)
	a.ccommand = make(chan *Command)
	a.cwait = make(chan *Waitmsg)
	a.nexec = 0
	a.command = nil
	a.pids = nil
}

func (e *reEngine) regerror(s string) {
	e.lastregexp = e.lastregexp[:0]
	e.a.warning(nil, "regexp: %s\n", s)
	panic(regErr(s))
}

func (e *reEngine) newinst(t int) *Inst {
	if e.progp >= reNPROG {
		e.regerror("expression too long")
	}
	e.prog[e.progp] = Inst{typ: t}
	p := &e.prog[e.progp]
	e.progp++
	return p
}

func (e *reEngine) realcompile() *Inst {
	e.startlex(e.expr)
	e.atorp = 0
	e.andp = 0
	e.subidp = 0
	e.cursubid = 0
	e.lastwasand = false
	// start with a low-priority operator to prime the parser
	e.pushator(reSTART - 1)
	for {
		token := e.lex()
		if token == reEND {
			break
		}
		if token&reISATOR == reOPERATOR {
			e.operator(token)
		} else {
			e.operand(token)
		}
	}
	// close with a low-priority operator, then force END
	e.evaluntil(reSTART)
	e.operand(reEND)
	e.evaluntil(reSTART)
	if e.nbra != 0 {
		e.regerror("unmatched `('")
	}
	e.andp-- // points to first and only operand
	return e.andstack[e.andp].first
}

// rxcompile compiles r (forward and backward machines), returning false on a
// malformed expression. It caches the last expression.
func (e *reEngine) rxcompile(r []rune) (ok bool) {
	if runeEq(e.lastregexp, r) {
		return true
	}
	e.lastregexp = e.lastregexp[:0]
	e.class = e.class[:0]
	defer func() {
		if recover() != nil {
			ok = false
		}
	}()
	e.progp = 0
	e.backwards = false
	e.bstartinst = nil
	e.expr = r
	e.startinst = e.realcompile()
	e.optimize(0)
	oprogp := e.progp
	e.backwards = true
	e.bstartinst = e.realcompile()
	e.optimize(oprogp)
	e.lastregexp = append(e.lastregexp[:0], r...)
	return true
}

func (e *reEngine) operand(t int) {
	if e.lastwasand {
		e.operator(reCAT) // catenate is implicit
	}
	i := e.newinst(t)
	if t == reCCLASS {
		if e.negateclass {
			i.typ = reNCCLASS // UGH
		}
		i.class = len(e.class) - 1 // UGH
	}
	e.pushand(i, i)
	e.lastwasand = true
}

func (e *reEngine) operator(t int) {
	if t == reRBRA {
		e.nbra--
		if e.nbra < 0 {
			e.regerror("unmatched `)'")
		}
	}
	if t == reLBRA {
		e.cursubid++ // silently ignored if too many
		e.nbra++
		if e.lastwasand {
			e.operator(reCAT)
		}
	} else {
		e.evaluntil(t)
	}
	if t != reRBRA {
		e.pushator(t)
	}
	e.lastwasand = false
	if t == reSTAR || t == reQUEST || t == rePLUS || t == reRBRA {
		e.lastwasand = true // these look like operands
	}
}

func (e *reEngine) pushand(f, l *Inst) {
	if e.andp >= reNSTACK {
		// unreachable
		bug("operand stack overflow")
	}
	e.andstack[e.andp] = reNode{first: f, last: l}
	e.andp++
}

func (e *reEngine) pushator(t int) {
	if e.atorp >= reNSTACK {
		// unreachable
		bug("operator stack overflow")
	}
	e.atorstack[e.atorp] = t
	e.atorp++
	if e.cursubid >= NRange {
		e.subidstack[e.subidp] = -1
	} else {
		e.subidstack[e.subidp] = e.cursubid
	}
	e.subidp++
}

func (e *reEngine) popand(op int) *reNode {
	if e.andp <= 0 {
		if op != 0 {
			e.regerror("missing operand for " + string(rune(op)))
		} else {
			// an empty stack with no pending operator cannot arise from a
			// well-formed token stream.
			// unreachable
			e.regerror("malformed regexp")
		}
	}
	e.andp--
	return &e.andstack[e.andp]
}

func (e *reEngine) popator() int {
	if e.atorp <= 0 {
		// unreachable
		bug("operator stack underflow")
	}
	e.subidp--
	e.atorp--
	return e.atorstack[e.atorp]
}

func (e *reEngine) evaluntil(pri int) {
	for pri == reRBRA || e.atorstack[e.atorp-1] >= pri {
		switch e.popator() {
		default:
			// unreachable
			bug("unknown regexp operator")
		case reLBRA:
			op1 := e.popand('(')
			inst2 := e.newinst(reRBRA)
			inst2.subid = e.subidstack[e.subidp]
			op1.last.lnext = inst2
			inst1 := e.newinst(reLBRA)
			inst1.subid = e.subidstack[e.subidp]
			inst1.lnext = op1.first
			e.pushand(inst1, inst2)
			return // must have been RBRA
		case reOR:
			op2 := e.popand('|')
			op1 := e.popand('|')
			inst2 := e.newinst(reNOP)
			op2.last.lnext = inst2
			op1.last.lnext = inst2
			inst1 := e.newinst(reOR)
			inst1.right = op1.first
			inst1.lnext = op2.first
			e.pushand(inst1, inst2)
		case reCAT:
			op2 := e.popand(0)
			op1 := e.popand(0)
			if e.backwards && op2.first.typ != reEND {
				op1, op2 = op2, op1
			}
			op1.last.lnext = op2.first
			e.pushand(op1.first, op2.last)
		case reSTAR:
			op2 := e.popand('*')
			inst1 := e.newinst(reOR)
			op2.last.lnext = inst1
			inst1.right = op2.first
			e.pushand(inst1, inst1)
		case rePLUS:
			op2 := e.popand('+')
			inst1 := e.newinst(reOR)
			op2.last.lnext = inst1
			inst1.right = op2.first
			e.pushand(op2.first, inst1)
		case reQUEST:
			op2 := e.popand('?')
			inst1 := e.newinst(reOR)
			inst2 := e.newinst(reNOP)
			inst1.lnext = inst2
			inst1.right = op2.first
			op2.last.lnext = inst2
			e.pushand(inst1, inst2)
		}
	}
}

func (e *reEngine) optimize(start int) {
	for i := start; e.prog[i].typ != reEND; i++ {
		target := e.prog[i].lnext
		for target != nil && target.typ == reNOP {
			target = target.lnext
		}
		e.prog[i].lnext = target
	}
}

func (e *reEngine) startlex(s []rune) {
	e.expr = s
	e.exprp = 0
	e.nbra = 0
}

func (e *reEngine) lexc() rune {
	if e.exprp >= len(e.expr) {
		return 0
	}
	c := e.expr[e.exprp]
	e.exprp++
	return c
}

func (e *reEngine) lex() int {
	c := int(e.lexc())
	switch c {
	case '\\':
		if e.exprp < len(e.expr) {
			c = int(e.lexc())
			if c == 'n' {
				c = '\n'
			}
		}
	case 0:
		c = reEND
		e.exprp-- // in case we come here again
	case '*':
		c = reSTAR
	case '?':
		c = reQUEST
	case '+':
		c = rePLUS
	case '|':
		c = reOR
	case '.':
		c = reANY
	case '(':
		c = reLBRA
	case ')':
		c = reRBRA
	case '^':
		c = reBOL
	case '$':
		c = reEOL
	case '[':
		c = reCCLASS
		e.bldcclass()
	}
	return c
}

func (e *reEngine) nextrec() rune {
	if e.exprp >= len(e.expr) || e.expr[e.exprp] == 0 ||
		(e.expr[e.exprp] == '\\' && (e.exprp+1 >= len(e.expr) || e.expr[e.exprp+1] == 0)) {
		e.regerror("malformed `[]'")
	}
	if e.expr[e.exprp] == '\\' {
		e.exprp++
		if e.expr[e.exprp] == 'n' {
			e.exprp++
			return '\n'
		}
		r := e.expr[e.exprp] | reQUOTED
		e.exprp++
		return r
	}
	r := e.expr[e.exprp]
	e.exprp++
	return r
}

func (e *reEngine) bldcclass() {
	var classp []rune
	// we have already seen the '['
	if e.exprp < len(e.expr) && e.expr[e.exprp] == '^' {
		classp = append(classp, '\n') // don't match newline in negate case
		e.negateclass = true
		e.exprp++
	} else {
		e.negateclass = false
	}
	for {
		c1 := e.nextrec()
		if c1 == ']' {
			break
		}
		if c1 == '-' {
			e.regerror("malformed `[]'")
		}
		if e.exprp < len(e.expr) && e.expr[e.exprp] == '-' {
			e.exprp++ // eat '-'
			c2 := e.nextrec()
			if c2 == ']' {
				e.regerror("malformed `[]'")
			}
			classp = append(classp, reRunemax, c1, c2)
		} else {
			classp = append(classp, c1&^reQUOTED)
		}
	}
	e.class = append(e.class, classp)
}

func (e *reEngine) classmatch(classno, c int, negate bool) bool {
	p := e.class[classno]
	for i := 0; i < len(p); {
		if p[i] == reRunemax {
			if int(p[i+1]) <= c && c <= int(p[i+2]) {
				return !negate
			}
			i += 3
		} else {
			if int(p[i]) == c {
				return !negate
			}
			i++
		}
	}
	return negate
}

// addinst adds inst to the nil-terminated thread list l with span sep, unless
// inst is already present. *l must be pending (a nil terminator follows); the
// early-return optimization relies on it (regx.c addinst).
func addinst(l []Ilist, inst *Inst, sep *Rangeset) bool {
	for i := 0; ; i++ {
		if l[i].inst == nil {
			l[i].inst = inst
			l[i].se = *sep
			l[i+1].inst = nil
			return true
		}
		if l[i].inst == inst {
			if sep.r[0].q0 < l[i].se.r[0].q0 {
				l[i].se = *sep // this would be a bug
			}
			return false // already there
		}
	}
}

func (e *reEngine) rxnull() bool {
	return e.startinst == nil || e.bstartinst == nil
}

// readc returns the rune at position p of the source: a Text (via textreadc)
// or, when t is nil, the rune slice r.
func (e *reEngine) readc(t *Text, r []rune, p int) int {
	if t != nil {
		return int(e.a.textreadc(t, p))
	}
	return int(r[p])
}

// rxexecute matches the compiled forward machine against the source (Text t or
// rune slice r) starting at startp, up to eof, returning the match in *rp
// (regx.c rxexecute).
func (e *reEngine) rxexecute(t *Text, r []rune, startp, eof int, rp *Rangeset) bool {
	flag := 0
	p := startp
	startchar := 0
	wrapped := 0
	nnl := 0
	if e.startinst.typ < reOPERATOR {
		startchar = e.startinst.typ
	}
	e.list[0][0].inst = nil
	e.list[1][0].inst = nil
	e.sel.r[0].q0 = -1
	var nc int
	if t != nil {
		nc = t.file.b.nc()
	} else {
		nc = len(r)
	}
	var ntl int
	var c int
	ret := false
	for {
	doloop:
		if p >= eof || p >= nc {
			switch wrapped {
			case 0, 2: // let the loop run one more click
				wrapped++
			case 1: // expired; wrap to beginning
				wrapped++
				if e.sel.r[0].q0 >= 0 || eof != Infinity {
					ret = e.sel.r[0].q0 >= 0
					goto Return
				}
				e.list[0][0].inst = nil
				e.list[1][0].inst = nil
				p = 0
				goto doloop
			default:
				ret = e.sel.r[0].q0 >= 0
				goto Return
			}
			c = 0
		} else {
			if ((wrapped != 0 && p >= startp) || e.sel.r[0].q0 > 0) && nnl == 0 {
				break
			}
			c = e.readc(t, r, p)
		}
		// fast check for first char
		if startchar != 0 && nnl == 0 && c != startchar {
			p++
			continue
		}
		tl := e.list[flag][:]
		flag ^= 1
		nl := e.list[flag][:]
		nl[0].inst = nil
		ntl = nnl
		nnl = 0
		if e.sel.r[0].q0 < 0 && (wrapped == 0 || p < startp || startp == eof) {
			e.sempty.r[0].q0 = p
			if addinst(tl, e.startinst, &e.sempty) {
				ntl++
				if ntl >= reNLIST {
					// ntl caps the thread list at reNLIST, so nnl <= ntl <= 126 and this
					// per-instruction check is never the one that overflows.
					// unreachable
					goto Overflow
				}
			}
		}
		for tn := 0; tl[tn].inst != nil; tn++ {
			inst := tl[tn].inst
		Switchstmt:
			switch inst.typ {
			default: // regular character
				if inst.typ == c {
					if addinst(nl, inst.lnext, &tl[tn].se) {
						nnl++
						if nnl >= reNLIST {
							// ntl caps the thread list at reNLIST, so nnl <= ntl <= 126 and this
							// per-instruction check is never the one that overflows.
							// unreachable
							goto Overflow
						}
					}
				}
			case reLBRA:
				if inst.subid >= 0 {
					tl[tn].se.r[inst.subid].q0 = p
				}
				inst = inst.lnext
				goto Switchstmt
			case reRBRA:
				if inst.subid >= 0 {
					tl[tn].se.r[inst.subid].q1 = p
				}
				inst = inst.lnext
				goto Switchstmt
			case reANY:
				if c != '\n' {
					if addinst(nl, inst.lnext, &tl[tn].se) {
						nnl++
						if nnl >= reNLIST {
							// ntl caps the thread list at reNLIST, so nnl <= ntl <= 126 and this
							// per-instruction check is never the one that overflows.
							// unreachable
							goto Overflow
						}
					}
				}
			case reBOL:
				if p == 0 || e.readc(t, r, p-1) == '\n' {
					inst = inst.lnext
					goto Switchstmt
				}
			case reEOL:
				if c == '\n' {
					inst = inst.lnext
					goto Switchstmt
				}
			case reCCLASS:
				if c >= 0 && e.classmatch(inst.class, c, false) {
					if addinst(nl, inst.lnext, &tl[tn].se) {
						nnl++
						if nnl >= reNLIST {
							// ntl caps the thread list at reNLIST, so nnl <= ntl <= 126 and this
							// per-instruction check is never the one that overflows.
							// unreachable
							goto Overflow
						}
					}
				}
			case reNCCLASS:
				if c >= 0 && e.classmatch(inst.class, c, true) {
					if addinst(nl, inst.lnext, &tl[tn].se) {
						nnl++
						if nnl >= reNLIST {
							// ntl caps the thread list at reNLIST, so nnl <= ntl <= 126 and this
							// per-instruction check is never the one that overflows.
							// unreachable
							goto Overflow
						}
					}
				}
			case reOR:
				// evaluate right choice later. Dedup-scan from the current
				// thread (tl[tn:]), not the whole list -- regx.c's forward
				// rxexecute passes tlp here (unlike rxbexecute, which passes tl),
				// so an inst.right already enqueued *before* tn is re-added rather
				// than suppressed.
				if addinst(tl[tn:], inst.right, &tl[tn].se) {
					ntl++
					if ntl >= reNLIST {
						goto Overflow
					}
				}
				inst = inst.lnext // advance and re-evaluate
				goto Switchstmt
			case reEND: // match!
				tl[tn].se.r[0].q1 = p
				e.newmatch(&tl[tn].se)
			}
		}
		p++
	}
	ret = e.sel.r[0].q0 >= 0
	goto Return
Overflow:
	e.a.warning(nil, "regexp list overflow\n")
	e.sel.r[0].q0 = -1
	ret = false
Return:
	*rp = e.sel
	return ret
}

func (e *reEngine) newmatch(sp *Rangeset) {
	if e.sel.r[0].q0 < 0 || sp.r[0].q0 < e.sel.r[0].q0 ||
		(sp.r[0].q0 == e.sel.r[0].q0 && sp.r[0].q1 > e.sel.r[0].q1) {
		e.sel = *sp
	}
}

// rxbexecute matches the backward machine against Text t ending at startp
// (regx.c rxbexecute).
func (e *reEngine) rxbexecute(t *Text, startp int, rp *Rangeset) bool {
	flag := 0
	nnl := 0
	wrapped := 0
	p := startp
	startchar := 0
	if e.bstartinst.typ < reOPERATOR {
		startchar = e.bstartinst.typ
	}
	e.list[0][0].inst = nil
	e.list[1][0].inst = nil
	e.sel.r[0].q0 = -1
	var ntl, c int
	ret := false
	for {
	doloop:
		if p <= 0 {
			switch wrapped {
			case 0, 2:
				wrapped++
			case 1: // expired; wrap to end
				wrapped++
				if e.sel.r[0].q0 >= 0 {
					ret = e.sel.r[0].q0 >= 0
					goto Return
				}
				e.list[0][0].inst = nil
				e.list[1][0].inst = nil
				p = t.file.b.nc()
				goto doloop
			default:
				ret = e.sel.r[0].q0 >= 0
				goto Return
			}
			c = 0
		} else {
			if ((wrapped != 0 && p <= startp) || e.sel.r[0].q0 > 0) && nnl == 0 {
				break
			}
			c = int(e.a.textreadc(t, p-1))
		}
		if startchar != 0 && nnl == 0 && c != startchar {
			p--
			continue
		}
		tl := e.list[flag][:]
		flag ^= 1
		nl := e.list[flag][:]
		nl[0].inst = nil
		ntl = nnl
		nnl = 0
		if e.sel.r[0].q0 < 0 && (wrapped == 0 || p > startp) {
			// the minus is so the optimizations in addinst work
			e.sempty.r[0].q0 = -p
			if addinst(tl, e.bstartinst, &e.sempty) {
				ntl++
				if ntl >= reNLIST {
					// ntl caps the thread list at reNLIST, so nnl <= ntl <= 126 and this
					// per-instruction check is never the one that overflows.
					// unreachable
					goto Overflow
				}
			}
		}
		for tn := 0; tl[tn].inst != nil; tn++ {
			inst := tl[tn].inst
		Switchstmt:
			switch inst.typ {
			default:
				if inst.typ == c {
					if addinst(nl, inst.lnext, &tl[tn].se) {
						nnl++
						if nnl >= reNLIST {
							// ntl caps the thread list at reNLIST, so nnl <= ntl <= 126 and this
							// per-instruction check is never the one that overflows.
							// unreachable
							goto Overflow
						}
					}
				}
			case reLBRA:
				if inst.subid >= 0 {
					tl[tn].se.r[inst.subid].q0 = p
				}
				inst = inst.lnext
				goto Switchstmt
			case reRBRA:
				if inst.subid >= 0 {
					tl[tn].se.r[inst.subid].q1 = p
				}
				inst = inst.lnext
				goto Switchstmt
			case reANY:
				if c != '\n' {
					if addinst(nl, inst.lnext, &tl[tn].se) {
						nnl++
						if nnl >= reNLIST {
							// ntl caps the thread list at reNLIST, so nnl <= ntl <= 126 and this
							// per-instruction check is never the one that overflows.
							// unreachable
							goto Overflow
						}
					}
				}
			case reBOL:
				if c == '\n' || p == 0 {
					inst = inst.lnext
					goto Switchstmt
				}
			case reEOL:
				if p < t.file.b.nc() && e.a.textreadc(t, p) == '\n' {
					inst = inst.lnext
					goto Switchstmt
				}
			case reCCLASS:
				if c > 0 && e.classmatch(inst.class, c, false) {
					if addinst(nl, inst.lnext, &tl[tn].se) {
						nnl++
						if nnl >= reNLIST {
							// ntl caps the thread list at reNLIST, so nnl <= ntl <= 126 and this
							// per-instruction check is never the one that overflows.
							// unreachable
							goto Overflow
						}
					}
				}
			case reNCCLASS:
				if c > 0 && e.classmatch(inst.class, c, true) {
					if addinst(nl, inst.lnext, &tl[tn].se) {
						nnl++
						if nnl >= reNLIST {
							// ntl caps the thread list at reNLIST, so nnl <= ntl <= 126 and this
							// per-instruction check is never the one that overflows.
							// unreachable
							goto Overflow
						}
					}
				}
			case reOR:
				if addinst(tl, inst.right, &tl[tn].se) {
					ntl++
					if ntl >= reNLIST {
						goto Overflow
					}
				}
				inst = inst.lnext
				goto Switchstmt
			case reEND: // match!
				tl[tn].se.r[0].q0 = -tl[tn].se.r[0].q0 // minus sign
				tl[tn].se.r[0].q1 = p
				e.bnewmatch(&tl[tn].se)
			}
		}
		p--
	}
	ret = e.sel.r[0].q0 >= 0
	goto Return
Overflow:
	e.a.warning(nil, "regexp list overflow\n")
	e.sel.r[0].q0 = -1
	ret = false
Return:
	*rp = e.sel
	return ret
}

func (e *reEngine) bnewmatch(sp *Rangeset) {
	if e.sel.r[0].q0 < 0 || sp.r[0].q0 > e.sel.r[0].q1 ||
		(sp.r[0].q0 == e.sel.r[0].q1 && sp.r[0].q1 < e.sel.r[0].q0) {
		for i := 0; i < NRange; i++ { // note the reversal; q0<=q1
			e.sel.r[i].q0 = sp.r[i].q1
			e.sel.r[i].q1 = sp.r[i].q0
		}
	}
}

// runeEq reports whether two rune slices are equal.
func runeEq(a, b []rune) bool {
	if len(a) != len(b) {
		return false
	}
	for i := range a {
		if a[i] != b[i] {
			return false
		}
	}
	return true
}
