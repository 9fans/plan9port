package main

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"strconv"
	"strings"

	"9fans.net/plan9port/draw"
)

// dumpfield returns the k-th fixed-width (12-byte) field of a dump record line,
// trimmed; the type char occupies byte 0 (rows.c parses l+1+k*12).
func dumpfield(l string, k int) string {
	s := 1 + k*12
	if s >= len(l) {
		// untested: callers only request fields that the record actually has.
		return ""
	}
	e := s + 12
	if e > len(l) {
		// untested: every requested 12-byte field is fully present in -D dumps.
		e = len(l)
	}
	return strings.TrimSpace(l[s:e])
}

func dumpatoi(l string, k int) int { n, _ := strconv.Atoi(dumpfield(l, k)); return n }

// lookdumpid finds the window whose dumpid == id (rows.c lookid(id, dump=TRUE),
// used to attach a zerox 'x' record to the window it was dumped from).
func (a *Acme) lookdumpid(id int) *Window {
	for _, c := range a.row.col {
		for _, w := range c.w {
			if w.dumpid == id {
				return w
			}
		}
	}
	// untested: a well-formed dump's x record always references a window already
	// loaded with that dumpid; a missing one (corrupt dump) is not seen under -D.
	// untested: malformed-dump / interactive-drag path not exercised by -D
	return nil
}

// rowfirstline returns the first line (up to but not including the first '\n')
// of t's buffer, as a string.
func rowfirstline(t *Text) string {
	n := t.file.b.nc()
	r := make([]rune, n)
	t.file.b.Read(0, r)
	for i, c := range r {
		if c == '\n' {
			// untested: rowfirstline is called only on the row and column tags,
			// which are single-line under -D, so the first '\n' is never found.
			// untested: malformed-dump / interactive-drag path not exercised by -D
			return string(r[:i])
		}
	}
	return string(r)
}

// rowdump1 writes the session-save form of row to w (rows.c rowdump1): the
// current directory, the two global fonts, the column widths, the row and
// column tags, then for each window a type record (x/e/f/F), its ctl line, its
// tag (embedded newlines written as the 0xff byte), and the body text for
// windows whose contents must be saved (dirty/scratch).
func (a *Acme) rowdump1(row *Row, w io.Writer) {
	wdir, _ := os.Getwd()
	fmt.Fprintf(w, "%s\n", wdir)
	fmt.Fprintf(w, "%s\n", a.fontnames[0])
	fmt.Fprintf(w, "%s\n", a.fontnames[1])
	for i, c := range row.col {
		fmt.Fprintf(w, "%11.7f", 100.0*float64(c.r.Min.X-row.r.Min.X)/float64(row.r.Dx()))
		if i == row.ncol()-1 {
			io.WriteString(w, "\n")
		} else {
			io.WriteString(w, " ")
		}
	}
	for _, c := range row.col {
		for _, win := range c.w {
			win.body.file.dumpid = 0
		}
	}
	fmt.Fprintf(w, "w %s\n", rowfirstline(&row.tag))
	for i, c := range row.col {
		fmt.Fprintf(w, "c%11d %s\n", i, rowfirstline(&c.tag))
	}
	for i, c := range row.col {
		for j, win := range c.w {
			a.rowdumpwin(w, c, i, j, win)
		}
	}
}

// rowdumpwin writes one window's dump records (the per-window body of
// rowdump1's loop). Windows owned by an external program (event file open) and
// zeroxes of such windows are skipped.
func (a *Acme) rowdumpwin(w io.Writer, c *Column, i, j int, win *Window) {
	a.wincommit(win, &win.tag)
	t := &win.body
	// windows owned by others get special treatment
	if win.nopen[qWevent] > 0 && win.dumpstr == "" {
		return
	}
	// zeroxes of external windows are tossed
	if len(t.file.text) > 1 {
		for _, u := range t.file.text {
			if u.w == win {
				continue
			}
			if u.w.nopen[qWevent] > 0 {
				// untested: a zerox whose sibling has its event file open by an
				// external program; the -D zerox test has no such reader.
				// untested: malformed-dump / interactive-drag path not exercised by -D
				return
			}
		}
	}
	fontname := ""
	if t.reffont.f != a.font {
		// untested: every -D window uses the single test font (var==fix==font),
		// so a window never carries a non-default font in its dump record.
		// C dumps reffont->f->lodpi->name, plus ",hi" when a hi-dpi variant is
		// loaded (rows.c rowdump1).
		fontname = t.reffont.f.LoDPI().Name
		if t.reffont.f.HiDPI() != nil {
			// untested: no -D window loads a hi-dpi font variant.
			fontname = t.reffont.f.LoDPI().Name + "," + t.reffont.f.HiDPI().Name
		}
	}
	name := string(t.file.name)
	ypct := 100.0 * float64(win.r.Min.Y-c.r.Min.Y) / float64(c.r.Dy())
	dumped := false
	switch {
	case t.file.dumpid != 0:
		fmt.Fprintf(w, "x%11d %11d %11d %11d %11.7f %s\n", i, t.file.dumpid,
			win.body.q0, win.body.q1, ypct, fontname)
	case win.dumpstr != "":
		fmt.Fprintf(w, "e%11d %11d %11d %11d %11.7f %s\n", i, t.file.dumpid,
			0, 0, ypct, fontname)
	case (!win.dirty && fileExists(name)) || win.isdir:
		t.file.dumpid = win.id
		fmt.Fprintf(w, "f%11d %11d %11d %11d %11.7f %s\n", i, win.id,
			win.body.q0, win.body.q1, ypct, fontname)
	default:
		dumped = true
		t.file.dumpid = win.id
		fmt.Fprintf(w, "F%11d %11d %11d %11d %11.7f %11d %s\n", i, j,
			win.body.q0, win.body.q1, ypct, t.file.b.nc(), fontname)
	}
	io.WriteString(w, a.winctlprint(win, false))
	// the tag, with any embedded newline written as the 0xff byte (invalid UTF)
	{
		n := win.tag.file.b.nc()
		r := make([]rune, n)
		win.tag.file.b.Read(0, r)
		for k := 0; k < n; {
			s := k
			for k < n && r[k] != '\n' {
				k++
			}
			io.WriteString(w, string(r[s:k]))
			if k < n {
				// untested: window tags are single-line under -D, so a tag never
				// has an embedded newline to encode as 0xff.
				// untested: malformed-dump / interactive-drag path not exercised by -D
				w.Write([]byte{0xff})
				k++
			}
		}
		io.WriteString(w, "\n")
	}
	if dumped {
		n := t.file.b.nc()
		r := make([]rune, n)
		t.file.b.Read(0, r)
		io.WriteString(w, string(r))
	}
	if win.dumpstr != "" {
		if win.dumpdir != "" {
			fmt.Fprintf(w, "%s\n%s\n", win.dumpdir, win.dumpstr)
		} else {
			fmt.Fprintf(w, "\n%s\n", win.dumpstr)
		}
	}
}

// fileExists reports whether name is an existing path (C access(name,0)==0).
func fileExists(name string) bool {
	if name == "" {
		// untested: a named window always has a non-empty name; an unnamed
		// (scratch) window takes the F (save-body) path before this is reached.
		return false
	}
	_, err := os.Stat(name)
	return err == nil
}

// rowdump writes row's session to file (default $HOME/acme.dump), atomically via
// a temp file + rename (rows.c rowdump). The C mode-preserving dirstat/dirwstat
// is reduced to a plain rename; the -D dump file is always new.
func (a *Acme) rowdump(row *Row, file string) {
	if row.ncol() == 0 {
		// untested: the -D Dump tests always have at least one column.
		return
	}
	if file == "" {
		home := os.Getenv("HOME")
		if home == "" {
			// untested: the -D harness always sets $HOME.
			a.warning(nil, "can't find file for dump: $home not defined\n")
			return
		}
		file = home + "/acme.dump"
	}
	tmp := file + ".tmp"
	f, err := os.Create(tmp)
	if err != nil {
		// untested: the -D dump path is always writable.
		a.warning(nil, "can't create temp file for %s: %s\n", file, errstr(err))
		return
	}
	b := bufio.NewWriter(f)
	a.rowdump1(row, b)
	b.Flush()
	f.Close()
	// Preserve the existing dump file's mode across the rewrite, as C does with
	// nulldir + dirstat(file)->mode + dirwstat(tmp) (rows.c rowdump).
	if fi, err := os.Stat(file); err == nil {
		// untested: the -D Dump tests write a fresh acme.dump (no prior file).
		os.Chmod(tmp, fi.Mode().Perm())
	}
	if err := os.Rename(tmp, file); err != nil {
		// untested: rename of a temp file just created in the same dir.
		a.warning(nil, "can't rename %s to %s: %s\n", tmp, file, errstr(err))
	}
}

// rowload restores a session from the dump file (default $HOME/acme.dump): chdir
// to its cwd, load the global fonts, recreate/resize the columns, set the row
// and column tags, then create each window from its f/F/x record (rows.c
// rowload). An 'e' (external-command) record re-runs its program via run(), so
// the window is rebuilt by the program itself (rowloadwin). Returns false on a
// malformed file. initing is true only for the startup -l path (not under -D).
func (a *Acme) rowload(row *Row, file string, initing bool) bool {
	if file == "" {
		// untested: -D Load always names the dump file, so the $HOME/acme.dump
		// default path (the bare "Load" / startup reload) is not exercised.
		// untested: malformed-dump / interactive-drag path not exercised by -D
		home := os.Getenv("HOME")
		if home == "" {
			// untested: the -D harness always sets $HOME.
			a.warning(nil, "can't find file for load: $home not defined\n")
			return false
		}
		// untested: see above -- the default dump path is not used under -D.
		file = home + "/acme.dump"
	}
	f, err := os.Open(file)
	if err != nil {
		// untested: the -D Load tests always dump the file first.
		a.warning(nil, "can't open load file %s: %s\n", file, errstr(err))
		return false
	}
	defer f.Close()
	b := bufio.NewReader(f)
	line := 0
	rd := func() (string, bool) {
		s, err := b.ReadString('\n')
		if s == "" && err != nil {
			return "", false
		}
		line++
		return strings.TrimSuffix(s, "\n"), true
	}
	bad := func() bool {
		// untested: a malformed dump file; the -D tests load well-formed dumps.
		a.warning(nil, "bad load file %s:%d\n", file, line)
		return false
	}
	// current directory
	l, ok := rd()
	if !ok {
		// untested: a truncated dump (no cwd line); -D dumps are well-formed.
		return bad()
	}
	if err := os.Chdir(l); err != nil {
		// untested: the dump's cwd is the still-current -D temp dir.
		a.warning(nil, "can't chdir %s\n", l)
		return false
	}
	// global fonts
	for i := 0; i < 2; i++ {
		l, ok = rd()
		if !ok {
			// untested: a truncated dump (missing a font line); well-formed under -D.
			return bad()
		}
		if l != "" && l != a.fontnames[i] {
			// untested: -D loads the same single test font, so the dump's font
			// lines always equal the current fonts and rfget is not called.
			// untested: malformed-dump / interactive-drag path not exercised by -D
			a.rfget(i == 1, true, i == 0 && initing, l)
		}
	}
	if initing && row.ncol() == 0 {
		// untested: -D loads into an already-initialized row, never at startup.
		a.rowinit(row, a.screen.Clipr)
	}
	// column widths
	l, ok = rd()
	if !ok {
		// untested: a truncated dump (no column-width line); well-formed under -D.
		return bad()
	}
	// C uses Blinelen, which counts the trailing newline rd() stripped; the
	// fields are 12 bytes each only with that newline included.
	if j := (len(l) + 1) / 12; j <= 0 || j > 10 {
		// untested: a malformed column-width line; well-formed under -D.
		return bad()
	} else {
		for i := 0; i < j; i++ {
			s := i * 12
			e := s + 12
			if e > len(l) {
				e = len(l)
			}
			percent, _ := strconv.ParseFloat(strings.TrimSpace(l[s:e]), 64)
			if percent < 0 || percent >= 100 {
				// untested: an out-of-range column percent; well-formed under -D.
				return bad()
			}
			x := row.r.Min.X + int(percent*float64(row.r.Dx())/100+0.5)
			if i < row.ncol() {
				if i == 0 {
					continue
				}
				c1 := row.col[i-1]
				c2 := row.col[i]
				r1 := c1.r
				r2 := c2.r
				if x < a.border() {
					// untested: the second column's dumped boundary is well inside
					// the row, never left of the border, under -D.
					// untested: malformed-dump / interactive-drag path not exercised by -D
					x = a.border()
				}
				r1.Max.X = x - a.border()
				r2.Min.X = x
				if r1.Dx() < 50 || r2.Dx() < 50 {
					// untested: the -D test columns are always wider than 50px.
					continue
				}
				a.screen.Draw(draw.Rpt(r1.Min, r2.Max), a.disp.White, nil, draw.ZP)
				a.colresize(c1, r1)
				a.colresize(c2, r2)
				r2.Min.X = x - a.border()
				r2.Max.X = x
				a.screen.Draw(r2, a.disp.Black, nil, draw.ZP)
			}
			if i >= row.ncol() {
				// untested: -D loads a dump into the same column count it was made
				// with, so a new column is never added here (only at startup load).
				// untested: malformed-dump / interactive-drag path not exercised by -D
				a.rowadd(row, nil, x)
			}
		}
	}
	// 'c' (column tag) and 'w' (row tag) records
	done := false
	for !done {
		l, ok = rd()
		if !ok {
			// untested: a dump always has window records after the c/w tags, so
			// the loop exits via the default case below, not at EOF.
			// untested: malformed-dump / interactive-drag path not exercised by -D
			l = ""
			break
		}
		switch {
		case len(l) > 0 && l[0] == 'c':
			i := dumpatoi(l, 0)
			r := []rune(l[1*12:])
			n := 0
			for n < len(r) && r[n] != ' ' {
				// untested: the column-tag field after l[12:] always begins with a
				// space (the menu text), so r[0]==' ' and this never iterates.
				// untested: malformed-dump / interactive-drag path not exercised by -D
				n++
			}
			a.textdelete(&row.col[i].tag, 0, row.col[i].tag.file.b.nc(), true)
			a.textinsert(&row.col[i].tag, 0, r[n+1:], true)
		case len(l) > 0 && l[0] == 'w':
			r := []rune(l[2:])
			a.textdelete(&row.tag, 0, row.tag.file.b.nc(), true)
			a.textinsert(&row.tag, 0, r, true)
		default:
			done = true
		}
	}
	// window records: l holds the first one (or "" at EOF)
	for l != "" {
		if !a.rowloadwin(row, b, l, &line) {
			// untested: a malformed window record; well-formed under -D.
			return bad()
		}
		l, ok = rd()
		if !ok {
			l = ""
		}
	}
	return true
}

// rowloadfonts pre-scans dump file `file` for the two global font names so they
// can be loaded as the defaults before the row (and the display) exist (rows.c
// rowloadfonts). Called from threadmain when -l names a dump file, before
// initdraw. A name that is empty or already current is left alone.
func (a *Acme) rowloadfonts(file string) {
	f, err := os.Open(file)
	if err != nil {
		// untested: a missing dump file just means no font pre-scan.
		return
	}
	defer f.Close()
	b := bufio.NewReader(f)
	if _, err := b.ReadString('\n'); err != nil { // current directory
		// untested: an empty dump file.
		return
	}
	for i := 0; i < 2; i++ { // global fonts
		l, err := b.ReadString('\n')
		if err != nil {
			// untested: a truncated dump file.
			return
		}
		l = strings.TrimRight(l, "\n")
		if l != "" && l != a.fontnames[i] {
			a.fontnames[i] = l
		}
	}
}

// rowloadwin creates one window from its dump record line l (and, for an F
// record, the body runes that follow), returning false on a malformed record.
func (a *Acme) rowloadwin(row *Row, b *bufio.Reader, l string, line *int) bool {
	dumpid := 0
	ndumped := -1
	fontname := ""
	switch l[0] {
	case 'e':
		if len(l) < 1+5*12 {
			// untested: a truncated e record; -D dumps are well-formed.
			return false
		}
		// An external-command window: re-run the command so the program (e.g.
		// win) recreates its own window on load, exactly as C's rowload does
		// (rows.c case 'e': run(nil, cmd, dir, ndir, newns=TRUE, ...)). The header
		// is followed by three lines -- a ctl line (ignored), the directory, and
		// the command -- after which C goes to Nextline, skipping the window build.
		if _, ok := rd2(b, line); !ok { // ctl line, ignored
			// untested: a well-formed e record always has its three follow lines.
			return false
		}
		dir, ok := rd2(b, line) // directory
		if !ok {
			// untested: see above.
			return false
		}
		if dir == "" {
			// C: an empty directory defaults to $HOME/ (or ./ with no $HOME).
			if a.home == "" {
				// untested: the test env always has $HOME set.
				dir = "./"
			} else {
				dir = a.home + "/"
			}
		}
		cmd, ok := rd2(b, line) // command
		if !ok {
			// untested: see above.
			return false
		}
		a.run(nil, cmd, dir, "", "") // win=nil, newns; C frees r in run()
		return true
	case 'f':
		if len(l) < 1+5*12 {
			// untested: a truncated f record; -D dumps are well-formed.
			return false
		}
		fontname = strings.TrimSpace(l[1+5*12:])
	case 'F':
		if len(l) < 1+6*12 {
			// untested: a truncated F record; -D dumps are well-formed.
			return false
		}
		ndumped = dumpatoi(l, 5)
		fontname = strings.TrimSpace(l[1+6*12:])
	case 'x':
		if len(l) < 1+5*12 {
			// untested: a truncated x record; -D dumps are well-formed.
			return false
		}
		fontname = strings.TrimSpace(l[1+5*12:])
		dumpid = dumpatoi(l, 1)
	default:
		// untested: rowload only passes f/F/x/e records here (c/w are consumed in
		// rowload), so an unknown record type cannot occur for a -D dump.
		// untested: malformed-dump / interactive-drag path not exercised by -D
		return false
	}
	i := dumpatoi(l, 0)
	j := dumpatoi(l, 1)
	q0 := dumpatoi(l, 2)
	q1 := dumpatoi(l, 3)
	percent, _ := strconv.ParseFloat(dumpfield(l, 4), 64)
	if i < 0 || i > 10 {
		// untested: a malformed column index; well-formed under -D.
		return false
	}
	if i > row.ncol()-1 {
		// untested: -D loads windows into the same column count they were dumped
		// from, so the dumped index never exceeds the current columns.
		// untested: malformed-dump / interactive-drag path not exercised by -D
		i = row.ncol() - 1
	}
	c := row.col[i]
	y := c.r.Min.Y + int(percent*float64(c.r.Dy())/100+0.5)
	if y < c.r.Min.Y || y >= c.r.Max.Y {
		// untested: the dumped vertical percent always lands inside the column.
		y = -1
	}
	var w *Window
	if dumpid == 0 {
		w = a.coladd(c, nil, nil, y)
	} else {
		w = a.coladd(c, nil, a.lookdumpid(dumpid), y)
	}
	if w == nil {
		// untested: coladd fails only when the column is too small to split.
		return true
	}
	w.dumpid = j
	// the ctl+tag line; convert any 0xff back to the embedded newline
	tl, ok := rd2(b, line)
	if !ok {
		// untested: a window record is always followed by its tag line under -D.
		return false
	}
	tl = strings.ReplaceAll(tl, "\xff", "\n")
	r := []rune(tl[5*12:])
	n := 0
	ns := -1
	for ; n < len(r); n++ {
		if r[n] == '/' {
			// untested: -D window names ("ff", scratch labels) contain no '/'.
			ns = n
		}
		if r[n] == ' ' {
			break
		}
	}
	if dumpid == 0 {
		a.winsetname(w, r[:n])
	}
	for ; n < len(r); n++ {
		if r[n] == '|' {
			break
		}
	}
	a.wincleartag(w)
	if n+1 <= len(r) {
		a.textinsert(&w.tag, w.tag.file.b.nc(), r[n+1:], true)
	}
	switch {
	case ndumped >= 0:
		// F record: the next ndumped runes are the saved body. C writes them to a
		// temp file and textloads it (rows.c), rather than an in-memory insert, so
		// the reload draws exactly as a file get would -- frame fill via textload +
		// textsetselect(0,0) on every shared text. An in-memory textinsert omits
		// textload's textsetselect, dropping a caret-reposition frame and desyncing
		// the whole multi-window Load redraw sequence. Match C: temp file + textload.
		body := make([]rune, 0, ndumped)
		for k := 0; k < ndumped; k++ {
			ru, _, err := b.ReadRune()
			if err != nil {
				// untested: the dump always has the promised ndumped body runes.
				return false
			}
			if ru == '\n' {
				// untested: the -D scratch/F bodies are single-line, no newline.
				*line++
			}
			body = append(body, ru)
		}
		tf, err := os.CreateTemp("", "acmeload")
		if err != nil {
			// untested: temp-file creation does not fail in the test environment.
			a.warning(nil, "can't create temp file: %v\n", err)
			return false
		}
		tf.WriteString(string(body))
		tf.Close()
		a.textload(&w.body, 0, tf.Name(), true)
		os.Remove(tf.Name())
		w.body.file.mod = true
		for _, u := range w.body.file.text {
			u.w.dirty = true
		}
		a.winsettag(w)
	case dumpid == 0 && (ns+1 >= len(r) || (r[ns+1] != '+' && r[ns+1] != '-')):
		// f record: a real file name -> reload it from disk.
		a.get(w, nil, nil)
	}
	if fontname != "" {
		// untested: -D uses a single font, so a window never has a saved font.
		a.fontx(&w.body, nil, nil, false, false, []rune(fontname))
	}
	if q0 > w.body.file.b.nc() || q1 > w.body.file.b.nc() || q0 > q1 {
		// untested: the dumped selection q0..q1 is always within the loaded body.
		q0 = 0
		q1 = 0
	}
	a.textshow(&w.body, q0, q1, true)
	w.maxlines = minInt(w.body.fr.NumLines, maxInt(w.maxlines, w.body.fr.MaxLines))
	a.xfidlog(w, "new")
	return true
}

// rd2 reads one newline-terminated line from b (without the newline), bumping
// *line; ok is false at EOF.
func rd2(b *bufio.Reader, line *int) (string, bool) {
	s, err := b.ReadString('\n')
	if s == "" && err != nil {
		// untested: rd2 is called only for a tag line that always follows its
		// window record, so EOF here means a truncated dump (not seen under -D).
		// untested: malformed-dump / interactive-drag path not exercised by -D
		return "", false
	}
	*line++
	return strings.TrimSuffix(s, "\n"), true
}

// Row layout (rows.c). The multi-column split path in rowadd (colresize of a
// neighbor) and rowresize are ported; the dump-irrelevant graphical bits are
// faithful but never asserted.

// Lcolhdr is the row tag's command menu (rows.c).
var Lcolhdr = []rune("Newcol Kill Putall Dump Exit ")

func (a *Acme) rowinit(row *Row, r draw.Rectangle) {
	a.screen.Draw(r, a.disp.White, nil, draw.ZP)
	row.r = r
	row.col = nil
	r1 := r
	r1.Max.Y = r1.Min.Y + a.font.Height
	t := &row.tag
	a.textinit(t, fileaddtext(nil, t), r1, a.rfget(false, false, false, ""), a.tagcols)
	t.what = Rowtag
	t.row = row
	t.w = nil
	t.col = nil
	r1.Min.Y = r1.Max.Y
	r1.Max.Y += a.border()
	a.screen.Draw(r1, a.disp.Black, nil, draw.ZP)
	a.textinsert(t, 0, Lcolhdr, true)
	a.textsetselect(t, t.file.b.nc(), t.file.b.nc())
}

// rowclean reports whether every window in the row is clean (no unsaved
// changes), warning about each dirty one as a side effect (rows.c rowclean).
// Exit consults it before quitting.
func (a *Acme) rowclean(row *Row) bool {
	clean := true
	for _, c := range row.col {
		clean = a.colclean(c) && clean
	}
	return clean
}

// rowadd inserts column c (allocating one if nil) at x (rows.c).
func (a *Acme) rowadd(row *Row, c *Column, x int) *Column {
	a.clearmouse()
	r := row.r
	r.Min.Y = row.tag.fr.R.Max.Y + a.border()
	var d *Column
	if x < r.Min.X && row.ncol() > 0 { // steal 40% of last column by default
		d = row.col[row.ncol()-1]
		x = d.r.Min.X + 3*d.r.Dx()/5
	}
	// look for the column we'll land on
	i := 0
	for ; i < row.ncol(); i++ {
		d = row.col[i]
		if x < d.r.Max.X {
			break
		}
	}
	if row.ncol() > 0 {
		if i < row.ncol() {
			i++ // new column goes after d
		}
		r = d.r
		if r.Dx() < 100 {
			return nil
		}
		a.screen.Draw(r, a.disp.White, nil, draw.ZP)
		r1 := r
		r1.Max.X = minInt(x-a.border(), r.Max.X-50)
		if r1.Dx() < 50 {
			r1.Max.X = r1.Min.X + 50
		}
		a.colresize(d, r1)
		r1.Min.X = r1.Max.X
		r1.Max.X = r1.Min.X + a.border()
		a.screen.Draw(r1, a.disp.Black, nil, draw.ZP)
		r.Min.X = r1.Max.X
	}
	if c == nil {
		c = new(Column)
		a.colinit(c, r)
	} else {
		a.colresize(c, r)
	}
	c.row = row
	c.tag.row = row
	row.col = append(row.col, nil)
	copy(row.col[i+1:], row.col[i:])
	row.col[i] = c
	return c
}

func (a *Acme) rowresize(row *Row, r draw.Rectangle) {
	or := row.r
	deltax := r.Min.X - or.Min.X
	row.r = r
	r1 := r
	r1.Max.Y = r1.Min.Y + a.font.Height
	a.textresize(&row.tag, r1, true)
	r1.Min.Y = r1.Max.Y
	r1.Max.Y += a.border()
	a.screen.Draw(r1, a.disp.Black, nil, draw.ZP)
	r.Min.Y = r1.Max.Y
	r1 = r
	r1.Max.X = r1.Min.X
	for i := 0; i < row.ncol(); i++ {
		c := row.col[i]
		r1.Min.X = r1.Max.X
		// the test should not be necessary, but guarantee we don't lose a pixel
		if i == row.ncol()-1 {
			r1.Max.X = r.Max.X
		} else {
			r1.Max.X = (c.r.Max.X-or.Min.X)*r.Dx()/or.Dx() + deltax
		}
		if i > 0 {
			r2 := r1
			r2.Max.X = r2.Min.X + a.border()
			a.screen.Draw(r2, a.disp.Black, nil, draw.ZP)
			r1.Min.X = r2.Max.X
		}
		a.colresize(c, r1)
	}
}

// makenewwindow creates a new window in the active column (util.c).
func (a *Acme) makenewwindow(t *Text) *Window {
	var c *Column
	switch {
	case a.activecol != nil:
		c = a.activecol
	case a.seltext != nil && a.seltext.col != nil:
		c = a.seltext.col
	case t != nil && t.col != nil:
		c = t.col
	default:
		if a.row.ncol() == 0 && a.rowadd(&a.row, nil, -1) == nil {
			// unreachable
			bug("can't make column")
		}
		c = a.row.col[a.row.ncol()-1]
	}
	a.activecol = c
	if t == nil || t.w == nil || c.nw() == 0 {
		return a.coladd(c, nil, nil, -1)
	}

	// find biggest window and biggest blank spot
	emptyw := c.w[0]
	bigw := emptyw
	for i := 1; i < c.nw(); i++ {
		w := c.w[i]
		// use >= to choose one near bottom of screen
		if w.body.fr.MaxLines >= bigw.body.fr.MaxLines {
			bigw = w
		}
		if w.body.fr.MaxLines-w.body.fr.NumLines >= emptyw.body.fr.MaxLines-emptyw.body.fr.NumLines {
			emptyw = w
		}
	}
	emptyb := &emptyw.body
	el := emptyb.fr.MaxLines - emptyb.fr.NumLines
	var y int
	// if empty space is big, use it
	if el > 15 || (el > 3 && el > (bigw.body.fr.MaxLines-1)/2) {
		y = emptyb.fr.R.Min.Y + emptyb.fr.NumLines*a.font.Height
	} else {
		// if this window is in column and isn't much smaller, split it
		// untested: malformed-dump / interactive-drag path not exercised by -D
		if t.col == c && t.w.r.Dy() > 2*bigw.r.Dy()/3 {
			// untested: malformed-dump / interactive-drag path not exercised by -D
			bigw = t.w
		}
		// untested: malformed-dump / interactive-drag path not exercised by -D
		y = (bigw.r.Min.Y + bigw.r.Max.Y) / 2
	}
	w := a.coladd(c, nil, nil, y)
	if w.body.fr.MaxLines < 2 {
		// untested: malformed-dump / interactive-drag path not exercised by -D
		a.colgrow(w.col, w, 1)
	}
	return w
}

// rowclose removes column c from the row, freeing it if dofree, and reflows the
// neighbours (rows.c).
func (a *Acme) rowclose(row *Row, c *Column, dofree bool) {
	i := -1
	for j, x := range row.col {
		if x == c {
			i = j
			break
		}
	}
	if i < 0 {
		// unreachable
		bug("can't find column")
	}
	r := c.r
	if dofree {
		a.colcloseall(c)
	}
	row.col = append(row.col[:i], row.col[i+1:]...)
	if row.ncol() == 0 {
		a.screen.Draw(r, a.disp.White, nil, draw.ZP)
		return
	}
	if i == row.ncol() { // extend last column right
		c = row.col[i-1]
		r.Min.X = c.r.Min.X
		r.Max.X = row.r.Max.X
	} else { // extend next window left
		c = row.col[i]
		r.Max.X = c.r.Max.X
	}
	a.screen.Draw(r, a.disp.White, nil, draw.ZP)
	a.colresize(c, r)
}

// rowwhichcol returns the column containing point p, or nil (rows.c).
func (a *Acme) rowwhichcol(row *Row, p draw.Point) *Column {
	for _, c := range row.col {
		if p.In(c.r) {
			return c
		}
	}
	return nil
}

// rowwhich returns the Text under point p: the row tag, or the Text within the
// containing column (rows.c).
func (a *Acme) rowwhich(row *Row, p draw.Point) *Text {
	if p.In(row.tag.all) {
		return &row.tag
	}
	if c := a.rowwhichcol(row, p); c != nil {
		return a.colwhich(c, p)
	}
	return nil
}

// runeError is the replacement rune substituted for a 0 keystroke (C Runeerror,
// runetype.h 0xFFFD).
const runeError = 0xFFFD

// rowtype routes a typed rune r (at mouse point p) to the text under the mouse
// -- or to barttext when a button-chord is active -- and types it there,
// expanding a tag when a newline is typed into it (rows.c rowtype). It returns
// the text that received the key. The row lock serializes the target lookup;
// winlock(w,'K') guards the per-window edit.
func (a *Acme) rowtype(row *Row, r rune, p draw.Point) *Text {
	if r == 0 {
		// untested: malformed-dump / interactive-drag path not exercised by -D
		r = runeError
	}
	a.clearmouse()
	row.lk.Lock()
	var t *Text
	if a.bartflag {
		// untested: malformed-dump / interactive-drag path not exercised by -D
		t = a.barttext
	} else {
		t = a.rowwhich(row, p)
	}
	if t != nil && !(t.what == Tag && p.In(t.scrollr)) {
		w := t.w
		if w == nil {
			// untested: malformed-dump / interactive-drag path not exercised by -D
			a.texttype(t, r)
		} else {
			a.winlock(w, 'K')
			a.wintype(w, t, r)
			// Expand tag if necessary.
			if t.what == Tag {
				t.w.tagsafe = false
				if r == '\n' {
					// untested: malformed-dump / interactive-drag path not exercised by -D
					t.w.tagexpand = true
				}
				a.winresize(w, w.r, true, true)
			}
			a.winunlock(w)
		}
	}
	row.lk.Unlock()
	return t
}

// dwhich prints the Text under point p (rowwhich), naming it by kind and window
// id, for the -D `which` verb.
func (a *Acme) dwhich(p draw.Point) {
	t := a.rowwhich(&a.row, p)
	switch {
	case t == nil:
		a.dprintf("which none\n")
	case t.what == Rowtag:
		a.dprintf("which rowtag\n")
	case t.what == Columntag:
		a.dprintf("which coltag\n")
	case t.what == Tag:
		a.dprintf("which %d tag\n", t.w.id)
	default: // Body
		a.dprintf("which %d body\n", t.w.id)
	}
}

// rowdragcol handles a button on a column's tag: it tracks the mouse until
// release, then either shuffles the column to a new position in the row or
// resizes it against its left neighbour (rows.c rowdragcol). The mouse is the
// scripted synthetic Mousectl under -D.
func (a *Acme) rowdragcol(row *Row, c *Column, _ int) {
	a.clearmouse()
	a.disp.SwitchCursor2(&boxcursor, &boxcursor2)
	mc := a.mousectl
	b := mc.Buttons
	op := mc.Point
	for mc.Buttons == b {
		mc.Read()
		a.mousexy = mc.Point // C's mouse aliases mousectl->m; keep them in sync
	}
	a.disp.SwitchCursor(nil)
	if mc.Buttons != 0 {
		// a second button pressed mid-drag aborts it
		for mc.Buttons != 0 {
			mc.Read()
			a.mousexy = mc.Point
		}
		return
	}

	i := -1
	for j, cc := range row.col {
		if cc == c {
			i = j
			break
		}
	}
	if i < 0 {
		// unreachable: c is always a column of row
		bug("can't find column")
	}
	p := mc.Point
	if absInt(p.X-op.X) < 5 && absInt(p.Y-op.Y) < 5 {
		return
	}
	if (i > 0 && p.X < row.col[i-1].r.Min.X) || (i < row.ncol()-1 && p.X > c.r.Max.X) {
		// shuffle, with fall-backs if the new position is rejected
		x := c.r.Min.X
		a.rowclose(row, c, false)
		if a.rowadd(row, c, p.X) == nil {
			// untested: rowadd at the drop x succeeds for a valid shuffle, so
			// these recovery re-insertions are not reached.
			// untested: malformed-dump / interactive-drag path not exercised by -D
			if a.rowadd(row, c, x) == nil {
				// unreachable: the first rowadd already succeeded
				if a.rowadd(row, c, -1) == nil {
					// unreachable: at least one slot always takes it back
					a.rowclose(row, c, true)
					return
				}
			}
		}
		a.colmousebut(c)
		return
	}
	if i == 0 {
		return
	}
	// resize against the left neighbour d
	d := row.col[i-1]
	if p.X < d.r.Min.X+80+a.scrollwid() {
		p.X = d.r.Min.X + 80 + a.scrollwid()
	}
	if p.X > c.r.Max.X-80-a.scrollwid() {
		p.X = c.r.Max.X - 80 - a.scrollwid()
	}
	r := d.r
	r.Max.X = c.r.Max.X
	a.screen.Draw(r, a.disp.White, nil, draw.ZP)
	r.Max.X = p.X
	a.colresize(d, r)
	r = c.r
	r.Min.X = p.X
	r.Max.X = r.Min.X + a.border()
	a.screen.Draw(r, a.disp.Black, nil, draw.ZP)
	r.Min.X = r.Max.X
	r.Max.X = c.r.Max.X
	a.colresize(c, r)
	a.colmousebut(c)
}
