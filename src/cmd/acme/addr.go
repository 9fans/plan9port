package main

// Address evaluation for the edit language (addr.c). Addresses (line numbers,
// #char offsets, /regexp/, ., $, +, -, ,, ;) are read from a source via a getc
// callback and evaluated against a Text.

const (
	addrNone = 0
	addrFore = '+'
	addrBack = '-'
)

const (
	sizeChar = iota
	sizeLine
)

func isaddrc(r rune) bool {
	return r != 0 && runeIn(r, "0123456789+-/$.#,;?")
}

// isregexc: alphanumerics and a few metacharacters, no space (addr.c). Uses
// isalnum (so '_' and non-ASCII letters count), matching C exactly.
func isregexc(r rune) bool {
	if r == 0 {
		return false
	}
	if isalnum(r) {
		return true
	}
	return runeIn(r, "^+-.*?#,;[]()$")
}

func runeIn(r rune, set string) bool {
	for _, c := range set {
		if c == r {
			return true
		}
	}
	return false
}

// nlcounttopos starts at q0, advances nl lines (not past end of text), then nr
// chars (not past end of line), and returns the final position (addr.c).
func (a *Acme) nlcounttopos(t *Text, q0, nl, nr int) int {
	for nl > 0 && q0 < t.file.b.nc() {
		if a.textreadc(t, q0) == '\n' {
			nl--
		}
		q0++
	}
	if nl > 0 {
		return q0
	}
	for nr > 0 && q0 < t.file.b.nc() && a.textreadc(t, q0) != '\n' {
		q0++
		nr--
	}
	return q0
}

// number evaluates a line/char count relative to r in the given direction
// (addr.c number).
func (a *Acme) number(showerr bool, t *Text, r Range, line, dir, size int, evalp *bool) Range {
	nc := t.file.b.nc()
	if size == sizeChar {
		if dir == addrFore {
			line = r.q1 + line
		} else if dir == addrBack {
			if r.q0 == 0 && line > 0 {
				r.q0 = nc
			}
			line = r.q0 - line
		}
		if line < 0 || line > nc {
			goto Rescue
		}
		*evalp = true
		return runge(line, line)
	}
	{
		q0 := r.q0
		q1 := r.q1
		// addrNone and addrFore count forward into the file (the Forward loop in
		// C, reached by fall-through and a goto respectively); addrBack counts
		// backward in place.
		forward := false
		switch dir {
		case addrNone:
			q0 = 0
			q1 = 0
			forward = true
		case addrFore:
			if q1 > 0 {
				for q1 < nc && a.textreadc(t, q1-1) != '\n' {
					q1++
				}
			}
			q0 = q1
			forward = true
		case addrBack:
			if q0 < nc {
				for q0 > 0 && a.textreadc(t, q0-1) != '\n' {
					q0--
				}
			}
			q1 = q0
			for line > 0 && q0 > 0 {
				if a.textreadc(t, q0-1) == '\n' {
					line--
					if line >= 0 {
						q1 = q0
					}
				}
				q0--
			}
			// :1-1 is :0 = #0, but :1-2 is an error
			if line > 1 {
				goto Rescue
			}
			for q0 > 0 && a.textreadc(t, q0-1) != '\n' {
				q0--
			}
		}
		if forward {
			for line > 0 && q1 < nc {
				if a.textreadc(t, q1) == '\n' || q1+1 == nc {
					line--
					if line > 0 {
						q0 = q1 + 1
					}
				}
				q1++
			}
			if line == 1 && q1 == nc { // 6 goes to end of 5-line file
				goto Done
			}
			if line > 0 {
				goto Rescue
			}
		}
	Done:
		*evalp = true
		return runge(q0, q1)
	}
Rescue:
	if showerr {
		a.warning(nil, "address out of range\n")
	}
	*evalp = false
	return r
}

// addrRegexp evaluates a /regexp/ address (addr.c regexp).
func (a *Acme) addrRegexp(showerr bool, t *Text, lim, r Range, pat []rune, dir int, foundp *bool) Range {
	if len(pat) == 0 && a.re.rxnull() {
		if showerr {
			a.warning(nil, "no previous regular expression\n")
		}
		*foundp = false
		return r
	}
	if len(pat) > 0 && !a.re.rxcompile(pat) {
		*foundp = false
		return r
	}
	var sel Rangeset
	var found bool
	if dir == addrBack {
		found = a.re.rxbexecute(t, r.q0, &sel)
	} else {
		q := Infinity
		if lim.q0 >= 0 {
			q = lim.q1
		}
		found = a.re.rxexecute(t, nil, r.q1, q, &sel)
	}
	if !found && showerr {
		a.warning(nil, "no match for regexp\n")
	}
	*foundp = found
	return sel.r[0]
}

// address parses and evaluates an address from getc over [q0,q1), starting from
// ar, against Text t (addr.c address). It is recursive for , and ;.
func (a *Acme) address(showerr bool, t *Text, lim, ar Range, getc func(int) int, q0, q1 int, evalp *bool, qp *int, reverse bool) Range {
	r := ar
	q := q0
	dir := addrNone
	if reverse {
		dir = addrBack
	}
	size := sizeLine
	c := 0
	for q < q1 {
		prevc := c
		c = getc(q)
		q++
		switch c {
		default:
			*qp = q - 1
			return r
		case ';':
			ar = r
			fallthrough
		case ',':
			if prevc == 0 { // lhs defaults to 0
				r.q0 = 0
			}
			if q >= q1 && t != nil && t.file != nil { // rhs defaults to $
				r.q1 = t.file.b.nc()
			} else {
				var nq int
				nr := a.address(showerr, t, lim, ar, getc, q, q1, evalp, &nq, false)
				q = nq
				r.q1 = nr.q1
			}
			*qp = q
			return r
		case '+', '-':
			if *evalp && (prevc == '+' || prevc == '-') {
				if nc := getc(q); nc != '#' && nc != '/' && nc != '?' {
					r = a.number(showerr, t, r, 1, prevc, sizeLine, evalp) // do previous one
				}
			}
			dir = c
		case '.', '$':
			if q != q0+1 {
				*qp = q - 1
				return r
			}
			if *evalp {
				if c == '.' {
					r = ar
				} else {
					r = runge(t.file.b.nc(), t.file.b.nc())
				}
			}
			if q < q1 {
				dir = addrFore
			} else {
				dir = addrNone
			}
		case '#':
			if q == q1 {
				*qp = q - 1
				return r
			}
			c = getc(q)
			q++
			if c < '0' || '9' < c {
				*qp = q - 1
				return r
			}
			size = sizeChar
			fallthrough
		case '0', '1', '2', '3', '4', '5', '6', '7', '8', '9':
			n := c - '0'
			for q < q1 {
				nc := getc(q)
				q++
				if nc < '0' || '9' < nc {
					q--
					break
				}
				n = n*10 + (nc - '0')
			}
			if *evalp {
				r = a.number(showerr, t, r, n, dir, size, evalp)
			}
			dir = addrNone
			size = sizeLine
		case '?':
			dir = addrBack
			fallthrough
		case '/':
			var pat []rune
		out:
			for q < q1 {
				c = getc(q)
				q++
				switch c {
				case '\n':
					q--
					break out
				case '\\':
					pat = append(pat, rune(c))
					if q == q1 {
						break out
					}
					c = getc(q)
					q++
				case '/':
					break out
				}
				pat = append(pat, rune(c))
			}
			if *evalp {
				r = a.addrRegexp(showerr, t, lim, r, pat, dir, evalp)
			}
			dir = addrNone
			size = sizeLine
		}
	}
	if *evalp && dir != addrNone {
		r = a.number(showerr, t, r, 1, dir, sizeLine, evalp) // do previous one
	}
	*qp = q
	return r
}
