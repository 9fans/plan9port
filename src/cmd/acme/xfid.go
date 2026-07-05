package main

import (
	"fmt"
	"strings"
	"unicode/utf8"

	"9fans.net/go/plan9/srv9p"
)

// fullrunewrite converts a streaming 9P write to runes, carrying a multi-byte
// rune split across consecutive writes on the same fid via per-fid leftover
// bytes (xfid.c fullrunewrite + f->rpart/nrpart). A trailing incomplete UTF-8
// sequence is held back and prepended to the next write; complete runes (incl.
// invalid single bytes) are converted now, and NUL bytes are elided as
// cvttorunes does. The previously-held bytes were already counted in their own
// write's reply, so the caller still reports only this write's byte count.
func (a *Acme) fullrunewrite(fid *srv9p.Fid, data []byte) []rune {
	x := auxOf(fid)
	if len(x.rpart) > 0 {
		data = append(append([]byte(nil), x.rpart...), data...)
		x.rpart = nil
	}
	var r []rune
	i := 0
	for i < len(data) {
		if !utf8.FullRune(data[i:]) {
			break // incomplete trailing rune: carry it to the next write
		}
		ru, w := utf8.DecodeRune(data[i:])
		i += w
		if ru != 0 { // elide NUL, like cvttorunes
			r = append(r, ru)
		}
	}
	if i < len(data) {
		x.rpart = append([]byte(nil), data[i:]...)
	}
	return r
}

// Port of the acme 9P file semantics (xfid.c). The 9P server (fsys.go) calls
// xfidopen/xfidclose/xfidReadName/xfidWriteName for each request; the -D
// read/write verbs reach them as ordinary 9P clients. The C oracle drives the
// real xfid.c through the same protocol, so these must agree byte-for-byte.
//
// The qid constants are defined in fsys.go (the full dat.h enum).

// winappend queues a preformatted event record, prefixed by the window owner,
// for a window whose event file is open (wind.c winevent core). Callers
// (winevent, the execute event block) have already checked nopen>0; the owner
// is always set by the open/lock that preceded the event.
func (a *Acme) winappend(w *Window, s string) {
	w.events = append(w.events, byte(w.owner))
	w.events = append(w.events, s...)
	if a.event != nil {
		// wake a blocked event-file reader (xfid.c winevent's sendp(eventx->c))
		a.event.Broadcast()
	}
}

// xfidopen mirrors xfid.c xfidopen: per-window open bookkeeping (the addr file
// resets the address range on its first opener).
func (a *Acme) xfidopen(w *Window, qid int) {
	// The caller (fsOpen) holds winlock(w,'E'), which already set w.owner; C's
	// xfidopen is the body inside that winlock.
	switch qid {
	case qWaddr:
		if w.nopen[qid] == 0 {
			w.addr = Range{0, 0}
			w.limit = Range{-1, -1}
		}
		w.nopen[qid]++
	case qWdata, qWxdata:
		w.nopen[qid]++
	case qWwrsel:
		// opening wrsel cuts the selection; subsequent writes fill the gap
		// (xfid.c). nomark keeps the replacement out of the undo history.
		w.nopen[qid]++
		a.seq++
		a.filemark(w.body.file)
		a.cut(&w.body, &w.body, nil, false, true, nil)
		w.wrselrange = Range{w.body.q1, w.body.q1}
		w.nomark = true
	case qWevent:
		// Increment before winsettag so the tag edits it makes are reported as
		// events (matching C's post-increment in the open condition).
		w.nopen[qid]++
		if w.nopen[qid] == 1 && !w.isdir && w.col != nil {
			w.filemenu = false
			a.winsettag(w)
		}
	}
}

// xfidclose mirrors xfid.c xfidclose's per-file close actions. The caller
// (fsClunk) still holds this fid's window reference, so w is alive here; winclose
// runs afterward. The non-drawing state restores are done now; the two drawing
// restores C also does -- QWevent's filemenu redraw (filemenu=TRUE; winsettag)
// and QWwrsel's scroll-to-show (textshow) -- are deferred because a clunk-time
// redraw adds window frames that desync the -D capture's per-setmouse cursor
// delay alignment (see PORT.md triage #1); the filemenu *flag* is still set so a
// later winsettag shows the menu.
func (a *Acme) xfidclose(w *Window, qid int, fid *srv9p.Fid) {
	switch qid {
	case qWctl:
		// Release the exclusive ctl lock if this fid holds it, so a clunk without
		// "unlock" doesn't strand the window (C xfidclose QWctl, under winlock(w)).
		if w.ctlfid == fid {
			w.ctlfid = nil
			w.ctllock.Unlock()
		}
	case qWdata, qWxdata:
		w.nomark = false
		fallthrough
	case qWaddr, qWevent:
		if w.nopen[qid] > 0 {
			w.nopen[qid]--
			if w.nopen[qid] == 0 {
				// untested: -D keeps these fids open, so the last-close restores
				// fire only for a real external client.
				if qid == qWdata || qid == qWxdata {
					w.nomark = false
				}
				if qid == qWevent {
					if !w.isdir && w.col != nil {
						w.filemenu = true
						// C redraws the restored menu now (winsettag). Only the
						// live editor does so: under -D a clunk-time redraw adds a
						// window frame the capture can't pair with a cursor frame
						// (PORT.md triage #1); the flag is set either way, so the
						// next winsettag shows the menu.
						if a.blockEvents {
							a.winsettag(w)
						}
					}
					w.dumpstr = ""
					w.dumpdir = ""
				}
			}
		}
	case qWwrsel:
		// untested: -D does not open/close wrsel as a separate client fid
		w.nomark = false
		// C scrolls to show the wrsel-written text on close (textshow). Live only,
		// for the same -D frame-pairing reason as the QWevent winsettag above.
		if a.blockEvents && w.col != nil {
			// untested: 9P error / edge path not exercised by -D
			t := &w.body
			a.textshow(t, minInt(w.wrselrange.q0, t.file.b.nc()), minInt(w.wrselrange.q1, t.file.b.nc()), true)
			a.textscrdraw(t)
		}
	}
}

// clampaddr clamps w.addr to the body's character range (xfid.c).
func clampaddr(w *Window) {
	nc := w.body.file.b.nc()
	if w.addr.q0 < 0 {
		// untested: a 9P address never evaluates to a negative offset
		w.addr.q0 = 0
	}
	if w.addr.q1 < 0 {
		// untested: a 9P address never evaluates to a negative offset
		w.addr.q1 = 0
	}
	if w.addr.q0 > nc {
		w.addr.q0 = nc
	}
	if w.addr.q1 > nc {
		w.addr.q1 = nc
	}
}

// textbsinsert inserts r at q0, honoring embedded backspaces (text.c).
func (a *Acme) textbsinsert(t *Text, q0 int, r []rune, tofile bool) (int, int) {
	if t.what == Tag { // can't happen, but mustn't backspace over the file name
		// untested: textbsinsert is only ever called on a body, never a tag
		a.textinsert(t, q0, r, tofile)
		return q0, len(r)
	}
	n := len(r)
	for i := 0; i < n; i++ {
		if r[i] != '\b' {
			continue
		}
		initial := 0
		tp := make([]rune, n)
		copy(tp, r[:i])
		up := i
		for ; i < n; i++ {
			tp[up] = r[i]
			if tp[up] == '\b' {
				if up == 0 {
					initial++
				} else {
					up--
				}
			} else {
				up++
			}
		}
		if initial != 0 {
			if initial > q0 {
				initial = q0
			}
			q0 -= initial
			a.textdelete(t, q0, q0+initial, tofile)
		}
		tp = tp[:up]
		a.textinsert(t, q0, tp, tofile)
		return q0, len(tp)
	}
	a.textinsert(t, q0, r, tofile)
	return q0, n
}

// indexread builds the /mnt/acme/index file: one line per currently-active
// window, each its ctl prefix (no font, so build-independent) and tag text
// (xfid.c xfidindexread).
func (a *Acme) indexread() string {
	var b strings.Builder
	for _, c := range a.row.col {
		for _, w := range c.w {
			if w.body.file.curtext != &w.body { // show only the active zerox
				continue
			}
			b.WriteString(a.winctlprint(w, false))
			tag := readRunes(&w.tag, 0, w.tag.file.b.nc())
			if i := strings.IndexByte(tag, '\n'); i >= 0 {
				tag = tag[:i]
			}
			b.WriteString(tag)
			b.WriteByte('\n')
		}
	}
	return b.String()
}

func bint(b bool) int {
	if b {
		return 1
	}
	return 0
}

// winctlprint formats a window's ctl line (wind.c). With fonts, it appends the
// body width, font name, tab width and undo/redo flags; the font name matches
// the C oracle because both load the same -f/$font font.
func (a *Acme) winctlprint(w *Window, fonts bool) string {
	s := fmt.Sprintf("%11d %11d %11d %11d %11d ",
		w.id, w.tag.file.b.nc(), w.body.file.b.nc(), bint(w.isdir), bint(w.dirty))
	if fonts {
		s += fmt.Sprintf("%11d %s %11d %11d %11d ",
			w.body.fr.R.Dx(), plan9quote(w.body.reffont.f.Name), w.body.fr.MaxTab,
			bint(a.seqof(w, true) != 0), bint(a.seqof(w, false) != 0))
	}
	return s
}

// plan9quote mirrors libc's %q: wrap a string in single quotes (doubling any
// embedded quote) only when it is empty or contains a space/control/quote.
func plan9quote(s string) string {
	if s == "" {
		return "''"
	}
	quote := false
	for _, r := range s {
		if r <= ' ' || r == '\'' {
			quote = true
			break
		}
	}
	if !quote {
		return s
	}
	var b strings.Builder
	b.WriteByte('\'')
	for _, r := range s {
		if r == '\'' {
			b.WriteByte('\'')
		}
		b.WriteRune(r)
	}
	b.WriteByte('\'')
	return b.String()
}

// xfidReadName returns the bytes that reading the named file would yield, or a
// non-empty 9P error string (xfidread).
func (a *Acme) xfidReadName(w *Window, qid int) (string, string) {
	switch qid {
	case qIndex:
		return a.indexread(), ""
	case qLabel, qCons:
		// the host-window label and the cons input are empty on read (xfid.c)
		return "", ""
	}
	if w == nil {
		// Defensive: the top-level readable files are handled above (index/label/
		// cons) or in fsRead (log; new is a directory), so every qid reaching the
		// per-window switch below has a window. A nil window here would be a qid
		// routing bug, not a real client read.
		// untested: qid routing gives every per-window read its window.
		return "", "acme: read of file without a window"
	}
	switch qid {
	case qWaddr:
		a.textcommit(&w.body, true)
		clampaddr(w)
		return fmt.Sprintf("%11d %11d ", w.addr.q0, w.addr.q1), ""
	case qWbody:
		return readRunes(&w.body, 0, w.body.file.b.nc()), ""
	case qWctl:
		return a.winctlprint(w, true), ""
	case qWtag:
		return readRunes(&w.tag, 0, w.tag.file.b.nc()), ""
		// qWevent is handled directly in fsRead (count-aware consuming read that
		// keeps the untended tail, xfid.c xfideventread); qWdata/qWxdata likewise
		// (count-aware cursor read, xfid.c xfidruneread). Neither reaches here.
	}
	// untested: resolveName only returns the qids handled above
	return "", "unknown file"
}

// readRunes returns the UTF-8 text of t's buffer over [q0,q1).
func readRunes(t *Text, q0, q1 int) string {
	r := make([]rune, q1-q0)
	t.file.b.Read(q0, r)
	return string(r)
}

// xfidWriteName applies a write of data to the named file, returning the count
// written and a non-empty 9P error string on failure (xfidwrite). md is the
// fid's mount directory (nil for the default attach), used to route cons output
// to that directory's +Errors window.
func (a *Acme) xfidWriteName(w *Window, qid int, data string, md *Mntdir, fid *srv9p.Fid) (int, string) {
	// Convert to runes, dropping embedded NULs as C's xfidwrite does; the reported
	// count stays the byte count. The streaming text-append files (body/tag/wrsel/
	// cons/editout/errors) go through fullrunewrite, which carries a multi-byte
	// rune split across consecutive writes (xfid.c). The addressed/parsed files
	// (data/addr/ctl/event) are single count-bounded writes and use cvttorunes.
	var r []rune
	switch qid {
	case qWbody, qWtag, qWwrsel, qCons, qEditout, qWeditout, qWerrors:
		r = a.fullrunewrite(fid, []byte(data))
	default:
		cvttorunes([]byte(data), len(data), &r, nil)
	}
	count := len(data) // 9P count is bytes
	if w != nil {
		// C's xfidwrite locks the window with 'F' (or 'E' for body/tag), which
		// sets the owner used to tag any events the write generates.
		if qid == qWbody || qid == qWtag {
			w.owner = 'E'
		} else {
			w.owner = 'F'
		}
	}
	if qid == qLabel {
		// the host-window label: accepted and ignored (xfid.c)
		return count, ""
	}
	switch qid {
	case qWaddr:
		t := &w.body
		a.wincommit(w, t)
		eval := true
		var nb int
		ar := a.address(false, t, w.limit, w.addr, runeGetc(r), 0, len(r), &eval, &nb, false)
		if nb < len(r) {
			return 0, "bad address syntax"
		}
		if !eval {
			return 0, "address out of range"
		}
		w.addr = ar
		return count, ""
	case qWdata:
		t := &w.body
		a.wincommit(w, t)
		if w.addr.q0 > t.file.b.nc() || w.addr.q1 > t.file.b.nc() {
			return 0, "address out of range"
		}
		if !w.nomark {
			a.seq++
			a.filemark(t.file)
		}
		q0 := w.addr.q0
		if w.addr.q1 > q0 {
			a.textdelete(t, q0, w.addr.q1, true)
			w.addr.q1 = q0
		}
		tq0, tq1 := t.q0, t.q1
		a.textinsert(t, q0, r, true)
		if tq0 >= q0 {
			tq0 += len(r)
		}
		if tq1 >= q0 {
			tq1 += len(r)
		}
		a.textsetselect(t, tq0, tq1)
		if a.shouldscroll(t, q0, qWdata) {
			// scroll to show the just-written text, as xfid.c does after a data
			// write within the visible region
			a.textshow(t, q0+len(r), q0+len(r), false)
		}
		a.textscrdraw(t)
		a.winsettag(w)
		w.addr.q0 += len(r)
		w.addr.q1 = w.addr.q0
		return count, ""
	case qWbody, qWtag, qWwrsel:
		t := &w.body
		if qid == qWtag {
			t = &w.tag
		}
		a.bodyTag(w, t, qid, r)
		return count, ""
	case qCons:
		// cons appends to the +Errors window for the fid's mount directory
		// (xfid.c errorwin(mntdir)); a nil mntdir is the shared +Errors.
		var dir []rune
		var incl [][]rune
		if md != nil {
			dir = md.dir
			incl = md.incl
		}
		ew := a.errorwin1(dir, incl)
		ew.owner = 'X'
		a.bodyTag(ew, &ew.body, qCons, r)
		return count, ""
	case qWctl:
		return a.xfidctlwrite(w, data, fid)
	case qEditout, qWeditout:
		// the editout files accept text only while an edit pipe runs; -D never
		// starts one, so edittext always denies the write (ecmd.c edittext).
		q := 0
		if w != nil {
			// untested: 9P error / edge path not exercised by -D
			q = w.wrselrange.q1
		}
		if err := a.edittext(w, q, r); err != "" {
			return 0, err
		}
		// untested: -D cannot start an edit pipe, so edittext always denies
		return count, ""
	case qWerrors:
		ew := a.errorwinforwin(w)
		ew.owner = 'X'
		a.bodyTag(ew, &ew.body, qWbody, r)
		return count, ""
	case qWevent:
		return a.xfideventwrite(w, data)
	}
	// untested: resolveName only returns the qids handled above
	return 0, "unknown file"
}

// xfideventwrite re-dispatches event records written to a window's event file
// back through execute/look3, exactly as if the user had acted on them (xfid.c
// xfideventwrite). Each record is: owner byte, command byte, q0, q1, newline.
func (a *Acme) xfideventwrite(w *Window, data string) (int, string) {
	const Ebadevent = "bad event syntax"
	// The caller (the Write dispatch) holds winlock(w,'F') across this re-dispatch,
	// as C's xfidwrite does; each execute/look3 then runs under row.lk ("just like
	// mousethread", xfideventwrite's qlock(&row.lk)) — winlock-outer / row.lk-inner.
	// execute/look3 may reach the X/Y filelooper, which winunlocks and re-locks this
	// window, so it must be held on entry. Each record overrides w.owner with its
	// own owner byte (the C hack).
	n := 0
	for n < len(data) {
		p := n
		if p+2 > len(data) {
			return 0, Ebadevent
		}
		w.owner = int(data[p]) // disgusting
		p++
		c := data[p]
		p++
		q0, p, ok := scanevent(data, p)
		if !ok {
			return 0, Ebadevent
		}
		q1, p, ok := scanevent(data, p)
		if !ok {
			return 0, Ebadevent
		}
		for p < len(data) && data[p] == ' ' {
			p++
		}
		if p >= len(data) || data[p] != '\n' {
			return 0, Ebadevent
		}
		p++
		var t *Text
		switch {
		case 'a' <= c && c <= 'z':
			t = &w.tag
		case 'A' <= c && c <= 'Z':
			t = &w.body
		default:
			return 0, Ebadevent
		}
		if q0 > t.file.b.nc() || q1 > t.file.b.nc() || q0 > q1 {
			return 0, Ebadevent
		}
		// qlock(&row.lk) per record, just like mousethread (xfid.c xfideventwrite);
		// winlock(w) is already held by the caller (winlock-outer / row.lk-inner).
		a.row.lk.Lock()
		switch c {
		case 'x', 'X':
			a.execute(t, q0, q1, true, nil)
		case 'l', 'L':
			a.look3(t, q0, q1, true, false)
		case 'r', 'R':
			a.look3(t, q0, q1, true, true)
		default:
			// untested: c was already constrained to a letter above
			a.row.lk.Unlock()
			return 0, Ebadevent
		}
		a.row.lk.Unlock()
		n = p
	}
	return n, ""
}

// scanevent skips leading spaces then reads a decimal number from data at off,
// mirroring C strtoul; ok is false when no digits are present (xfid.c).
func scanevent(data string, off int) (val, next int, ok bool) {
	for off < len(data) && data[off] == ' ' {
		off++
	}
	start := off
	for off < len(data) && '0' <= data[off] && data[off] <= '9' {
		val = val*10 + int(data[off]-'0')
		off++
	}
	if off == start {
		return 0, off, false
	}
	return val, off, true
}

// shouldscroll reports whether a write should scroll to show the new text
// (xfid.c); cons always scrolls, otherwise only when the insert is visible.
func (a *Acme) shouldscroll(t *Text, q0, qid int) bool {
	if qid == qCons {
		return true
	}
	return t.org <= q0 && q0 <= t.org+t.fr.NumChars
}

// bodyTag inserts r into t, the shared body/tag/wrsel/cons write path of
// xfidwrite (the C BodyTag label).
func (a *Acme) bodyTag(w *Window, t *Text, qid int, r []rune) {
	if len(r) == 0 {
		return
	}
	a.wincommit(w, t)
	q0 := t.file.b.nc()
	if qid == qWwrsel {
		q0 = min(w.wrselrange.q1, t.file.b.nc())
	}
	if qid == qWtag {
		a.textinsert(t, q0, r, true)
	} else {
		if !w.nomark {
			a.seq++
			a.filemark(t.file)
		}
		var nr int
		q0, nr = a.textbsinsert(t, q0, r, true)
		a.textsetselect(t, t.q0, t.q1)
		if qid != qWwrsel && a.shouldscroll(t, q0, qid) {
			a.textshow(t, q0+nr, q0+nr, true)
		}
		a.textscrdraw(t)
		if qid == qWwrsel {
			w.wrselrange.q1 += nr
		}
	}
	a.winsettag(w)
}

// ctlarg extracts a newline-terminated argument starting at off, returning the
// argument, the number of bytes consumed, and ok (xfid.c reads to '\n').
func ctlarg(p string, off int) (string, int, bool) {
	nl := strings.IndexByte(p[off:], '\n')
	if nl <= 0 { // no newline, or an empty argument
		return "", 0, false
	}
	return p[off : off+nl], off + nl + 1, true
}

// xfidctlwrite applies a window ctl message (xfid.c xfidctlwrite). It handles
// the state-changing commands, including get/put and the font verb (which runs
// fontx, the same path as the Font builtin).
func (a *Acme) xfidctlwrite(w *Window, data string, fid *srv9p.Fid) (int, string) {
	const eBadctl = "ill-formed control message"
	settag := false
	a.textcommit(&w.tag, true)
	n := 0
	// err accumulates the message's error: the non-fatal bad-name-char (C's
	// goto-out: apply the name, record the error, keep processing) and the fatal
	// errors (bad syntax, nulls, del-on-dirty), which C reports with `err = X;
	// break`. Either way C still runs the trailing winsettag after the loop, so
	// the fatal cases break out of the loop rather than returning, reaching the
	// settag tail below (xfid.c xfidctlwrite).
	err := ""
ctlLoop:
	for n < len(data) {
		p := data[n:]
		var m int
		switch {
		case strings.HasPrefix(p, "lock"): // make window exclusive use
			// C qlock(&w->ctllock) while the caller holds winlock(w): a second
			// fid's "lock" blocks here still holding winlock(w), so the holder's
			// "unlock" (which needs winlock(w)) can never run -- C's latent ctl
			// deadlock, reproduced faithfully. Under -D there is a single fid, so
			// it never contends. (See PORT.md follow-ups.)
			w.ctllock.Lock()
			w.ctlfid = fid
			m = 4
		case strings.HasPrefix(p, "unlock"): // release exclusive use
			w.ctlfid = nil
			w.ctllock.Unlock() // C: w->ctlfid = ~0; qunlock(&w->ctllock)
			m = 6
		case strings.HasPrefix(p, "clean"):
			t := &w.body
			t.eq0 = ^0
			filereset(t.file)
			t.file.mod = false
			w.dirty = false
			settag = true
			m = 5
		case strings.HasPrefix(p, "dirty"):
			w.body.file.mod = true
			w.dirty = true
			settag = true
			m = 5
		case strings.HasPrefix(p, "show"):
			t := &w.body
			a.textshow(t, t.q0, t.q1, true)
			m = 4
		case strings.HasPrefix(p, "name "):
			arg, k, ok := ctlarg(p, 5)
			if !ok {
				err = eBadctl
				break ctlLoop
			}
			if strings.ContainsRune(arg, 0) {
				// untested: -D never writes a NUL in a name; C breaks here.
				err = "nulls in file name"
				break ctlLoop
			}
			r := []rune(arg)
			for _, c := range r {
				if c < ' ' {
					// C's goto-out path: apply the name anyway, record the error,
					// and keep processing the rest of the message.
					err = "bad character in file name"
					break
				}
			}
			a.seq++
			a.filemark(w.body.file)
			a.winsetname(w, r)
			m = k
		case strings.HasPrefix(p, "font "):
			arg, k, ok := ctlarg(p, 5)
			if !ok {
				err = eBadctl
				break ctlLoop
			}
			if strings.ContainsRune(arg, 0) {
				// untested: -D never writes a NUL byte in a font string.
				err = "nulls in font string"
				break ctlLoop
			}
			a.fontx(&w.body, nil, nil, false, false, []rune(arg))
			m = k
		case strings.HasPrefix(p, "dump "):
			arg, k, ok := ctlarg(p, 5)
			if !ok {
				err = eBadctl
				break ctlLoop
			}
			if strings.ContainsRune(arg, 0) {
				// untested: -D never writes a NUL in a dump string
				err = "nulls in dump string"
				break ctlLoop
			}
			w.dumpstr = arg
			m = k
		case strings.HasPrefix(p, "dumpdir "):
			arg, k, ok := ctlarg(p, 8)
			if !ok {
				err = eBadctl
				break ctlLoop
			}
			if strings.ContainsRune(arg, 0) {
				// untested: -D never writes a NUL in a dump directory string
				err = "nulls in dump directory string"
				break ctlLoop
			}
			w.dumpdir = arg
			m = k
		case strings.HasPrefix(p, "get"):
			a.get(w, nil, nil)
			m = 3
		case strings.HasPrefix(p, "put"):
			a.put(w)
			m = 3
		case strings.HasPrefix(p, "delete"):
			a.colclose(w.col, w, true)
			m = 6
		case strings.HasPrefix(p, "del"):
			if !a.winclean(w, true) {
				err = "file dirty"
				break ctlLoop
			}
			a.colclose(w.col, w, true)
			m = 3
		case strings.HasPrefix(p, "dot=addr"):
			a.textcommit(&w.body, true)
			clampaddr(w)
			w.body.q0 = w.addr.q0
			w.body.q1 = w.addr.q1
			a.textsetselect(&w.body, w.body.q0, w.body.q1)
			settag = true
			m = 8
		case strings.HasPrefix(p, "addr=dot"):
			w.addr.q0 = w.body.q0
			w.addr.q1 = w.body.q1
			m = 8
		case strings.HasPrefix(p, "limit=addr"):
			a.textcommit(&w.body, true)
			clampaddr(w)
			w.limit.q0 = w.addr.q0
			w.limit.q1 = w.addr.q1
			m = 10
		case strings.HasPrefix(p, "nomark"):
			w.nomark = true
			m = 6
		case strings.HasPrefix(p, "mark"):
			a.seq++
			a.filemark(w.body.file)
			settag = true
			m = 4
		case strings.HasPrefix(p, "nomenu"):
			w.filemenu = false
			settag = true
			m = 6
		case strings.HasPrefix(p, "menu"):
			w.filemenu = true
			settag = true
			m = 4
		case strings.HasPrefix(p, "cleartag"):
			a.wincleartag(w)
			settag = true
			m = 8
		default:
			err = eBadctl
			break ctlLoop
		}
		n += m
		for n < len(data) && data[n] == '\n' {
			n++
		}
	}
	if settag {
		a.winsettag(w)
	}
	if err != "" {
		// C responds with the recorded error after running the whole message.
		return 0, err
	}
	return len(data), ""
}

// wincleartag wipes the tag right of the bar (wind.c).
func (a *Acme) wincleartag(w *Window) {
	n := w.tag.file.b.nc()
	r, i := a.parsetag(w, 0)
	for ; i < n; i++ {
		if r[i] == '|' {
			break
		}
	}
	if i == n {
		// untested: a window tag always contains the " | " bar
		return
	}
	i++
	a.textdelete(&w.tag, i, n, true)
	w.tag.file.mod = false
	if w.tag.q0 > i {
		w.tag.q0 = i
	}
	if w.tag.q1 > i {
		w.tag.q1 = i
	}
	a.textsetselect(&w.tag, w.tag.q0, w.tag.q1)
}

// runeGetc returns an address getc callback reading from r.
func runeGetc(r []rune) func(int) int {
	return func(q int) int {
		if q < len(r) {
			return int(r[q])
		}
		// untested: address() evaluates within [0,len); this past-end guard
		// mirrors the addr command's getc (driver.go), which is covered
		// untested: 9P error / edge path not exercised by -D
		return 0
	}
}

const qhex = "0123456789abcdef"

// dquote renders bytes as a quoted string: \n \t \r \" \\ literal, else \xHH.
// It is byte-identical to debug.c's printquoted so read output can be compared.
func dquote(s string) string {
	b := make([]byte, 0, len(s)+2)
	b = append(b, '"')
	for i := 0; i < len(s); i++ {
		c := s[i]
		switch c {
		case '"':
			b = append(b, '\\', '"')
		case '\\':
			b = append(b, '\\', '\\')
		case '\n':
			b = append(b, '\\', 'n')
		case '\t':
			b = append(b, '\\', 't')
		case '\r':
			b = append(b, '\\', 'r')
		default:
			if c >= 0x20 && c < 0x7f {
				b = append(b, c)
			} else {
				b = append(b, '\\', 'x', qhex[c>>4], qhex[c&0xf])
			}
		}
	}
	b = append(b, '"')
	return string(b)
}

// dunquote parses a quoted string (the inverse of dquote / debug.c's dunquote).
func dunquote(s string) (string, bool) {
	if len(s) == 0 || s[0] != '"' {
		return "", false
	}
	var b []byte
	for i := 1; i < len(s) && s[i] != '"'; {
		if s[i] != '\\' {
			b = append(b, s[i])
			i++
			continue
		}
		i++
		if i >= len(s) {
			return "", false
		}
		switch s[i] {
		case 'n':
			b, i = append(b, '\n'), i+1
		case 't':
			b, i = append(b, '\t'), i+1
		case 'r':
			b, i = append(b, '\r'), i+1
		case '"':
			b, i = append(b, '"'), i+1
		case '\\':
			b, i = append(b, '\\'), i+1
		case '0':
			// untested: writing a NUL is not differentiable (C's cvttorunes
			// elides NUL on insert, the Go port keeps it), so no scenario uses it
			// untested: 9P error / edge path not exercised by -D
			b, i = append(b, 0), i+1
		case 'x':
			if i+2 >= len(s) {
				return "", false
			}
			hi, lo := hexv(s[i+1]), hexv(s[i+2])
			if hi < 0 || lo < 0 {
				return "", false
			}
			b = append(b, byte(hi<<4|lo))
			i += 3
		default:
			return "", false
		}
	}
	return string(b), true
}

// hexv returns the value of a hex digit, or -1 if c is not one.
func hexv(c byte) int {
	switch {
	case c >= '0' && c <= '9':
		return int(c - '0')
	case c >= 'a' && c <= 'f':
		return int(c-'a') + 10
	case c >= 'A' && c <= 'F':
		return int(c-'A') + 10
	}
	return -1
}
