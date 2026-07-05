package main

import (
	"bufio"
	"crypto/sha1"
	"io"
	"os"
	"sort"
	"syscall"
	"time"
)

// Port of the get/put file I/O used by the 9P ctl "get"/"put" commands
// (exec.c get/put, text.c textload/textreset/textcolumnate, file.c, ecmd.c
// nlcount), including directory windows.

// lookfile finds the (active) window whose body file is named s (look.c).
func (a *Acme) lookfile(s []rune) *Window {
	n := len(s)
	if n > 1 && s[n-1] == '/' {
		// untested: +Errors names never end in a slash
		n--
	}
	for _, c := range a.row.col {
		for _, w := range c.w {
			name := w.body.file.name
			k := len(name)
			if k > 1 && name[k-1] == '/' {
				// untested: -D never names a window with a trailing slash
				k--
			}
			if runeeqRange(s, n, name, k) {
				w = w.body.file.curtext.w
				if w.col != nil {
					return w
				}
			}
		}
	}
	return nil
}

// errorwin1 finds or creates the "+Errors" window for directory dir, copying the
// include-directory list incl into it (util.c).
func (a *Acme) errorwin1(dir []rune, incl [][]rune) *Window {
	var name []rune
	if len(dir) != 0 {
		// errorwinforwin passes the window's directory; cons passes none
		name = append(append(append(name, dir...), '/'), []rune("+Errors")...)
	} else {
		name = []rune("+Errors")
	}
	w := a.lookfile(name)
	if w == nil {
		if a.row.ncol() == 0 {
			a.rowadd(&a.row, nil, -1)
		}
		w = a.coladd(a.row.col[a.row.ncol()-1], nil, nil, -1)
		w.filemenu = false
		a.winsetname(w, name)
		a.xfidlog(w, "new")
	}
	// Apply the includes back-to-front: winaddincl prepends, so iterating in
	// reverse leaves the new window's list in the source's order (util.c's
	// for(i=nincl; --i>=0;) loop).
	for i := len(incl) - 1; i >= 0; i-- {
		// untested: get/put error-or-directory edge path not exercised by -D
		a.winaddincl(w, append([]rune(nil), incl[i]...))
	}
	w.autoindent = a.globalautoindent
	return w
}

// errorwinforwin returns the +Errors window for w's directory, creating it if
// necessary and inheriting w's include-directory list (util.c errorwinforwin;
// the winlock/retry dance is not needed on the single-threaded -D surface).
func (a *Acme) errorwinforwin(w *Window) *Window {
	dir := a.dirname(&w.body, nil)
	if len(dir) == 1 && dir[0] == '.' { // sigh
		dir = nil
	}
	return a.errorwin1(dir, w.incl)
}

// statid returns a file's identity for the modified-since-read check.
func statid(fi os.FileInfo) (dev int, qidpath uint64, mtime int64) {
	mtime = fi.ModTime().Unix()
	if st, ok := fi.Sys().(*syscall.Stat_t); ok {
		dev = int(st.Dev)
		qidpath = uint64(st.Ino)
	}
	return
}

// nlcount counts the newlines in [q0,q1); nr is the number of characters after
// the last newline (ecmd.c nlcount).
func (a *Acme) nlcount(t *Text, q0, q1 int) (nl, nr int) {
	if q1 <= q0 {
		return 0, 0
	}
	buf := make([]rune, q1-q0)
	t.file.b.Read(q0, buf)
	start := q0
	for i := 0; q0 < q1; i, q0 = i+1, q0+1 {
		if buf[i] == '\n' {
			start = q0 + 1
			nl++
		}
	}
	return nl, q0 - start
}

// textreset clears a text and its file without recording undo (text.c).
func (a *Acme) textreset(t *Text) {
	t.file.seq = 0
	t.eq0 = ^0
	a.textsetselect(t, t.org, t.org)
	t.fr.Delete(0, t.fr.NumChars)
	t.org = 0
	t.q0 = 0
	t.q1 = 0
	filereset(t.file)
	t.file.b.Reset()
}

// textload reads a regular file into t's (empty) body and fills its frame
// (text.c textload, regular-file path only).
func (a *Acme) textload(t *Text, q0 int, file string, setqid bool) int {
	if t.ncache != 0 || t.file.b.nc() != 0 || t.w == nil || t != &t.w.body {
		// untested: get calls textload only on a freshly reset body
		bug("text.load")
	}
	if t.w.isdir && t.file.nname() == 0 {
		a.warning(nil, "empty directory name")
		return -1
	}
	if a.ismtpt(file) {
		// refuse to open acme's own mount point, which would recurse forever
		a.warning(nil, "will not open self mount point %s\n", file)
		return -1
	}
	f, err := os.Open(file)
	if err != nil {
		a.warning(nil, "can't open %s: %s\n", file, errstr(err))
		return -1
	}
	defer f.Close()
	fi, err := f.Stat()
	if err != nil {
		// untested: stat of an already-open file does not fail in practice
		a.warning(nil, "can't fstat %s: %s\n", file, errstr(err))
		return -1
	}
	nulls := false
	var h io.Writer
	var sum interface{ Sum([]byte) []byte }
	var q1 int
	if fi.IsDir() {
		if len(t.file.text) > 1 {
			// untested: -D never opens a directory with multiple windows on it
			a.warning(nil, "%s is a directory; can't read with multiple windows on it\n", file)
			return -1
		}
		t.w.isdir = true
		t.w.filemenu = false
		if n := t.file.nname(); n > 0 && t.file.name[n-1] != '/' {
			rp := append(append([]rune(nil), t.file.name...), '/')
			a.winsetname(t.w, rp)
		}
		ents, _ := f.ReadDir(-1)
		var dlp []*Dirlist
		for _, e := range ents {
			name := e.Name()
			if e.IsDir() {
				// untested: -D cannot create a subdirectory in the opened directory
				name += "/"
			}
			dlp = append(dlp, &Dirlist{r: []rune(name), wid: t.fr.Font.StringWidth(name)})
		}
		sort.Slice(dlp, func(i, j int) bool { return string(dlp[i].r) < string(dlp[j].r) })
		t.w.dlp = dlp
		a.textcolumnate(t, dlp)
		q1 = t.file.b.nc()
	} else {
		t.w.isdir = false
		t.w.filemenu = true
		if q0 == 0 {
			sh := sha1.New()
			h, sum = sh, sh
		}
		var src io.Reader = f
		if h != nil {
			src = io.TeeReader(f, h)
		}
		q1 = q0 + t.file.b.Load(q0, src, &nulls)
	}
	if setqid {
		if sum != nil {
			copy(t.file.sha1[:], sum.Sum(nil))
		} else {
			t.file.sha1 = [20]byte{}
		}
		t.file.dev, t.file.qidpath, t.file.mtime = statid(fi)
	}
	// fill the frame with the visible portion
	rp := make([]rune, RBUFSIZE)
	for q := q0; q < q1; {
		n := min(q1-q, RBUFSIZE)
		t.file.b.Read(q, rp[:n])
		if q < t.org {
			// untested: get resets org to 0 before loading, so q >= org
			t.org += n
		} else if q <= t.org+t.fr.NumChars {
			t.fr.Insert(rp[:n], q-t.org)
		}
		if t.fr.LastLineFull {
			break
		}
		q += n
	}
	for _, u := range t.file.text {
		if u != t {
			if u.org > u.file.b.nc() {
				// untested: textreset already zeroed every text's origin
				u.org = 0
			}
			a.textresize(u, u.all, true)
			a.textbacknl(u, u.org, 0)
		}
		a.textsetselect(u, q0, q0)
	}
	if nulls {
		a.warning(nil, "%s: NUL bytes elided\n", file)
	}
	return q1 - q0
}

// textcolumnate lays the directory listing dlp out in columns in t's file,
// using the frame width and tab stops (text.c).
func (a *Acme) textcolumnate(t *Text, dlp []*Dirlist) {
	if len(t.file.text) > 1 {
		// untested: a directory window is never zeroxed
		return
	}
	ndl := len(dlp)
	mint := t.fr.Font.StringWidth("0")
	// go for narrower tabs if set more than 3 wide
	t.fr.MaxTab = min(a.maxtab, TABDIR) * mint
	maxt := t.fr.MaxTab
	colw := 0
	for _, dl := range dlp {
		w := dl.wid
		if maxt-w%maxt < mint || w%maxt == 0 {
			w += mint
		}
		if w%maxt != 0 {
			w += maxt - (w % maxt)
		}
		if w > colw {
			colw = w
		}
	}
	ncol := 1
	if colw != 0 {
		ncol = max(1, t.fr.R.Dx()/colw)
	}
	nrow := (ndl + ncol - 1) / ncol
	q1 := 0
	nl := []rune{'\n'}
	tab := []rune{'\t'}
	for i := 0; i < nrow; i++ {
		for j := i; j < ndl; j += nrow {
			dl := dlp[j]
			a.fileinsert(t.file, q1, dl.r)
			q1 += len(dl.r)
			if j+nrow >= ndl {
				break
			}
			w := dl.wid
			if maxt-w%maxt < mint {
				a.fileinsert(t.file, q1, tab)
				q1++
				w += mint
			}
			for {
				a.fileinsert(t.file, q1, tab)
				q1++
				w += maxt - (w % maxt)
				if w >= colw {
					break
				}
			}
		}
		a.fileinsert(t.file, q1, nl)
		q1++
	}
}

// get reloads the window's file from disk (exec.c get; the no-argument ctl path,
// which always reuses the window's current name -- so samename is true).
func (a *Acme) get(w *Window, argt *Text, arg []rune) {
	t := &w.body
	if !w.isdir && t.file.b.nc() > 0 && !a.winclean(w, true) {
		return
	}
	name := a.getname(t, argt, arg, false)
	if name == nil {
		a.warning(nil, "no file name\n")
		return
	}
	// With more than one window on this file, refuse a directory (exec.c get).
	if len(t.file.text) > 1 {
		if d, err := os.Stat(string(name)); err == nil && d.IsDir() {
			// untested: get/put error-or-directory edge path not exercised by -D
			a.warning(nil, "%s is a directory; can't read with multiple windows on it\n", string(name))
			return
		}
	}
	// save each text's origin/selection as (line, rune-in-line) to restore it
	// across the reload (textload renumbers positions).
	type taddr struct{ lorigin, rorigin, lq0, rq0, lq1, rq1 int }
	addr := make([]taddr, len(t.file.text))
	for i, u := range t.file.text {
		addr[i].lorigin, addr[i].rorigin = a.nlcount(u, 0, u.org)
		addr[i].lq0, addr[i].rq0 = a.nlcount(u, 0, u.q0)
		addr[i].lq1, addr[i].rq1 = a.nlcount(u, u.q0, u.q1)
	}
	for _, u := range t.file.text {
		a.textreset(u)
		a.windirfree(u.w) // drop the old directory listing before the reload (exec.c get)
	}
	// Reloading the window's own name keeps it clean and restores the saved
	// position; loading a different file replaces the contents and marks it
	// modified (exec.c get's samename branch).
	samename := runeEq(name, t.file.name)
	a.textload(t, 0, string(name), samename)
	dirty := !samename
	t.file.mod = dirty
	for _, u := range t.file.text {
		u.w.dirty = dirty
	}
	a.winsettag(w)
	t.file.unread = false
	for i, u := range t.file.text {
		a.textsetselect(&u.w.tag, u.w.tag.file.b.nc(), u.w.tag.file.b.nc())
		if samename {
			q0 := a.nlcounttopos(u, 0, addr[i].lq0, addr[i].rq0)
			q1 := a.nlcounttopos(u, q0, addr[i].lq1, addr[i].rq1)
			a.textsetselect(u, q0, q1)
			q0 = a.nlcounttopos(u, 0, addr[i].lorigin, addr[i].rorigin)
			a.textsetorigin(u, q0, false)
		}
		a.textscrdraw(u)
	}
	a.xfidlog(w, "get")
}

// checksha1 recomputes name's sha1 and, if it matches f's stored digest, accepts
// the new stat as identical (exec.c checksha1).
func checksha1(name string, f *File, fi os.FileInfo) {
	fd, err := os.Open(name)
	if err != nil {
		// untested: the caller just statted name successfully
		return
	}
	h := sha1.New()
	io.Copy(h, fd)
	fd.Close()
	var out [20]byte
	copy(out[:], h.Sum(nil))
	if out == f.sha1 {
		f.dev, f.qidpath, f.mtime = statid(fi)
	}
}

// put writes the window's body to its file (exec.c put -> file.c putfile).
func (a *Acme) put(w *Window) {
	if w.isdir {
		// untested: a directory window cannot be created through -D
		return
	}
	f := w.body.file
	name := string(f.name)
	if name == "" {
		a.warning(nil, "no file name\n")
		return
	}
	if w.autoindent {
		// C's put trims trailing whitespace on autoindent windows before writing;
		// this is the path Putall and the 9P ctl "put" take (exec.c put).
		// untested: no test Puts an autoindent window with trailing whitespace
		a.trimspaces(&w.body)
	}
	a.putfile(f, 0, f.b.nc(), f.name)
	a.xfidlog(w, "put")
}

// timefmt formats a Unix time the way C acme's %t verb does: YYYY/MM/DD
// hh:mm:ss in local time (acme.c timefmt). Used by putfile's "modified since
// last read" warning.
func timefmt(sec int64) string {
	return time.Unix(sec, 0).Format("2006/01/02 15:04:05")
}

// putfile writes f's [q0,q1) to the file named namer (file.c/exec.c putfile).
func (a *Acme) putfile(f *File, q0, q1 int, namer []rune) {
	w := f.curtext.w
	name := string(namer)
	if fi, err := os.Stat(name); err == nil && runeEq(namer, f.name) {
		d, dp, mt := statid(fi)
		if f.dev != d || f.qidpath != dp || f.mtime != mt {
			checksha1(name, f, fi)
		}
		if f.dev != d || f.qidpath != dp || f.mtime != mt {
			if f.unread {
				a.warning(nil, "%s not written; file already exists\n", name)
			} else {
				// C also prints "by <muid>" when the file system records the
				// modifying user; Unix os.Stat exposes no muid (it is empty in
				// the C oracle too), so that clause is omitted.
				a.warning(nil, "%s modified since last read\n\twas %s; now %s\n",
					name, timefmt(f.mtime), timefmt(mt))
			}
			f.dev, f.qidpath, f.mtime = d, dp, mt
			return
		}
	}
	// Refuse to write an append-only file (exec.c putfile's isapp check). C opens
	// then dirfstats; Go must check before os.Create truncates, so stat first.
	if fi, err := os.Stat(name); err == nil && fi.Size() > 0 && fi.Mode()&os.ModeAppend != 0 {
		// untested: -D cannot create an append-only file
		a.warning(nil, "%s not written; file is append only\n", name)
		return
	}
	fd, err := os.Create(name)
	if err != nil {
		// untested: create failure (e.g. permissions) not exercised
		a.warning(nil, "can't create file %s: %s\n", name, errstr(err))
		return
	}
	// Buffer the writes so they are large and block-aligned, matching C's use of
	// bio (exec.c putfile): not strictly necessary, but it works around buggy file
	// systems that mishandle unaligned writes. Maxblock (8K) is bio's default.
	b := bufio.NewWriterSize(fd, Maxblock)
	h := sha1.New()
	buf := make([]rune, RBUFSIZE)
	werr := false
	for q := q0; q < q1; {
		n := min(q1-q, RBUFSIZE)
		f.b.Read(q, buf[:n])
		bb := []byte(string(buf[:n]))
		h.Write(bb)
		if _, err := b.Write(bb); err != nil {
			// untested: a write to a freshly created temp file does not fail
			a.warning(nil, "can't write file %s: %s\n", name, errstr(err))
			werr = true
			break
		}
		q += n
	}
	if !werr {
		if err := b.Flush(); err != nil {
			// untested: the buffered flush to a temp file does not fail (matches
			// C's Bflush error check)
			// untested: get/put error-or-directory edge path not exercised by -D
			a.warning(nil, "can't write file %s: %s\n", name, errstr(err))
			werr = true
		}
	}
	fd.Close()
	if werr {
		// untested: see the write/flush-failure path above
		return
	}
	if runeEq(namer, f.name) {
		if q0 != 0 || q1 != f.b.nc() {
			// untested: the ctl put always writes the whole body (0..nc)
			f.mod = true
			w.dirty = true
			f.unread = true
		} else {
			// In case the file is on NFS, reopen the fd before stat'ing to refresh
			// the attribute cache (otherwise the mtime would be stale and not match
			// what NFS sees). The file is already written, so this is a no-op when
			// not on NFS. Reopen OWRITE (no truncation) in case we lack read
			// permission (the create worked, so we probably still have write).
			// (exec.c putfile: open + dirfstat, falling back to the post-create
			// stat.)
			fi, err := os.Stat(name)
			if fd2, e := os.OpenFile(name, os.O_WRONLY, 0); e == nil {
				if fi2, e := fd2.Stat(); e == nil {
					fi, err = fi2, nil
				}
				fd2.Close()
			}
			if err == nil {
				f.dev, f.qidpath, f.mtime = statid(fi)
				copy(f.sha1[:], h.Sum(nil))
				f.mod = false
				w.dirty = false
				f.unread = false
			}
		}
		for _, u := range f.text {
			u.w.putseq = f.seq
			u.w.dirty = w.dirty
		}
	}
	a.winsettag(w)
}
