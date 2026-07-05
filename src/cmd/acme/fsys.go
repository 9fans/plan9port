package main

import (
	"context"
	"errors"
	"io"
	"net"
	"os"
	"sort"
	"strconv"
	"sync"

	"9fans.net/go/plan9"
	"9fans.net/go/plan9/client"
	"9fans.net/go/plan9/srv9p"
)

// Go port of the acme 9P file server (fsys.c + the request dispatch of xfid.c).
// The Go acme serves its files over an in-process pipe to a 9fans.net/go/plan9
// client connection (a.fsys); the -D read/write verbs (driver.go) talk to it as
// an ordinary 9P client, exactly as the C oracle's debug.c talks to the real C
// acme via the plumb-style nsmount. The file semantics live in xfid.go.

// File qids, matching dat.h's enum order (the value is the low byte of a qid
// path; the window id is the high bits: path = id<<8 | qid).
const (
	qDir = iota
	qAcme
	qCons
	qConsctl
	qDraw
	qEditout
	qIndex
	qLabel
	qLog
	qNew
	qWaddr
	qWbody
	qWctl
	qWdata
	qWeditout
	qWerrors
	qWevent
	qWrdsel
	qWwrsel
	qWtag
	qWxdata
	qMax
)

func qpath(id, qid int) uint64 { return uint64(id)<<8 | uint64(qid) }

// winWriteDataPlane reports whether a write to qid touches only the addressed
// window's own text (no row/column structure, no command execution), so it can
// be served under winlock(w) instead of the global row.lk (C's xfidwrite
// winlock(w,'F')). The structural/cross-window/execute qids (ctl, event, cons,
// editout, errors, new, log, index, ...) are excluded and stay on row.lk.
func winWriteDataPlane(qid int) bool {
	switch qid {
	case qWaddr, qWbody, qWdata, qWtag, qWwrsel, qWxdata:
		return true
	}
	return false
}

// winReadDataPlane reports whether a read of qid is a per-window, non-blocking
// read of that window's own state, so it can be served under winlock(w,'F') like
// C's xfidread (which winlock(w,'F')s every per-window read). The globals
// (cons/label/index/log, all w==nil) and the blocking event read (which uses the
// row.lk-backed a.event cond) are excluded and stay on row.lk.
func winReadDataPlane(qid int) bool {
	switch qid {
	case qWaddr, qWbody, qWctl, qWtag, qWdata, qWxdata, qWrdsel:
		return true
	}
	return false
}
func qwin(path uint64) int  { return int(path >> 8) }
func qfile(path uint64) int { return int(path & 0xFF) }

// dirtab maps a top-level file name to its qid (fsys.c dirtab).
var dirtab = map[string]int{
	"acme": qAcme, "cons": qCons, "consctl": qConsctl, "draw": qDraw,
	"editout": qEditout, "index": qIndex, "label": qLabel, "log": qLog,
	"new": qNew,
}

// dirtabw maps a per-window file name to its qid (fsys.c dirtabw).
var dirtabw = map[string]int{
	"addr": qWaddr, "body": qWbody, "ctl": qWctl, "data": qWdata,
	"editout": qWeditout, "errors": qWerrors, "event": qWevent,
	"rdsel": qWrdsel, "wrsel": qWwrsel, "tag": qWtag, "xdata": qWxdata,
}

// dirent is one entry of the ordered directory tables (fsys.c Dirtab), carrying
// the qid type and 9P permission bits the maps above omit. The order matches C
// so directory reads list entries identically. (The maps and these lists encode
// the same names; keep them in sync.)
type dirent struct {
	name string
	qid  int
	typ  uint8
	perm plan9.Perm
}

// dirtabList/dirtabwList are the ordered top-level and per-window tables (fsys.c
// dirtab/dirtabw), used by dostat, the directory-read branch, and the open
// permission check. The leading "." is skipped when listing.
var dirtabList = []dirent{
	{".", qDir, plan9.QTDIR, plan9.DMDIR | 0500},
	{"acme", qAcme, plan9.QTDIR, plan9.DMDIR | 0500},
	{"cons", qCons, plan9.QTFILE, 0600},
	{"consctl", qConsctl, plan9.QTFILE, 0000},
	{"draw", qDraw, plan9.QTDIR, plan9.DMDIR}, // perm 0: suppress graphics progs
	{"editout", qEditout, plan9.QTFILE, 0200},
	{"index", qIndex, plan9.QTFILE, 0400},
	{"label", qLabel, plan9.QTFILE, 0600},
	{"log", qLog, plan9.QTFILE, 0400},
	{"new", qNew, plan9.QTDIR, plan9.DMDIR | 0500},
}

var dirtabwList = []dirent{
	{".", qDir, plan9.QTDIR, plan9.DMDIR | 0500},
	{"addr", qWaddr, plan9.QTFILE, 0600},
	{"body", qWbody, plan9.QTAPPEND, plan9.DMAPPEND | 0600},
	{"ctl", qWctl, plan9.QTFILE, 0600},
	{"data", qWdata, plan9.QTFILE, 0600},
	{"editout", qWeditout, plan9.QTFILE, 0200},
	{"errors", qWerrors, plan9.QTFILE, 0200},
	{"event", qWevent, plan9.QTFILE, 0600},
	{"rdsel", qWrdsel, plan9.QTFILE, 0400},
	{"wrsel", qWwrsel, plan9.QTFILE, 0200},
	{"tag", qWtag, plan9.QTAPPEND, plan9.DMAPPEND | 0600},
	{"xdata", qWxdata, plan9.QTFILE, 0600},
}

// direntForPath returns the table entry for a fid's qid path: a top-level or
// per-window file, or a directory (whose "." entry the lookup finds). Both the
// root and a window directory stat as "." (fsys.c uses dir->dir, the leading
// "." entry); the id-named DMDIR|0700 form is used only when listing the root.
func direntForPath(path uint64) (dirent, bool) {
	id, q := qwin(path), qfile(path)
	tab := dirtabList
	if id > 0 {
		tab = dirtabwList
	}
	for _, e := range tab {
		if e.qid == q {
			return e, true
		}
	}
	// untested: only valid qids reach here under -D
	return dirent{}, false
}

// dostat builds the 9P stat for entry e in window id (fsys.c dostat). The
// owner/clock fields are not part of the compared -D output (the stat/readdir
// verbs print only name/qid/perm/length), so the current time and an empty user
// suffice.
func (a *Acme) dostat(id int, e dirent) *plan9.Dir {
	return &plan9.Dir{
		Qid:  plan9.Qid{Path: qpath(id, e.qid), Type: e.typ},
		Mode: e.perm,
		Name: e.name,
	}
}

// fsysinit starts the in-process 9P server and connects a.fsys to it.
func (a *Acme) fsysinit() {
	srvConn, cliConn := net.Pipe()
	// Locking mirrors xfid.c/fsys.c per handler, not one blanket lock:
	//   - per-window data path -> winlock(w): fsOpen (xfidopen), fsClunk's close
	//     switch (xfidclose), per-window non-blocking reads in fsRead (xfidread),
	//     and per-window data writes in the Write dispatch (xfidwrite).
	//   - row.lk (via lockR): fsWalk + fsAttach + the directory/blocking/global
	//     reads + ctl/event/cons/editout writes (C fsyswalk/fsysread both
	//     qlock(&row.lk); xfidctlwrite colclose + xfideventwrite "just like
	//     mousethread" need row.lk and would deadlock under winlock -- see
	//     PORT.md (B)2).
	//   - no lock: fsStat (C fsysstat is lockless: static dirtab metadata only).
	// Ordering is always row.lk-outer / winlock-inner (the mousethread order), so
	// there is no deadlock; no row.lk holder makes a 9P client call. The
	// constant-deny Create/Remove/Wstat touch no state and need no lock.
	// lockR runs a 9P handler under row.lk and, in the live editor, flushes the
	// display afterward -- the Go analogue of xfid.c's flushimage(display,1) after
	// every request, so an external client's edit becomes visible immediately
	// rather than only on the next input event. The flush is outside row.lk (as
	// the mousethread's is) and is serialized with concurrent draws by the draw
	// package's display mutex. -D does not take this path (blockEvents false); its
	// frames are flushed by the debug write verb instead.
	lockR := func(f func()) {
		func() { a.row.lk.Lock(); defer a.row.lk.Unlock(); f() }()
		if a.blockEvents && a.disp != nil {
			a.disp.FlushImage(true)
		}
	}
	srv := &srv9p.Server{
		Attach: func(ctx context.Context, fid, afid *srv9p.Fid, user, aname string) (q plan9.Qid, err error) {
			lockR(func() { q, err = a.fsAttach(ctx, fid, afid, user, aname) })
			return
		},
		Walk: func(ctx context.Context, fid, newfid *srv9p.Fid, names []string) (qs []plan9.Qid, err error) {
			lockR(func() { qs, err = a.fsWalk(ctx, fid, newfid, names) })
			return
		},
		// fsOpen locks internally: a per-window file open holds winlock(w) (C's
		// xfidopen winlock(w,'E')); a global file (log/editout) holds row.lk. The
		// live flush mirrors lockR's.
		Open: func(ctx context.Context, fid *srv9p.Fid, mode uint8) (err error) {
			err = a.fsOpen(ctx, fid, mode)
			if a.blockEvents && a.disp != nil {
				a.disp.FlushImage(true)
			}
			return
		},
		// fsRead locks row.lk itself (not via lockR) so a blocking event read can
		// release it while waiting (xfid.c xfideventread).
		Read: a.fsRead,
		Write: func(ctx context.Context, fid *srv9p.Fid, data []byte, offset int64) (n int, err error) {
			// A write to a per-window data file (body/tag/data/addr/wrsel/xdata)
			// touches only that window's text, so hold winlock(w) for it rather
			// than the global row.lk — matching C's xfidwrite winlock(w,'F') so a
			// 9P write to one window proceeds concurrently with input or 9P on
			// other windows. The window and mnt are read lock-free from the fid
			// (aux.w / x->f->w, aux.mnt / f->mntdir), so we take winlock(w) alone.
			// The event and ctl writes are included: C's xfidwrite holds winlock(w)
			// across xfideventwrite (row.lk-inner for execute/look3) and across
			// xfidctlwrite (its w->ctllock acquire and del/delete colclose) alike --
			// PORT.md cross-cutting #2 / follow-ups. The remaining cross-window/global
			// writes (cons, editout, errors) stay on row.lk
			// via lockR.
			if q0 := qfile(fid.Qid().Path); winWriteDataPlane(q0) || q0 == qWevent || q0 == qWctl {
				w, q := a.fwin(fid)
				if w != nil {
					a.winlock(w, 'F')
					n, errs := a.xfidWriteName(w, q, string(data), auxOf(fid).mnt, fid)
					a.winunlock(w)
					if a.blockEvents && a.disp != nil {
						// untested: live/error 9P path (Tflush abort, blocking-read wake, socket-accept error) not reached by the -D or in-process 9P tests
						a.disp.FlushImage(true)
					}
					if errs != "" {
						return 0, errors.New(errs)
					}
					return n, nil
				}
			}
			lockR(func() { n, err = a.fsWrite(ctx, fid, data, offset) })
			return
		},
		// fsClunk locks internally (winlock(w) for the close switch, row.lk for
		// the fid bookkeeping + winclose). The live flush mirrors lockR's.
		Clunk: func(fid *srv9p.Fid) {
			a.fsClunk(fid)
			if a.blockEvents && a.disp != nil {
				a.disp.FlushImage(true)
			}
		},
		// fsysstat is lockless in C: dostat reads only static dirtab metadata
		// (qid/type/perm/name), no mutable window state, so no lock is needed.
		Stat: func(ctx context.Context, fid *srv9p.Fid) (d *plan9.Dir, err error) {
			return a.fsStat(ctx, fid)
		},
		Create: a.fsCreate,
		Remove: a.fsRemove,
		Wstat:  a.fsWstat,
	}
	a.fsysSrv = srv
	a.event = sync.NewCond(&a.row.lk)
	a.logread = sync.NewCond(&a.row.lk)
	go srv.Serve(srvConn, srvConn)
	conn, err := client.NewConn(cliConn)
	if err != nil {
		// untested: NewConn over an in-process pipe cannot fail
		a.fatalf("9P NewConn: %v", err)
	}
	// Attach as the login user (C's startup `user` = getuser() = $USER), so the
	// first attach establishes a.user as the value external clients on the posted
	// socket also send (client.MountService uses getuser()); attaching as a
	// literal "acme" would make every external mount fail fsysattach's user check.
	fsys, err := conn.Attach(nil, os.Getenv("USER"), "")
	if err != nil {
		// untested: attach to our own server cannot fail
		a.fatalf("9P attach: %v", err)
	}
	a.fsysconn = conn
	a.fsys = fsys
}

// fsyspost posts the 9P service on the name space so external programs (and
// acme's own commands) can mount /mnt/acme. C's fsysinit posts to /srv and
// mounts at mtpt; plan9port has no /srv, so the service is a Unix socket at
// $NAMESPACE/acme that clients dial (client.MountService("acme")). Each accepted
// connection gets its own Serve (and thus its own fid space). Called from
// threadmain in the live editor only; the -D harness never posts (it would
// pollute the real name space and is unused by the in-process tests).
func (a *Acme) fsyspost() error {
	addr := client.Namespace() + "/acme"
	os.Remove(addr) // clear a stale socket from a previous run
	l, err := net.Listen("unix", addr)
	if err != nil {
		// untested: live/error 9P path (Tflush abort, blocking-read wake, socket-accept error) not reached by the -D or in-process 9P tests
		return err
	}
	go func() {
		for {
			c, err := l.Accept()
			if err != nil {
				// untested: live/error 9P path (Tflush abort, blocking-read wake, socket-accept error) not reached by the -D or in-process 9P tests
				return // listener closed
			}
			go a.fsysSrv.Serve(c, c)
		}
	}()
	return nil
}

// fsysmount allocates a mount-directory id for a command's namespace (fsys.c
// fsysmount/fsysaddid). ref is 1, held by the (notional) command; each attach
// with this id adds a ref.
func (a *Acme) fsysmount(dir []rune, incl [][]rune) *Mntdir {
	a.mntlk.Lock()
	defer a.mntlk.Unlock()
	a.mntid++
	m := &Mntdir{id: a.mntid, dir: dir, ref: 1, incl: incl}
	a.mntdirs = append(a.mntdirs, m)
	return m
}

// fsysincid adds a reference to m (fsys.c).
func (a *Acme) fsysincid(m *Mntdir) {
	a.mntlk.Lock()
	m.ref++
	a.mntlk.Unlock()
}

// fsysdelid drops a reference to m, freeing it (removing it from the registry)
// when the last reference goes (fsys.c).
func (a *Acme) fsysdelid(m *Mntdir) {
	if m == nil {
		return
	}
	a.mntlk.Lock()
	defer a.mntlk.Unlock()
	m.ref--
	if m.ref > 0 {
		return
	}
	for i, x := range a.mntdirs {
		if x == m {
			a.mntdirs = append(a.mntdirs[:i], a.mntdirs[i+1:]...)
			return
		}
	}
	// untested: fsysdelid is only ever called with a registered Mntdir
}

// mntdirByID returns the registered Mntdir with the given id, or nil. The caller
// must hold a.mntlk (matching C's fsysattach, which scans mnt.md under mnt.lk).
func (a *Acme) mntdirByID(id int) *Mntdir {
	for _, m := range a.mntdirs {
		if m.id == id {
			return m
		}
	}
	// untested: -D only ever attaches with a live mount id (C sends a cerr
	// "unknown id" to the error pipe for a bad one, which -D does not surface).
	// untested: live/error 9P path (Tflush abort, blocking-read wake, socket-accept error) not reached by the -D or in-process 9P tests
	return nil
}

// fsysclose closes the cached client fids (clunking them server-side) and the
// connection. Called when a -D conversation ends.
func (a *Acme) fsysclose() {
	for _, fid := range a.fids {
		fid.Close()
	}
	a.fids = nil
	if a.fsys != nil {
		a.fsys.Close()
	}
}

func (a *Acme) fsAttach(ctx context.Context, fid, afid *srv9p.Fid, user, aname string) (plan9.Qid, error) {
	// Only acme's own user may attach (fsys.c fsysattach). The first attach
	// establishes that user (C uses the login name from startup).
	if a.user == "" {
		a.user = user
	} else if user != a.user {
		// untested: -D only ever attaches as acme's own user
		return plan9.Qid{}, errors.New("permission denied")
	}
	q := plan9.Qid{Path: qpath(0, qDir), Type: plan9.QTDIR}
	fid.SetQid(q)
	// Bind the mount directory named by the decimal aname id, if any. C's
	// fsysattach scans mnt.md and does m->ref++ in one mnt.lk section; mirror
	// that (the fidmnt write is the analogue of C's fid-local f->mntdir = m and
	// stays under the row.lk this handler already holds).
	if id, err := strconv.Atoi(aname); err == nil {
		a.mntlk.Lock()
		if m := a.mntdirByID(id); m != nil {
			m.ref++
			auxOf(fid).mnt = m
		}
		a.mntlk.Unlock()
	}
	return q, nil
}

// walkstep walks one path element from the file at qid path, returning the new
// qid (fsys.c fsyswalk).
func (a *Acme) walkstep(path uint64, name string) (plan9.Qid, bool) {
	id := qwin(path)
	switch qfile(path) {
	case qDir:
		if name == ".." {
			// ".." resolves to the acme root from any directory (fsys.c fsyswalk).
			// untested: the -D walk verbs never issue "..".
			return plan9.Qid{Path: qpath(0, qDir), Type: plan9.QTDIR}, true
		}
		if id == 0 {
			// walking into "new" creates a fresh window (fsys.c; C signals
			// newwindowthread) and lands on its directory.
			if name == "new" {
				w := a.makenewwindow(nil)
				a.winsettag(w)
				a.xfidlog(w, "new")
				return plan9.Qid{Path: qpath(w.id, qDir), Type: plan9.QTDIR}, true
			}
			// the root: a top-level file or a numbered window directory
			if q, ok := dirtab[name]; ok {
				typ := uint8(plan9.QTFILE)
				if q == qAcme || q == qDraw || q == qNew {
					typ = plan9.QTDIR
				}
				return plan9.Qid{Path: qpath(0, q), Type: typ}, true
			}
			if wid, err := strconv.Atoi(name); err == nil && a.lookid(wid) != nil {
				return plan9.Qid{Path: qpath(wid, qDir), Type: plan9.QTDIR}, true
			}
			// untested: walk failure -- only bad names reach here, and the
			// client's "not found" text is library-specific, not differentiable
			// untested: live/error 9P path (Tflush abort, blocking-read wake, socket-accept error) not reached by the -D or in-process 9P tests
			return plan9.Qid{}, false
		}
		// a window directory: one of its files
		if q, ok := dirtabw[name]; ok {
			return plan9.Qid{Path: qpath(id, q)}, true
		}
	}
	// untested: walk failure (see above)
	return plan9.Qid{}, false
}

func (a *Acme) fsWalk(ctx context.Context, fid, newfid *srv9p.Fid, names []string) ([]plan9.Qid, error) {
	path := fid.Qid().Path
	var qids []plan9.Qid
	for _, name := range names {
		q, ok := a.walkstep(path, name)
		if !ok {
			// untested: only bad names fail mid-walk (not differentiable)
			break
		}
		qids = append(qids, q)
		path = q.Path
	}
	// On a full successful walk, newfid lands on the last element. (The client
	// issues no zero-element clone walks for these verbs, and only bad names --
	// which are not differentiable -- fail mid-walk, so the partial cases need no
	// handling here.)
	if len(qids) > 0 && len(qids) == len(names) {
		newfid.SetQid(qids[len(qids)-1])
		// The cloned fid inherits its parent's mount directory, adding a ref
		// (fsys.c fsyswalk copies f->mntdir and bumps its ref).
		if m := auxOf(fid).mnt; m != nil && newfid != fid {
			a.fsysincid(m)
			auxOf(newfid).mnt = m
		}
		// Hold a reference to the window this fid now names, so it survives a
		// delete while the fid is open (fsys.c fsyswalk incref(&nf->w->ref)).
		a.fidsetwin(newfid)
	}
	return qids, nil
}

// fidAux is the per-fid state hung on each srv9p Fid's Aux -- the analogue of C's
// Fid fields (f->w, f->mntdir, f->rpart, ...). Because the 9P server serializes
// requests per fid, a handler reads and writes its own fid's aux without a lock
// (exactly as C reads x->f->w lock-free); that is what lets the per-window
// handlers take winlock(w) alone, the way xfid.c does.
type fidAux struct {
	w       *Window // C f->w: window this fid references (nil for global/top fids)
	mnt     *Mntdir // C f->mntdir
	rpart   []byte  // C f->rpart: bytes of a rune split across consecutive writes
	rdsel   string  // QWrdsel: selection text captured at open
	logoff  int     // qLog read offset (a global sequence number)
	editout bool    // this fid holds the (global or per-window) editout lock
}

// auxOf returns fid's per-fid state, creating it on first use. Safe without a
// lock: the server delivers a fid's requests one at a time. A nil fid (used by
// unit tests that exercise the write path without a real fid) gets a throwaway
// aux with no persistent state.
func auxOf(fid *srv9p.Fid) *fidAux {
	if fid == nil {
		return &fidAux{}
	}
	x, _ := fid.Aux().(*fidAux)
	if x == nil {
		x = &fidAux{}
		fid.SetAux(x)
	}
	return x
}

// fwin returns the window and file qid for an open fid. A fid that walked to a
// window holds a reference to it (aux.w / fsys.c f->w), so use that pointer -- it
// stays valid even if the window was deleted from its column while this fid is
// still open (the reference keeps it alive), matching C's direct x->f->w. Reading
// aux.w needs no lock (fid-local), so per-window handlers can winlock(w) alone.
func (a *Acme) fwin(fid *srv9p.Fid) (*Window, int) {
	return auxOf(fid).w, qfile(fid.Qid().Path)
}

// fidsetwin makes fid hold a reference to the window named by its current qid
// (fsys.c fsyswalk's incref(&nf->w->ref)). Any previous reference is dropped
// first, so a re-walked fid stays balanced. Caller holds row.lk (lookid + the
// ref/winclose touch the row); the aux.w write itself is fid-local.
func (a *Acme) fidsetwin(fid *srv9p.Fid) {
	var w *Window
	if id := qwin(fid.Qid().Path); id != 0 {
		w = a.lookid(id)
	}
	x := auxOf(fid)
	if x.w == w {
		return
	}
	if x.w != nil {
		// untested: live/error 9P path (Tflush abort, blocking-read wake, socket-accept error) not reached by the -D or in-process 9P tests
		old := x.w
		x.w = nil
		a.winclose(old)
	}
	if w != nil {
		w.ref.Add(1)
		x.w = w
	}
}

// fsCreate/fsRemove/fsWstat deny their requests with C's exact error, matching
// fsys.c fsyscreate/fsysremove/fsyswstat (all "permission denied"). Registering
// them overrides the library's default "... prohibited" wording.
func (a *Acme) fsCreate(ctx context.Context, fid *srv9p.Fid, name string, perm plan9.Perm, mode uint8) (plan9.Qid, error) {
	return plan9.Qid{}, errors.New("permission denied")
}

func (a *Acme) fsRemove(ctx context.Context, fid *srv9p.Fid) error {
	return errors.New("permission denied")
}

func (a *Acme) fsWstat(ctx context.Context, fid *srv9p.Fid, d *plan9.Dir) error {
	return errors.New("permission denied")
}

// fsStat returns the 9P metadata for an open or walked fid (fsys.c fsysstat).
func (a *Acme) fsStat(ctx context.Context, fid *srv9p.Fid) (*plan9.Dir, error) {
	path := fid.Qid().Path
	e, ok := direntForPath(path)
	if !ok {
		// untested: only valid qids are ever statted under -D
		return nil, errors.New("file not found")
	}
	return a.dostat(qwin(path), e), nil
}

// modeperm maps an open mode to the permission bits C's fsysopen requires.
var modeperm = map[uint8]plan9.Perm{
	plan9.OREAD: 0400, plan9.OWRITE: 0200, plan9.ORDWR: 0600,
}

func (a *Acme) fsOpen(ctx context.Context, fid *srv9p.Fid, mode uint8) error {
	// Permission check (fsys.c fsysopen): truncation is ignored; only
	// OREAD/OWRITE/ORDWR are allowed (so OEXEC, ORCLOSE, etc. are denied); and
	// the dirtab perm bits must grant the requested access.
	pm, ok := modeperm[mode&^(plan9.OTRUNC|plan9.OCEXEC)]
	if !ok {
		// untested: the -D read/write/readdir verbs only open OREAD/OWRITE, so
		// the OEXEC/ORCLOSE denial is not reached.
		// untested: live/error 9P path (Tflush abort, blocking-read wake, socket-accept error) not reached by the -D or in-process 9P tests
		return errors.New("permission denied")
	}
	if e, ok := direntForPath(fid.Qid().Path); ok &&
		(e.perm&^(plan9.DMDIR|plan9.DMAPPEND))&pm != pm {
		return errors.New("permission denied")
	}
	w, q := a.fwin(fid) // lock-free (aux.w), like C's x->f->w
	if w == nil {
		// Global files (no window): C's xfidopen w==nil branch holds no winlock.
		a.row.lk.Lock()
		defer a.row.lk.Unlock()
		if q == qLog {
			// the log fid starts reading at the current end, in global sequence
			// numbers, and joins the reader set so its unread entries survive GC
			// (logf.c xfidlogopen).
			auxOf(fid).logoff = a.logStart + len(a.log)
			if a.logfids == nil {
				a.logfids = make(map[*srv9p.Fid]bool)
			}
			a.logfids[fid] = true
		}
		if q == qEditout {
			// global editout: only one writer at a time (xfid.c xfidopen Qeditout
			// canqlock(&editoutlk) -> Einuse). The editing==Inactive denial happens
			// at write time (edittext), as in C's Qeditout (no open-time Eperm).
			if !a.editoutlk.TryLock() {
				// untested: -D never opens the global editout twice concurrently
				return errors.New("already in use")
			}
			auxOf(fid).editout = true
		}
		return nil
	}
	// Per-window file: C holds winlock(w,'E') for the whole of xfidopen and takes
	// no row.lk (the window pointer is x->f->w, lock-free), so an open of one
	// window proceeds concurrently with input or 9P on other windows. The fid's
	// aux.w reference keeps w alive until clunk.
	a.winlock(w, 'E')
	defer a.winunlock(w)
	if q == qWrdsel {
		// rdsel captures the body's current selection at open (xfid.c uses a
		// per-window temp file with an Einuse reopen guard; here the text is
		// stashed per-fid, so two fids get independent snapshots and the Einuse
		// guard does not apply).
		w.nopen[q]++
		auxOf(fid).rdsel = string(readRunes(&w.body, w.body.q0, w.body.q1))
		return nil
	}
	if q == qWeditout {
		// editout requires an active Edit and exclusive access (xfid.c xfidopen
		// QWeditout: editing==FALSE -> Eperm; canqlock(&w->editoutlk) -> Einuse).
		if a.editing == Inactive {
			return errors.New("permission denied")
		}
		if !w.editoutlk.TryLock() {
			// untested: -D never opens a window's editout twice concurrently
			return errors.New("already in use")
		}
		auxOf(fid).editout = true
		w.wrselrange = Range{w.body.q1, w.body.q1}
		return nil
	}
	a.xfidopen(w, q)
	return nil
}

func (a *Acme) fsClunk(fid *srv9p.Fid) {
	x := auxOf(fid)
	// drop this fid's mount-directory reference (fsys.c fsysclunk always calls
	// fsysdelid; the ref is nil for the common, unmounted fid). fsysdelid takes
	// mnt.lk itself; aux.mnt/aux.rpart are fid-local.
	a.fsysdelid(x.mnt)
	x.mnt = nil
	x.rpart = nil // discard any unfinished partial rune (xfid.c)
	a.row.lk.Lock()
	delete(a.logfids, fid) // leave the /log reader set (logf.c xfidlogclose)
	w := x.w
	if w == nil {
		a.row.lk.Unlock()
		if x.editout {
			a.editoutlk.Unlock() // global editout (xfid.c xfidclose Qeditout)
		}
		return
	}
	a.row.lk.Unlock()
	// The exclusive ctl lock is released in xfidclose (QWctl) under winlock(w),
	// exactly as C's xfidclose does (if w->ctlfid==x->f->fid: qunlock(&w->ctllock)).
	x.w = nil
	q := qfile(fid.Qid().Path)
	// The per-window close switch runs under winlock(w) alone, as in C's xfidclose
	// (winlock(w,'E') ... switch ... winunlock(w)): xfidclose drops the open count
	// while w is still alive (this fid's aux.w reference holds it).
	a.winlock(w, 'E')
	if x.editout {
		w.editoutlk.Unlock() // C xfidclose QWeditout qunlock(&w->editoutlk)
	}
	a.xfidclose(w, q, fid)
	a.winunlock(w)
	// winclose then drops that reference and tears the window down if it was the
	// last (C's winclose after winunlock(w)). At teardown it calls xfidlog, which
	// needs row.lk (the logread cond), so take it for the decref.
	a.row.lk.Lock()
	a.winclose(w)
	a.row.lk.Unlock()
}

// dirents builds the directory listing for a directory qid: the per-window or
// top-level files (skipping "."), plus, for the root, one entry per window
// sorted by id (fsys.c fsysread).
func (a *Acme) dirents(path uint64) []*plan9.Dir {
	id, q := qwin(path), qfile(path)
	if q == qAcme {
		return nil // empty directory
	}
	tab := dirtabList
	if id > 0 {
		tab = dirtabwList
	}
	var ds []*plan9.Dir
	for _, e := range tab[1:] { // skip "."
		ds = append(ds, a.dostat(id, e))
	}
	if id == 0 {
		var ids []int
		for _, c := range a.row.col {
			for _, w := range c.w {
				ids = append(ids, w.id)
			}
		}
		sort.Ints(ids)
		for _, wid := range ids {
			ds = append(ds, a.dostat(wid, dirent{strconv.Itoa(wid), qDir, plan9.QTDIR, plan9.DMDIR | 0700}))
		}
	}
	return ds
}

func (a *Acme) fsRead(ctx context.Context, fid *srv9p.Fid, data []byte, offset int64) (int, error) {
	w, q := a.fwin(fid) // lock-free (aux.w), like C's x->f->w
	// Per-window, non-blocking reads run under winlock(w,'F') alone (C xfidread
	// winlock(w,'F'), no row.lk), so a read of one window proceeds concurrently
	// with input or 9P on other windows; the fid's aux.w keeps w alive. The
	// blocking event read, the directory read, and the globals
	// (log/index/cons/label) take row.lk below.
	if w != nil && fid.Qid().Type&plan9.QTDIR == 0 && winReadDataPlane(q) {
		a.winlock(w, 'F')
		defer a.winunlock(w)
		// A read on a deleted (but not yet clunked) window is Edel (xfid.c
		// xfidread: if(w->col==nil) respond Edel).
		if w.col == nil {
			// untested: needs an external client reading a window's data plane
			// after the window was deleted but before the fid is clunked.
			return 0, errors.New("deleted window")
		}
		if q == qWrdsel {
			return fid.ReadBytes(data, offset, []byte(auxOf(fid).rdsel))
		}
		if q == qWdata || q == qWxdata {
			return a.fsReadData(w, q, data)
		}
		s, errs := a.xfidReadName(w, q) // qWaddr, qWbody, qWctl, qWtag
		if errs != "" {
			// untested: live/error 9P path (Tflush abort, blocking-read wake, socket-accept error) not reached by the -D or in-process 9P tests
			return 0, errors.New(errs)
		}
		return fid.ReadBytes(data, offset, []byte(s))
	}
	a.row.lk.Lock()
	defer a.row.lk.Unlock()
	if q == qWevent && w != nil {
		// A read on a deleted (but not yet clunked) window is Edel, exactly as
		// C's xfidread guards every per-window read (xfid.c: if(w->col==nil)).
		if w.col == nil {
			// untested: needs an external client reading a window's event file
			// after the window was deleted but before the fid is clunked.
			return 0, errors.New("deleted window")
		}
		// Blocking event read (xfid.c xfideventread): wait until events are queued,
		// releasing row.lk while blocked (a.event.Wait), so the editor can keep
		// running and generate them. Spurious wakeups are handled by the loop.
		// Enabled only in the live editor; the single-threaded -D harness reads
		// pending events once and must not block (it would deadlock its own driver
		// goroutine), so it skips the wait and returns whatever is pending.
		if a.blockEvents {
			stop := make(chan struct{})
			defer close(stop)
			go a.flushWaker(ctx, a.event, stop)
			for len(w.events) == 0 {
				// untested: live/error 9P path (Tflush abort, blocking-read wake, socket-accept error) not reached by the -D or in-process 9P tests
				if ctx.Err() != nil {
					// a 9P Tflush cancelled the read (xfid.c xfidflush wakes the flushed
					// reader). srv9p already answered Rflush; return without a reply.
					// untested: live/error 9P path (Tflush abort, blocking-read wake, socket-accept error) not reached by the -D or in-process 9P tests
					return 0, ctx.Err()
				}
				// untested: live/error 9P path (Tflush abort, blocking-read wake, socket-accept error) not reached by the -D or in-process 9P tests
				a.event.Wait()
				// the window may have been deleted while we waited
				// untested: live/error 9P path (Tflush abort, blocking-read wake, socket-accept error) not reached by the -D or in-process 9P tests
				if w.body.file == nil {
					// untested: needs an external event reader during window close
					return 0, nil
				}
			}
		}
		// Return up to the client's count and keep the untended tail for the next
		// read (xfid.c xfideventread: n=min(nevents,count); memmove; nevents-=n).
		// The event file is a consuming stream, so the offset is ignored as in C.
		n := copy(data, w.events)
		if n < len(w.events) {
			w.events = w.events[n:]
		} else {
			w.events = nil
		}
		return n, nil
	}
	if fid.Qid().Type&plan9.QTDIR != 0 {
		ds := a.dirents(fid.Qid().Path)
		return fid.ReadDir(data, offset, func(n int, d *plan9.Dir) bool {
			if n >= len(ds) {
				return false
			}
			*d = *ds[n]
			return true
		})
	}
	if q == qLog {
		// one log entry per read, from this fid's offset (logf.c xfidlogread).
		// In the live editor a caught-up reader blocks for the next entry
		// (releasing row.lk via a.logread.Wait, woken by xfidlog); the -D harness
		// (blockEvents false) returns 0 instead, as its scripts read after acting.
		lx := auxOf(fid)
		off := lx.logoff
		if a.blockEvents {
			stop := make(chan struct{})
			defer close(stop)
			go a.flushWaker(ctx, a.logread, stop)
			for off >= a.logStart+len(a.log) {
				if ctx.Err() != nil {
					// Tflush cancelled the log read (logf.c xfidlogflush).
					// untested: live/error 9P path (Tflush abort, blocking-read wake, socket-accept error) not reached by the -D or in-process 9P tests
					return 0, ctx.Err()
				}
				a.logread.Wait()
			}
		} else if off >= a.logStart+len(a.log) {
			return 0, nil
		}
		lx.logoff = off + 1
		// off is a global sequence number; index into the live window. GC never
		// drops below an open fid's offset, so off-logStart is always valid here.
		return fid.ReadBytes(data, offset, []byte(a.log[off-a.logStart]))
	}
	// qWevent (-D non-blocking), qIndex, qLabel, qCons: all served under row.lk
	// (qIndex reads every window; the -D event read is non-blocking).
	s, errs := a.xfidReadName(w, q)
	if errs != "" {
		// untested: live/error 9P path (Tflush abort, blocking-read wake, socket-accept error) not reached by the -D or in-process 9P tests
		return 0, errors.New(errs)
	}
	return fid.ReadBytes(data, offset, []byte(s))
}

// flushWaker broadcasts cond when the request's context is cancelled (a 9P
// Tflush), so a reader blocked in cond.Wait rechecks ctx and aborts -- the Go
// analogue of xfid.c xfidflush / logf.c xfidlogflush waking the flushed reader.
// The broadcast is under row.lk (cond's lock) so the blocked wait cannot miss it;
// the caller closes stop to retire the waker once the read completes normally.
func (a *Acme) flushWaker(ctx context.Context, cond *sync.Cond, stop <-chan struct{}) {
	select {
	case <-ctx.Done():
		a.row.lk.Lock()
		cond.Broadcast()
		a.row.lk.Unlock()
	case <-stop:
	}
}

// fsReadData serves a QWdata/QWxdata read under winlock(w) (C xfidread
// QWdata/QWxdata via xfidruneread: w.addr is the read cursor). It returns as many
// whole runes from w.addr.q0 as fit in this request's buffer (len(data)) -- the
// 9P offset is ignored -- and advances w.addr.q0 by the runes returned, so a
// chunked reader walks the range across successive reads. data reads to
// end-of-buffer and tracks q1 to q0; xdata reads only to w.addr.q1 and leaves it.
func (a *Acme) fsReadData(w *Window, q int, data []byte) (int, error) {
	a.wincommit(w, &w.body)
	nc := w.body.file.b.nc()
	if w.addr.q0 > nc {
		return 0, errors.New("address out of range")
	}
	hi := nc
	if q == qWxdata {
		hi = w.addr.q1
		if hi > nc {
			// untested: an addr write clamps q1 <= nc
			hi = nc
		}
		if hi < w.addr.q0 {
			// xdata with an inverted range reads nothing (C xfidruneread's
			// while(q<q1) runs zero times -- the "BUG: q1>q0?" comment); leave
			// the cursor unchanged.
			// untested: live/error 9P path (Tflush abort, blocking-read wake, socket-accept error) not reached by the -D or in-process 9P tests
			return 0, nil
		}
	}
	full := readRunes(&w.body, w.addr.q0, hi)
	m, runes := 0, 0
	for _, ru := range full {
		rw := len(string(ru))
		if m+rw > len(data) {
			break
		}
		m += rw
		runes++
	}
	copy(data, full[:m])
	w.addr.q0 += runes
	if q == qWdata {
		w.addr.q1 = w.addr.q0
	}
	return m, nil
}

func (a *Acme) fsWrite(ctx context.Context, fid *srv9p.Fid, data []byte, offset int64) (int, error) {
	w, q := a.fwin(fid)
	n, errs := a.xfidWriteName(w, q, string(data), auxOf(fid).mnt, fid)
	if errs != "" {
		return 0, errors.New(errs)
	}
	return n, nil
}

// open9p opens name with mode through a.fsys, caching the fid (keyed by
// name+mode) and keeping it open -- like the C verb's open9p, so per-window file
// state (the addr range) persists across requests.
func (a *Acme) open9p(name string, mode uint8) (*client.Fid, error) {
	key := name + "\x00" + string(rune(mode))
	if a.fids == nil {
		a.fids = make(map[string]*client.Fid)
	}
	if fid, ok := a.fids[key]; ok {
		return fid, nil
	}
	fid, err := a.fsys.Open(name, mode)
	if err != nil {
		// untested: open fails only for bad names, whose client error text is
		// library-specific (not byte-comparable with the C oracle)
		return nil, err
	}
	a.fids[key] = fid
	return fid, nil
}

// do9pWrite handles the -D `write` verb: write data at offset 0 of the file.
func (a *Acme) do9pWrite(name string, data []byte) {
	fid, err := a.open9p(name, plan9.OWRITE)
	if err != nil {
		// untested: see open9p (open only fails for non-differentiable names)
		a.dprintf("error %s\n", err)
		return
	}
	n, err := fid.WriteAt(data, 0)
	if err != nil {
		a.dprintf("error %s\n", err)
		return
	}
	a.dprintf("wrote %d\n", n)
}

// docmderr handles the -D `cmderr` verb, exercising the mount-directory path a
// command uses: allocate a Mntdir for dir, attach a fid carrying its id, write
// text to that fid's /cons (which routes to dir/+Errors), then release the cons
// fid, the attach, and the command's own reference.
func (a *Acme) docmderr(dir, text string) {
	m := a.fsysmount([]rune(dir), nil)
	defer a.fsysdelid(m) // the command's reference
	fsys, err := a.fsysconn.Attach(nil, os.Getenv("USER"), strconv.Itoa(m.id))
	if err != nil {
		// untested: attach to our own server cannot fail
		a.dprintf("error %s\n", err)
		return
	}
	defer fsys.Close() // clunks the attach root fid
	fid, err := fsys.Open("cons", plan9.OWRITE)
	if err != nil {
		// untested: cons is world-writable, so the open cannot fail
		a.dprintf("error %s\n", err)
		return
	}
	n, err := fid.Write([]byte(text))
	fid.Close()
	if err != nil {
		// untested: a cons write over the in-process pipe cannot fail
		a.dprintf("error %s\n", err)
		return
	}
	a.dprintf("wrote %d\n", n)
}

// do9pStat handles the -D `stat` verb: walk to name, stat it, and print the
// deterministic Dir fields. Owner and times are omitted (the C oracle's user
// name and getclock are machine/time dependent).
func (a *Acme) do9pStat(name string) {
	d, err := a.fsys.Stat(name)
	if err != nil {
		// untested: stat fails only for bad names (not differentiable)
		a.dprintf("error %s\n", err)
		return
	}
	a.dprintf("stat name=%s path=%d type=%d perm=%#o length=%d\n",
		d.Name, d.Qid.Path, d.Qid.Type, uint(d.Mode), d.Length)
}

// do9pReaddir handles the -D `readdir` verb: open name as a directory and print
// each entry's name in order (the listing's owner/times are not printed).
func (a *Acme) do9pReaddir(name string) {
	fid, err := a.open9p(name, plan9.OREAD)
	if err != nil {
		// untested: open fails only for bad names (not differentiable)
		a.dprintf("error %s\n", err)
		return
	}
	dirs, err := fid.Dirreadall()
	if err != nil {
		// untested: a directory read over the in-process pipe does not fail
		a.dprintf("error %s\n", err)
		return
	}
	for _, d := range dirs {
		a.dprintf("dir name=%s type=%d perm=%#o\n", d.Name, d.Qid.Type, uint(d.Mode))
	}
}

// do9pRead handles the -D `read` verb: read the file at offset 0 and print it.
func (a *Acme) do9pRead(name string) {
	fid, err := a.open9p(name, plan9.OREAD)
	if err != nil {
		// untested: see open9p (open only fails for non-differentiable names)
		a.dprintf("error %s\n", err)
		return
	}
	buf := make([]byte, 8192)
	n, err := fid.ReadAt(buf, 0)
	if err != nil && err != io.EOF {
		a.dprintf("error %s\n", err)
		return
	}
	a.dprintf("data %s\n", dquote(string(buf[:n])))
}
