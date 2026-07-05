package main

import (
	"bufio"
	"fmt"
	"os"
	"path"
	"strings"
	"time"

	"9fans.net/go/plan9"
	"9fans.net/plan9port/draw"
	"9fans.net/plan9port/plumb"
)

// Port of look.c: button-3 (look3) selection expansion, text search, and file
// opening. Plumbing is wired: startplumbing connects plumbsendfid and look3
// sends the click to the plumber (plumbword), falling through to the built-in
// interpretation only when no plumber is connected or it declines. The
// include-directory machinery (w.incl) is present via the Incl builtin.

// Expand describes what a button-3 click expanded to (dat.h Expand).
type Expand struct {
	q0, q1  int
	name    []rune
	bname   string
	jump    bool
	reverse bool
	at      *Text         // the text the address (a0:a1) refers to (in-file address)
	a0, a1  int           // the address expression's range, read via agetc
	agetc   func(int) int // getc for the address expression; nil = read from at
}

// tgetc returns a getc callback over text t for address evaluation (util.c tgetc).
func (a *Acme) tgetcOf(t *Text) func(int) int {
	return func(n int) int {
		if n >= t.file.b.nc() {
			// untested: addresses are evaluated within the buffer here
			return 0
		}
		return int(a.textreadc(t, n))
	}
}

// texthas reports whether t contains the runes of s starting at q0 (look.c).
func (a *Acme) texthas(t *Text, q0 int, s []rune) bool {
	if q0 < 0 {
		return false
	}
	for i, c := range s {
		if q0+i >= t.file.b.nc() || a.textreadc(t, q0+i) != c {
			return false
		}
	}
	return true
}

// cleanrname cleans a rune path (look.c cleanrname / libc cleanname).
func cleanrname(r []rune) []rune {
	return []rune(path.Clean(string(r)))
}

// ismtpt reports whether file is under the acme mount point (acme.c ismtpt).
// "Not foolproof, but it will stop a lot of them." mtpt is unset (no -m) under
// -D, so this is false there.
func (a *Acme) ismtpt(file string) bool {
	if a.mtpt == "" {
		return false
	}
	n := len(a.mtpt)
	if !strings.HasPrefix(file, a.mtpt) {
		return false
	}
	return a.mtpt[n-1] == '/' || len(file) == n || file[n] == '/'
}

// includefile returns dir/file if it exists, else nil (look.c).
func includefile(dir, file []rune) []rune {
	name := string(dir) + "/" + string(file)
	if _, err := os.Stat(name); err != nil {
		return nil
	}
	return cleanrname([]rune(name))
}

// includename resolves an <include> file against the window's own include list,
// the standard include dirs, and finally the $objtype-derived dir (look.c).
func (a *Acme) includename(t *Text, r []rune) []rune {
	// derive /$objtype/include once, like C's cached objdir
	if a.objdir == nil && a.objtype != "" {
		a.objdir = []rune("/" + a.objtype + "/include")
	}
	dirs := [][]rune{
		[]rune("/sys/include"),
		[]rune("/usr/local/plan9/include"),
		[]rune("/usr/local/include"),
		[]rune("/usr/include"),
	}
	if len(r) == 0 || r[0] == '/' || t.w == nil {
		return r
	}
	if len(r) > 2 && r[0] == '.' && r[1] == '/' {
		// untested: no -D look uses a ./-relative include
		return r
	}
	for _, d := range t.w.incl {
		if f := includefile(d, r); f != nil {
			return f
		}
	}
	for _, d := range dirs {
		if f := includefile(d, r); f != nil {
			return f
		}
	}
	if a.objdir != nil {
		if f := includefile(a.objdir, r); f != nil {
			return f
		}
	}
	return r
}

// dirname combines r with the directory of t's file name (look.c).
func (a *Acme) dirname(t *Text, r []rune) []rune {
	if t == nil || t.w == nil || t.w.tag.file.b.nc() == 0 || (len(r) >= 1 && r[0] == '/') {
		// untested: dirname is reached only with a relative name in a tagged window
		return rescueDir(r)
	}
	b, i := a.parsetag(t.w, len(r))
	slash := -1
	for i--; i >= 0; i-- {
		if b[i] == '/' {
			slash = i
			break
		}
	}
	if slash < 0 {
		return rescueDir(r)
	}
	// Grow into parsetag's reserved capacity before splicing r after the slash
	// (C runemove into the over-allocated buffer + runestr(b, slash+1+n)); a
	// plain copy would truncate r and the reslice would panic on a tight buffer.
	b = b[:slash+1+len(r)]
	copy(b[slash+1:], r)
	return cleanrname(b)
}

func rescueDir(r []rune) []rune {
	if r != nil {
		return cleanrname(r)
	}
	// untested: dirname always passes a non-nil name
	return r
}

// expandfile decides whether [q0,q1) names a file (look.c expandfile).
func (a *Acme) expandfile(t *Text, q0, q1 int, e *Expand, reverse bool) bool {
	lhttp := []rune("http://")
	lhttps := []rune("https://")
	amax := q1
	if q1 == q0 {
		colon := -1
		for q1 < t.file.b.nc() {
			c := a.textreadc(t, q1)
			if !isfilec(c) {
				break
			}
			if c == ':' && !a.texthas(t, q1-4, lhttp) && !a.texthas(t, q1-5, lhttps) {
				colon = q1
				break
			}
			q1++
		}
		for q0 > 0 {
			c := a.textreadc(t, q0-1)
			if !(isfilec(c) || isaddrc(c) || isregexc(c)) {
				break
			}
			q0--
			if colon < 0 && c == ':' && !a.texthas(t, q0-4, lhttp) && !a.texthas(t, q0-5, lhttps) {
				colon = q0
			}
		}
		if colon >= 0 {
			q1 = colon
			if colon < t.file.b.nc()-1 && isaddrc(a.textreadc(t, colon+1)) {
				q1 = colon + 1
				for q1 < t.file.b.nc() && isaddrc(a.textreadc(t, q1)) {
					q1++
				}
			}
		}
		if q1 > q0 {
			if colon >= 0 {
				for amax = colon + 1; amax < t.file.b.nc(); amax++ {
					if c := a.textreadc(t, amax); c == ' ' || c == '\t' || c == '\n' {
						break
					}
				}
			} else {
				amax = t.file.b.nc()
			}
		}
		if colon != q0 {
			reverse = false
		}
	} else if reverse {
		if a.textreadc(t, q0) != ':' {
			reverse = false
		}
	}
	amin := amax
	e.q0 = q0
	e.q1 = q1
	n := q1 - q0
	if n == 0 {
		return false
	}
	r := make([]rune, n)
	t.file.b.Read(q0, r)
	if runeHasPrefix(r, lhttp) || runeHasPrefix(r, lhttps) {
		if r[n-1] == '.' {
			e.q1--
			n--
		}
		e.name = r[:n]
		e.at = t
		e.a0 = e.q1
		e.a1 = e.q1
		return true
	}
	nname := -1
	for i := 0; i < n; i++ {
		if r[i] == ':' && nname < 0 {
			if q0+i+1 < t.file.b.nc() && (i == n-1 || isaddrc(a.textreadc(t, q0+i+1))) {
				amin = q0 + i
			} else {
				return false
			}
			nname = i
		}
	}
	if nname == -1 {
		nname = n
	}
	for i := 0; i < nname; i++ {
		if !isfilec(r[i]) && r[i] != ' ' {
			return false
		}
	}
	switch {
	case q0 > 0 && a.textreadc(t, q0-1) == '<' && q1 < t.file.b.nc() && a.textreadc(t, q1) == '>':
		r = a.includename(t, r[:nname])
		nname = len(r)
	case amin == q0:
		// already a file
	default:
		r = a.dirname(t, r[:nname])
		nname = len(r)
	}
	e.bname = string(r[:nname])
	if w := a.lookfile(r[:nname]); w != nil {
		return a.expandIsFile(t, e, r[:nname], amin, amax, reverse)
	}
	if a.ismtpt(e.bname) {
		// untested: ismtpt always returns false in this port (no mount table)
		e.bname = ""
		return false
	}
	if _, err := os.Stat(e.bname); err != nil {
		e.bname = ""
		return false
	}
	return a.expandIsFile(t, e, r[:nname], amin, amax, reverse)
}

func (a *Acme) expandIsFile(t *Text, e *Expand, name []rune, amin, amax int, reverse bool) bool {
	e.name = name
	e.at = t
	e.a0 = amin + 1
	e.reverse = reverse
	eval := false
	a.address(true, nil, Range{-1, -1}, Range{0, 0}, a.tgetcOf(t), e.a0, amax, &eval, &e.a1, e.reverse)
	return true
}

// expand expands the click at [q0,q1) to a word or file (look.c expand).
func (a *Acme) expand(t *Text, q0, q1 int, e *Expand, reverse bool) bool {
	*e = Expand{jump: true}
	if q1 == q0 && t.q1 > t.q0 && t.q0 <= q0 && q0 <= t.q1 {
		q0 = t.q0
		q1 = t.q1
		if t.what == Tag {
			e.jump = false
		}
	}
	if a.expandfile(t, q0, q1, e, reverse) {
		return true
	}
	if q0 == q1 {
		for q1 < t.file.b.nc() && isalnum(a.textreadc(t, q1)) {
			q1++
		}
		for q0 > 0 && isalnum(a.textreadc(t, q0-1)) {
			q0--
		}
	}
	e.q0 = q0
	e.q1 = q1
	return q1 > q0
}

func runeHasPrefix(r, p []rune) bool {
	if len(r) < len(p) {
		return false
	}
	for i, c := range p {
		if r[i] != c {
			return false
		}
	}
	return true
}

// look3 implements a button-3 action (look.c look3): expand, report to a
// watching event reader, otherwise search for the text or open the file.
func (a *Acme) look3(t *Text, q0, q1 int, external, reverse bool) {
	ct := a.seltext
	if ct == nil {
		a.seltext = t
	}
	var e Expand
	expanded := a.expand(t, q0, q1, &e, reverse)
	if !external && t.w != nil && t.w.nopen[qWevent] > 0 {
		if !expanded {
			// untested: a non-expanding click queues no event, so the following
			// read would block the C oracle -- not differentiable
			// untested: live plumber relay / expand edge path; the -D suite has no plumber
			return
		}
		f := 0
		if (e.at != nil && t.w != nil) || (len(e.name) > 0 && a.lookfile(e.name) != nil) {
			f = 1
		}
		if q0 != e.q0 || q1 != e.q1 {
			f |= 2
		}
		if len(e.name) > 0 {
			f |= 4
		}
		c := byte('l')
		if t.what == Body {
			c = 'L'
		}
		if reverse {
			c += 'R' - 'L'
		}
		a.lookevent(t, c, q0, q1, f)
		if q0 == e.q0 && q1 == e.q1 {
			return
		}
		var r []rune
		if len(e.name) > 0 {
			r = append(r, e.name...)
			if e.a1 > e.a0 {
				r = append(r, ':')
				m := make([]rune, e.a1-e.a0)
				e.at.file.b.Read(e.a0, m)
				r = append(r, m...)
			}
		} else {
			r = make([]rune, e.q1-e.q0)
			t.file.b.Read(e.q0, r)
		}
		f &^= 2
		a.lookeventText(t, c, e.q0, e.q1, f, r)
		return
	}
	// If a plumber is connected, send the click to it; if it accepts the message
	// the plumber routes it (often back to us as a showfile, opening the file),
	// so we are done. Only fall through to the built-in interpretation when no
	// plumber is connected or the send fails (look.c look3 plumbsendfid branch).
	if a.plumbsendfid != nil {
		// untested: live plumber relay / expand edge path; the -D suite has no plumber
		if m, ok := a.plumbword(t, q0, q1); ok {
			// untested: live plumber relay / expand edge path; the -D suite has no plumber
			if len(m.Data) < messagesize-1024 {
				// untested: live plumber relay / expand edge path; the -D suite has no plumber
				if err := m.Send(a.plumbsendfid); err == nil {
					// untested: live plumber relay / expand edge path; the -D suite has no plumber
					return
				}
			}
		} else {
			// untested: live plumber relay / expand edge path; the -D suite has no plumber
			return // the click expanded to nothing (C: plumbfree + Return)
		}
	}
	// no plumber (or it declined): interpret the string ourselves.
	if !expanded {
		return
	}
	if e.name != nil || e.at != nil {
		a.openfile(t, &e)
		return
	}
	if t.w == nil {
		// untested: -D always looks from a windowed text
		return
	}
	ct = &t.w.body
	if t == ct {
		q := e.q1
		if reverse {
			q = e.q0
		}
		a.textsetselect(ct, q, q)
	}
	r := make([]rune, e.q1-e.q0)
	t.file.b.Read(e.q0, r)
	if a.search(ct, r, reverse) && e.jump {
		a.moveto(ct.fr.PointOf(ct.fr.P0).Add(draw.Pt(4, ct.fr.Font.Height-4)))
	}
}

// lookevent emits a look event whose text is the buffer at [q0,q1) (the first,
// pre-expansion message); lookeventText emits one whose text is the explicit r
// (the resolved name/expansion -- the second message).
func (a *Acme) lookevent(t *Text, c byte, q0, q1, f int) {
	a.execevent(t, c, q0, q1, f)
}

func (a *Acme) lookeventText(t *Text, c byte, q0, q1, f int, r []rune) {
	if len(r) <= EVENTSIZE {
		a.winappend(t.w, fmt.Sprintf("%c%d %d %d %d %s\n", c, q0, q1, f, len(r), string(r)))
	} else {
		// untested: a looked name longer than 256 runes
		a.winappend(t.w, fmt.Sprintf("%c%d %d %d 0 \n", c, q0, q1, f))
	}
}

// search finds the runes r in ct, forward or reverse, wrapping once (look.c).
func (a *Acme) search(ct *Text, r []rune, reverse bool) bool {
	// look3 always searches for the word it just expanded, which is therefore
	// present in the buffer; so search never fails to match and the various
	// no-match / full-wrap exits below are not reachable through -D.
	n := len(r)
	nc := ct.file.b.nc()
	if n == 0 || n > nc {
		// untested: the expanded look string is non-empty and fits the buffer
		return false
	}
	if 2*n > RBUFSIZE {
		// untested: -D never searches for a string longer than RBUFSIZE/2 (~4K)
		a.warning(nil, "string too long\n")
		return false
	}
	if reverse {
		q1 := ct.q0
		around := false
		for {
			if q1 <= 0 {
				q1 = nc
				around = true
			}
			start := q1 - n
			if start < 0 {
				// untested: reverse-searching a word closer to the start than its
				// own length is undefined in the C oracle (its sliding-buffer
				// search reads before the buffer), so it is not differize-tested
				// untested: live plumber relay / expand edge path; the -D suite has no plumber
				if around {
					// untested
					break
				}
				// untested
				q1--
				continue
			}
			m := make([]rune, n)
			ct.file.b.Read(start, m)
			if runeEqN(m, r) {
				a.searchHit(ct, q1-n, q1)
				return true
			}
			q1--
			if around && q1 <= 0 {
				// untested: no-match full wrap
				break
			}
		}
	} else {
		q := ct.q1
		around := false
		for {
			if q > nc-n {
				q = 0
				around = true
			}
			m := make([]rune, n)
			ct.file.b.Read(q, m)
			if runeEqN(m, r) {
				a.searchHit(ct, q, q+n)
				return true
			}
			q++
			if around && q > ct.q1-n {
				// untested: no-match full wrap
				break
			}
		}
	}
	// untested: unreachable (a match is always found, see above)
	return false
}

func (a *Acme) searchHit(ct *Text, q0, q1 int) {
	if ct.w != nil {
		a.textshow(ct, q0, q1, true)
		a.winsettag(ct.w)
	} else {
		// untested: look3 always searches the window body, so ct.w != nil
		ct.q0 = q0
		ct.q1 = q1
	}
	a.seltext = ct
}

func runeEqN(a, b []rune) bool {
	if len(a) != len(b) {
		// untested: callers always pass equal-length slices
		return false
	}
	for i := range a {
		if a[i] != b[i] {
			return false
		}
	}
	return true
}

// openfile opens (or finds) the file named by e and shows its address (look.c).
func (a *Acme) openfile(t *Text, e *Expand) *Window {
	var r Range
	var w *Window
	if len(e.name) == 0 {
		w = t.w
		if w == nil {
			// untested: look3 always opens from a windowed text
			return nil
		}
	} else {
		w = a.lookfile(e.name)
		if w == nil && e.name[0] != '/' {
			// pwd-relative -> absolute against the startup dir: look.c openfile
			// uses the wdir global, not the live cwd, so a later Cd does not change
			// how an unrooted Look path resolves.
			e.name = cleanrname([]rune(a.wdir + "/" + string(e.name)))
			w = a.lookfile(e.name)
		}
	}
	if w != nil {
		t = &w.body
		if !t.col.safe && t.fr.MaxLines == 0 {
			// untested: re-opening a window obscured by a full-column window
			a.colgrow(t.col, t.col.w[0], 1)
		}
	} else {
		var ow *Window
		if t != nil {
			ow = t.w
		}
		w = a.makenewwindow(t)
		t = &w.body
		a.winsetname(w, e.name)
		if a.textload(t, 0, e.bname, true) >= 0 {
			t.file.unread = false
		}
		t.file.mod = false
		t.w.dirty = false
		a.winsettag(t.w)
		a.textsetselect(&t.w.tag, t.w.tag.file.b.nc(), t.w.tag.file.b.nc())
		if ow != nil {
			w.autoindent = ow.autoindent
		} else {
			// untested: look3 always opens from an existing window (ow != nil)
			w.autoindent = a.globalautoindent
		}
		a.xfidlog(w, "new")
	}
	eval := false
	if e.a1 == e.a0 {
		// untested: openfile is reached only with a non-empty address here
		eval = false
	} else {
		eval = true
		var dummy int
		// The address expression is normally read from e.at (the in-file
		// address); a plumb message instead supplies it as a separate string via
		// e.agetc (C's e.u.ar + plumbgetc).
		getc := a.tgetcOf(e.at)
		if e.agetc != nil {
			getc = e.agetc
		}
		r = a.address(true, t, Range{-1, -1}, Range{t.q0, t.q1}, getc, e.a0, e.a1, &eval, &dummy, e.reverse)
		if r.q0 > r.q1 {
			// untested: an out-of-order address is not exercised
			eval = false
			a.warning(nil, "addresses out of order\n")
		}
		if !eval {
			// untested: a failing address eval is not exercised
			e.jump = false
		}
	}
	if !eval {
		r.q0 = t.q0
		r.q1 = t.q1
	}
	a.textshow(t, r.q0, r.q1, true)
	a.winsettag(t.w)
	a.seltext = t
	if e.jump {
		a.moveto(t.fr.PointOf(t.fr.P0).Add(draw.Pt(4, a.font.Height-4)))
	}
	return w
}

// startplumbing connects to the plumber and starts plumbthread (look.c
// startplumbing). cplumb relays received messages to mousethread. Called from
// the live threadmain; if no plumber is running, plumbthread keeps retrying.
func (a *Acme) startplumbing() {
	a.cplumb = make(chan *plumb.Message)
	go a.plumbthread()
}

// plumbthread connects to the plumber's edit port and relays each received
// message onto cplumb, reconnecting if the plumber restarts (look.c
// plumbthread). The send port is recorded for look3's outgoing plumb.
func (a *Acme) plumbthread() {
	for {
		fs, err := plumb.Mount()
		if err != nil {
			time.Sleep(2 * time.Second) // C waits on a 2s timer and retries
			continue
		}
		// untested: live plumber relay / expand edge path; the -D suite has no plumber
		edit, err := fs.Open("edit", plan9.OREAD)
		if err != nil {
			// untested: live plumber relay / expand edge path; the -D suite has no plumber
			time.Sleep(2 * time.Second)
			continue
		}
		// untested: live plumber relay / expand edge path; the -D suite has no plumber
		send, err := fs.Open("send", plan9.OWRITE)
		if err == nil {
			// untested: live plumber relay / expand edge path; the -D suite has no plumber
			a.plumbsendfid = send
		}
		// untested: live plumber relay / expand edge path; the -D suite has no plumber
		br := bufio.NewReader(edit)
		for {
			// untested: live plumber relay / expand edge path; the -D suite has no plumber
			m := new(plumb.Message)
			if err := m.Recv(br); err != nil {
				// untested: live plumber relay / expand edge path; the -D suite has no plumber
				break // lost connection; reconnect
			}
			// untested: live plumber relay / expand edge path; the -D suite has no plumber
			a.cplumb <- m
		}
		// untested: live plumber relay / expand edge path; the -D suite has no plumber
		a.plumbsendfid = nil
		edit.Close()
	}
}

// plumblook handles a received "look"/"showfile" plumb message: open the named
// file (look.c plumblook) and, if the message carries an "addr" attribute, jump
// to that address via the Expand agetc machinery (C's e.u.ar/plumbgetc), which
// openfile evaluates against the opened file.
func (a *Acme) plumblook(m *plumb.Message) {
	if len(m.Data) >= BUFSIZE {
		// untested: live plumber relay / expand edge path; the -D suite has no plumber
		a.warning(nil, "insanely long file name (%d bytes) in plumb message (%.32s...)\n", len(m.Data), string(m.Data))
		return
	}
	if len(m.Data) == 0 {
		// untested: live plumber relay / expand edge path; the -D suite has no plumber
		return
	}
	var e Expand
	e.bname = string(m.Data)
	e.name = []rune(e.bname)
	e.jump = true
	// An "addr" attribute is an address to evaluate against the opened file (C
	// plumblook sets e.u.ar + e.agetc=plumbgetc); openfile then jumps there.
	if addr := m.LookupAttr("addr"); addr != "" {
		ar := []rune(addr)
		e.agetc = func(n int) int {
			if n >= len(ar) {
				// untested: live plumber relay / expand edge path; the -D suite has no plumber
				return 0
			}
			return int(ar[n])
		}
		e.a0 = 0
		e.a1 = len(ar)
	}
	a.drawtopwindow()
	a.openfile(nil, &e)
}

// plumbshow handles a received "showdata" plumb message: make a new window named
// by the filename attribute (or Untitled-N) and insert the message data (look.c
// plumbshow).
func (a *Acme) plumbshow(m *plumb.Message) {
	a.drawtopwindow()
	w := a.makenewwindow(nil)
	name := m.LookupAttr("filename")
	if name == "" {
		// untested: live plumber relay / expand edge path; the -D suite has no plumber
		a.nuntitled++
		name = fmt.Sprintf("Untitled-%d", a.nuntitled)
	}
	if name[0] != '/' && m.Dir != "" {
		// untested: live plumber relay / expand edge path; the -D suite has no plumber
		name = m.Dir + "/" + name
	}
	rs := cleanrname([]rune(name))
	a.winsetname(w, rs)
	var r []rune
	var nulls bool
	cvttorunes(m.Data, len(m.Data), &r, &nulls)
	a.textinsert(&w.body, 0, r, true)
	w.body.file.mod = false
	w.dirty = false
	a.winsettag(w)
	a.textscrdraw(&w.body)
	a.textsetselect(&w.tag, w.tag.file.b.nc(), w.tag.file.b.nc())
	a.xfidlog(w, "new")
}

// drawtopwindow raises acme's window to the top of the host window stack before
// a plumbed file is shown (look.c calls drawtopwindow in plumblook/plumbshow;
// C drawtopwindow -> _displaytop). The go/draw analogue is Display.Top.
func (a *Acme) drawtopwindow() {
	// untested: reached only from the live plumber (plumblook/plumbshow); the -D
	// and in-process suites deliver no plumb messages.
	if a.disp != nil {
		a.disp.Top()
	}
}

// messagesize bounds an outgoing plumb message (C uses the plumber connection's
// negotiated msize; 8192 matches the usual 9P default and only excludes
// pathologically large selections).
const messagesize = 8192

// plumbword builds the plumb message for a button-3 click at [q0,q1) in t: the
// selection, or -- for a bare click -- the surrounding whitespace-delimited
// word tagged with a click=N attribute, sent as "text" with the window's
// directory as wdir (look.c look3's plumbsendfid branch). Returns false if a
// bare click expands to no word.
func (a *Acme) plumbword(t *Text, q0, q1 int) (*plumb.Message, bool) {
	m := &plumb.Message{Src: "acme", Type: "text"}
	dir := a.dirname(t, nil)
	if len(dir) == 1 && dir[0] == '.' { // sigh (look.c)
		// untested: live plumber relay / expand edge path; the -D suite has no plumber
		dir = nil
	}
	if len(dir) == 0 {
		m.Dir = a.wdir
	} else {
		// untested: live plumber relay / expand edge path; the -D suite has no plumber
		m.Dir = string(dir)
	}
	if q1 == q0 {
		if t.q1 > t.q0 && t.q0 <= q0 && q0 <= t.q1 {
			// untested: live plumber relay / expand edge path; the -D suite has no plumber
			q0 = t.q0
			q1 = t.q1
		} else {
			p := q0
			for q0 > 0 {
				if c := a.textreadc(t, q0-1); c == ' ' || c == '\t' || c == '\n' {
					// untested: live plumber relay / expand edge path; the -D suite has no plumber
					break
				}
				q0--
			}
			for q1 < t.file.b.nc() {
				if c := a.textreadc(t, q1); c == ' ' || c == '\t' || c == '\n' {
					break
				}
				q1++
			}
			if q1 == q0 {
				// untested: live plumber relay / expand edge path; the -D suite has no plumber
				return nil, false
			}
			m.Attr = &plumb.Attribute{Name: "click", Value: fmt.Sprintf("%d", p-q0)}
		}
	}
	data := make([]rune, q1-q0)
	t.file.b.Read(q0, data)
	m.Data = []byte(string(data))
	return m, true
}
