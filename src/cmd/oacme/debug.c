#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
#include <bio.h>
#include <plumb.h>
#include <libsec.h>
#include <9pclient.h>
#include "dat.h"
#include "fns.h"

/*
 * Debug-conversation mode (-D). A single deterministic driver replaces the
 * mouse and keyboard threads: it reads a line-based command script from stdin,
 * mutates and prints acme's internal state under row.lk, and writes results to
 * stdout. This exercises everything but the actual pixel drawing and serves as
 * the oracle for the Go port's differential tests. See go/cmd/acme/HACKING.md.
 *
 * A window is named by its integer id; a Text within it is the body by default
 * or the tag if the id is suffixed with ".tag" (e.g. "3.tag").
 *
 * Script commands (one per line; text may use \t \n \b \\ escapes):
 *	newcol			add a column; prints its index
 *	col N			make column N the active column
 *	win			make a new window; prints its id
 *	name ID TEXT		set window ID's filename and rebuild its tag
 *	body ID TEXT		append TEXT to window ID's body
 *	tag ID TEXT		append TEXT to window ID's tag
 *	dot WHO Q0 Q1		set WHO's selection (dot) to [Q0,Q1)
 *	dclick WHO POS		double-click expand at POS; leave it selected
 *	load WHO FILE		load FILE's bytes into WHO's buffer (bufload)
 *	mtime FILE SEC		set FILE's modification time (get/put recheck tests)
 *	writefile NAME SEC TEXT	(over)write NAME with TEXT, set its mtime to SEC
 *	type WHO TEXT		type TEXT into WHO (runs the real typing path)
 *	key WHO NAME...		type special keys: bs ^w ^u del esc nl tab left
 *				right up down home end pgup pgdn
 *	b1 WHO Q0 Q1		button 1: select [Q0,Q1) and make it current
 *	b2 WHO Q0 Q1		button 2: execute [Q0,Q1) (any builtin or command)
 *	undo WHO		run the Undo builtin on WHO's window
 *	redo WHO		run the Redo builtin on WHO's window
 *	b3 WHO Q0 Q1		button 3: look/plumb [Q0,Q1)
 *	print WHO		print WHO's name, dot, and text
 *	dump			print all columns and windows
 */

static Rune*
torunes(char *s, int *np)
{
	Rune *r, *w;

	r = runemalloc(strlen(s)+1);
	w = r;
	while(*s){
		if(*s == '\\'){
			s++;
			switch(*s){
			case 't': *w++ = '\t'; s++; break;
			case 'n': *w++ = '\n'; s++; break;
			case 'b': *w++ = '\b'; s++; break;
			case '\\': *w++ = '\\'; s++; break;
			case 0: *w++ = '\\'; break;
			default: *w++ = (uchar)*s++; break;
			}
		}else
			s += chartorune(w++, s);
	}
	*np = w - r;
	return r;
}

/* return the first space-delimited token of *pp, advancing *pp past it */
static char*
firsttok(char **pp)
{
	char *s, *p;

	p = *pp;
	while(*p == ' ')
		p++;
	s = p;
	while(*p && *p != ' ')
		p++;
	if(*p)
		*p++ = 0;
	while(*p == ' ')
		p++;
	*pp = p;
	return s;
}

/* parse "ID" (body) or "ID.tag" into a window and the chosen Text */
static Text*
gettext(char *s, Window **wp)
{
	Window *w;
	char *dot;
	int id, tag;

	tag = FALSE;
	dot = strchr(s, '.');
	if(dot != nil){
		*dot = 0;
		tag = strcmp(dot+1, "tag") == 0;
	}
	id = atoi(s);
	w = lookid(id, FALSE);
	if(w == nil)
		sysfatal("no window %d", id);
	if(wp)
		*wp = w;
	return tag? &w->tag : &w->body;
}

static Rune
keysym(char *s)
{
	if(strcmp(s, "bs") == 0) return Kbs;
	if(strcmp(s, "^w") == 0) return 0x17;	/* erase word */
	if(strcmp(s, "^u") == 0) return 0x15;	/* erase line */
	if(strcmp(s, "^a") == 0) return 0x01;	/* beginning of line */
	if(strcmp(s, "^e") == 0) return 0x05;	/* end of line */
	if(strcmp(s, "^f") == 0) return 0x06;	/* filename completion */
	if(strcmp(s, "%c") == 0) return Kcmd+'c';	/* copy */
	if(strcmp(s, "%x") == 0) return Kcmd+'x';	/* cut */
	if(strcmp(s, "%v") == 0) return Kcmd+'v';	/* paste */
	if(strcmp(s, "%z") == 0) return Kcmd+'z';	/* undo */
	if(strcmp(s, "%Z") == 0) return Kcmd+'Z';	/* redo */
	if(strcmp(s, "del") == 0) return Kdel;
	if(strcmp(s, "esc") == 0) return Kesc;
	if(strcmp(s, "nl") == 0) return '\n';
	if(strcmp(s, "tab") == 0) return '\t';
	if(strcmp(s, "left") == 0) return Kleft;
	if(strcmp(s, "right") == 0) return Kright;
	if(strcmp(s, "up") == 0) return Kup;
	if(strcmp(s, "down") == 0) return Kdown;
	if(strcmp(s, "home") == 0) return Khome;
	if(strcmp(s, "end") == 0) return Kend;
	if(strcmp(s, "pgup") == 0) return Kpgup;
	if(strcmp(s, "pgdn") == 0) return Kpgdown;
	if(strcmp(s, "scrollup") == 0) return Kscrolloneup;
	if(strcmp(s, "scrolldown") == 0) return Kscrollonedown;
	sysfatal("unknown key %s", s);
	return 0;
}

static void
printtext(Text *t, char *who)
{
	Rune *r;
	int n;

	n = t->file->b.nc;
	r = runemalloc(n+1);
	bufread(&t->file->b, 0, r, n);
	if(t->what == Body)
		print("win %d name=%.*S\n", t->w->id, t->file->nname, t->file->name);
	print("  %s nc=%d dot=%d,%d\n", who, n, t->q0, t->q1);
	print("  %s text=%.*S\n", who, n, r);
	free(r);
}

static void
printwin(Window *w)
{
	printtext(&w->tag, "tag");
	printtext(&w->body, "body");
}

/* flush a typing burst like the real event loop does via wincommit: commit the
 * typed text's cache to the file, then clear any zerox copies' caches so none is
 * left with a stale uncommitted cache. */
static void
dcommittyping(Text *t)
{
	int i;
	File *f;

	textcommit(t, TRUE);
	f = t->file;
	if(f->ntext > 1)
		for(i=0; i<f->ntext; i++)
			textcommit(f->text[i], FALSE);	/* no-op for t; clears the others */
}

static void
cmdrects(void)
{
	Column *c;
	int i, j;

	print("row ncol=%d r=%d,%d,%d,%d\n", row.ncol, row.r.min.x, row.r.min.y, row.r.max.x, row.r.max.y);
	for(i=0; i<row.ncol; i++){
		c = row.col[i];
		print("col %d r=%d,%d,%d,%d\n", i, c->r.min.x, c->r.min.y, c->r.max.x, c->r.max.y);
		for(j=0; j<c->nw; j++){
			Window *w = c->w[j];
			print("  win %d r=%d,%d,%d,%d\n", w->id,
				w->r.min.x, w->r.min.y, w->r.max.x, w->r.max.y);
			print("    tag r=%d,%d,%d,%d\n", w->tag.fr.r.min.x, w->tag.fr.r.min.y,
				w->tag.fr.r.max.x, w->tag.fr.r.max.y);
			print("    body r=%d,%d,%d,%d\n", w->body.fr.r.min.x, w->body.fr.r.min.y,
				w->body.fr.r.max.x, w->body.fr.r.max.y);
			print("    scroll r=%d,%d,%d,%d\n", w->body.lastsr.min.x, w->body.lastsr.min.y,
				w->body.lastsr.max.x, w->body.lastsr.max.y);
		}
	}
}

static void
cmddump(void)
{
	Column *c;
	int i, j;

	print("row ncol=%d\n", row.ncol);
	for(i=0; i<row.ncol; i++){
		c = row.col[i];
		print("col %d nw=%d\n", i, c->nw);
		for(j=0; j<c->nw; j++)
			printwin(c->w[j]);
	}
}

/*
 * 9P read/write verbs. These drive the real xfidread/xfidwrite (xfid.c) on a
 * synthesized Xfid/Fid, capturing the reply via respond()'s debugcap hook
 * (fsys.c), so the scripted driver exercises acme's actual 9P file semantics.
 * Data is exchanged as a Go-style quoted string (see printquoted/dunquote); the
 * Go port's driver.go uses byte-identical quoting so the output can be compared.
 */

static char qhex[] = "0123456789abcdef";

/* print data[0:n] as a quoted string: \n \t \r \" \\ literal, else \xHH */
static void
printquoted(char *data, int n)
{
	int i;
	uchar c;

	print("\"");
	for(i=0; i<n; i++){
		c = data[i];
		switch(c){
		case '"':  print("\\\""); break;
		case '\\': print("\\\\"); break;
		case '\n': print("\\n"); break;
		case '\t': print("\\t"); break;
		case '\r': print("\\r"); break;
		default:
			if(c >= 0x20 && c < 0x7f)
				print("%c", c);
			else
				print("\\x%c%c", qhex[c>>4], qhex[c&0xf]);
		}
	}
	print("\"");
}

static int
hexval(int c)
{
	if(c >= '0' && c <= '9') return c-'0';
	if(c >= 'a' && c <= 'f') return c-'a'+10;
	if(c >= 'A' && c <= 'F') return c-'A'+10;
	sysfatal("bad hex digit");
	return 0;
}

/* parse a quoted string s into out (sized >= strlen(s)); return byte count */
static int
dunquote(char *s, char *out)
{
	char *o;

	if(*s != '"')
		sysfatal("write arg must be a quoted string");
	s++;
	o = out;
	while(*s && *s != '"'){
		if(*s != '\\'){
			*o++ = *s++;
			continue;
		}
		s++;
		switch(*s){
		case 'n':  *o++ = '\n'; s++; break;
		case 't':  *o++ = '\t'; s++; break;
		case 'r':  *o++ = '\r'; s++; break;
		case '"':  *o++ = '"';  s++; break;
		case '\\': *o++ = '\\'; s++; break;
		case '0':  *o++ = 0;    s++; break;
		case 'x':  s++; *o++ = hexval(s[0])*16 + hexval(s[1]); s += 2; break;
		default: sysfatal("bad escape in write arg");
		}
	}
	return o-out;
}

/*
 * do9p connects to acme's own 9P service (posted by fsysinit) as an ordinary
 * client -- exactly as the plumb library connects to the plumber -- and reads
 * or writes the named window file (e.g. "3/body"). fsysproc serves the request
 * on its own proc, so this synchronous round trip from debugthread does not
 * deadlock (debugthread holds no lock here; the xfid handlers take their own).
 */
static CFsys *acmefs;

/*
 * Open fids are cached by (name, mode) and kept open for the life of the
 * conversation. acme's addr/data/event files reset or accumulate per-window
 * state on the first open (xfidopen, nopen) and lose it when the last fid
 * closes, so a real client -- and these verbs -- must hold the fid open across
 * the address/data sequence rather than reopening per request.
 */
static struct {
	char	*name;
	int	mode;
	CFid	*fid;
} fcache[64];
static int nfcache;

static CFsys*
getfs(void)
{
	if(acmefs == nil){
		acmefs = nsmount("acme", "");
		if(acmefs == nil)
			sysfatal("nsmount acme: %r");
	}
	return acmefs;
}

static CFid*
open9p(char *name, int mode)
{
	int i;
	CFid *fid;

	getfs();
	for(i=0; i<nfcache; i++)
		if(fcache[i].mode==mode && strcmp(fcache[i].name, name)==0)
			return fcache[i].fid;
	fid = fsopen(acmefs, name, mode);
	if(fid==nil)
		return nil;
	if(nfcache < nelem(fcache)){
		fcache[nfcache].name = estrdup(name);
		fcache[nfcache].mode = mode;
		fcache[nfcache].fid = fid;
		nfcache++;
	}
	return fid;
}

static void
do9p(int iswrite, char *name, char *data, int ndata)
{
	CFid *fid;
	char buf[8192];
	int n;

	fid = open9p(name, iswrite? OWRITE : OREAD);
	if(fid == nil){
		print("error %r\n");
		return;
	}
	if(iswrite){
		n = fspwrite(fid, data, ndata, 0);
		if(n < 0)
			print("error %r\n");
		else
			print("wrote %d\n", n);
	}else{
		n = fspread(fid, buf, sizeof buf, 0);
		if(n < 0)
			print("error %r\n");
		else{
			print("data ");
			printquoted(buf, n);
			print("\n");
		}
	}
}

void
debugthread(void *v)
{
	Biobuf *b;
	char *line, *p, *args[8];
	Rune *r;
	Text *t, *at;
	Window *w, *aw;
	Column *c;
	int nr, q0, q1, n, i;

	USED(v);
	threadsetname("debugthread");

	b = Bfdopen(0, OREAD);
	if(b == nil)
		sysfatal("Bfdopen: %r");

	while((line = Brdline(b, '\n')) != nil){
		line[Blinelen(b)-1] = 0;
		if(line[0] == '#' || line[0] == 0)
			continue;
		/* split command word from the remainder */
		p = line;
		while(*p && *p != ' ')
			p++;
		if(*p)
			*p++ = 0;
		while(*p == ' ')
			p++;

		/* The 9P read/write verbs drive xfidread/xfidwrite, which take their
		 * own window lock (like the real xfid threads); they must NOT hold
		 * row.lk, so handle them before acquiring it. */
		if(strcmp(line, "write") == 0){
			char *name, *ub;
			int nb;
			name = firsttok(&p);
			ub = emalloc(strlen(p)+1);
			nb = dunquote(p, ub);
			do9p(TRUE, name, ub, nb);
			free(ub);
			/* a ctl "put"/"get" etc. may have warned; drain it like the
			 * mousethread does (flushwarnings needs the row lock) */
			qlock(&row.lk);
			flushwarnings();
			qunlock(&row.lk);
			flushimage(display, 1);
			continue;
		}
		if(strcmp(line, "read") == 0){
			do9p(FALSE, firsttok(&p), nil, 0);
			qlock(&row.lk);
			flushwarnings();
			qunlock(&row.lk);
			flushimage(display, 1);
			continue;
		}
		if(strcmp(line, "open") == 0){
			/* open and keep a fid open without reading (for the event file) */
			if(open9p(firsttok(&p), OREAD) == nil)
				print("error %r\n");
			qlock(&row.lk);
			flushwarnings();
			qunlock(&row.lk);
			flushimage(display, 1);
			continue;
		}
		if(strcmp(line, "cmderr") == 0){
			/* mount DIR, attach a fid with its id, write TEXT to that mount's
			 * /cons (routed to DIR/+Errors), then release it -- the command
			 * namespace path (fsysmount/attach/cons/clunk/delid) */
			Rune *rdir;
			int nrdir, nw;
			Mntdir *md;
			CFsys *fs;
			CFid *cfid;
			char ids[32], *dir;
			dir = firsttok(&p);
			rdir = bytetorune(dir, &nrdir);
			md = fsysmount(rdir, nrdir, nil, 0);
			snprint(ids, sizeof ids, "%d", md->id);
			fs = nsmount("acme", ids);
			if(fs == nil)
				print("error %r\n");
			else{
				cfid = fsopen(fs, "cons", OWRITE);
				if(cfid == nil)
					print("error %r\n");
				else{
					nw = fswrite(cfid, p, strlen(p));
					if(nw < 0)
						print("error %r\n");
					else
						print("wrote %d\n", nw);
					fsclose(cfid);
				}
				fsunmount(fs);
			}
			fsysdelid(md);
			flushimage(display, 1);
			continue;
		}
		if(strcmp(line, "wait") == 0){
			/* wait N: block until the +Errors window's body holds >= N runes,
			 * i.e. the async external command's output has been applied by the
			 * xfid procs. Polls with a short sleep that yields to those procs. */
			int want, cur, i, j, nn;
			Window *w, *ew;
			Rune *nm;
			static Rune errsuf[7] = {'+','E','r','r','o','r','s'};
			want = atoi(firsttok(&p));
			for(;;){
				ew = nil;
				qlock(&row.lk);
				for(i=0; i<row.ncol && ew==nil; i++)
					for(j=0; j<row.col[i]->nw; j++){
						w = row.col[i]->w[j];
						nm = w->body.file->name;
						nn = w->body.file->nname;
						if(nn>=7 && runeeq(nm+nn-7, 7, errsuf, 7)){
							ew = w;
							break;
						}
					}
				cur = ew ? ew->body.file->b.nc : 0;
				qunlock(&row.lk);
				if(cur >= want)
					break;
				yield();	/* cooperatively let runproc/xfid procs run */
			}
			flushimage(display, 1);
			continue;
		}
		if(strcmp(line, "stat") == 0){
			/* 9P stat of an acme file: print the deterministic fields only */
			Dir *d;
			d = fsdirstat(getfs(), firsttok(&p));
			if(d == nil)
				print("error %r\n");
			else{
				print("stat name=%s path=%llud type=%d perm=%#luo length=%llud\n",
					d->name, (uvlong)d->qid.path, d->qid.type, d->mode, (uvlong)d->length);
				free(d);
			}
			flushimage(display, 1);
			continue;
		}
		if(strcmp(line, "readdir") == 0){
			/* 9P read of a directory: print each entry's name in order */
			CFid *dfid;
			Dir *d;
			long i, nd;
			dfid = fsopen(getfs(), firsttok(&p), OREAD);
			if(dfid == nil)
				print("error %r\n");
			else{
				nd = fsdirreadall(dfid, &d);
				for(i=0; i<nd; i++)
					print("dir name=%s type=%d perm=%#luo\n",
						d[i].name, d[i].qid.type, d[i].mode);
				if(nd >= 0)
					free(d);
				fsclose(dfid);
			}
			flushimage(display, 1);
			continue;
		}

		qlock(&row.lk);
		if(strcmp(line, "newcol") == 0){
			c = rowadd(&row, nil, -1);
			if(c == nil)
				sysfatal("rowadd failed");
			activecol = c;
			print("col %d\n", row.ncol-1);
		}else if(strcmp(line, "col") == 0){
			i = atoi(p);
			if(i < 0 || i >= row.ncol)
				sysfatal("no column %d", i);
			activecol = row.col[i];
		}else if(strcmp(line, "win") == 0){
			w = makenewwindow(nil);
			winsettag(w);
			print("win %d\n", w->id);
		}else if(strcmp(line, "name") == 0){
			gettext(firsttok(&p), &w);
			r = torunes(p, &nr);
			winsetname(w, r, nr);
			winsettag(w);
			free(r);
		}else if(strcmp(line, "body") == 0 || strcmp(line, "tag") == 0){
			i = strtol(p, &p, 10);
			while(*p == ' ')
				p++;
			w = lookid(i, FALSE);
			if(w == nil)
				sysfatal("no window %d", i);
			t = strcmp(line, "tag")==0? &w->tag : &w->body;
			r = torunes(p, &nr);
			textinsert(t, t->file->b.nc, r, nr, TRUE);
			free(r);
		}else if(strcmp(line, "dot") == 0){
			if(tokenize(p, args, 3) != 3)
				sysfatal("dot WHO Q0 Q1");
			t = gettext(args[0], nil);
			textsetselect(t, atoi(args[1]), atoi(args[2]));
		}else if(strcmp(line, "dclick") == 0){
			/* double-click expand at POS; leave the result selected */
			if(tokenize(p, args, 2) != 2)
				sysfatal("dclick WHO POS");
			t = gettext(args[0], &w);
			q0 = q1 = atoi(args[1]);
			textdoubleclick(t, (uint*)&q0, (uint*)&q1);
			textsetselect(t, q0, q1);
		}else if(strcmp(line, "addr") == 0){
			/* evaluate the address ADDR against WHO's dot, print the range */
			Range res;
			int eval;
			uint aq;
			t = gettext(firsttok(&p), &w);
			r = torunes(p, &nr);
			eval = TRUE;
			res = address(TRUE, t, range(-1,-1), range(t->q0, t->q1), r, 0, nr, rgetc, &eval, &aq, FALSE);
			print("addr %d %d eval=%d\n", res.q0, res.q1, eval);
			free(r);
		}else if(strcmp(line, "addrrev") == 0){
			/* like addr but starting in the reverse direction */
			Range res;
			int eval;
			uint aq;
			t = gettext(firsttok(&p), &w);
			r = torunes(p, &nr);
			eval = TRUE;
			res = address(TRUE, t, range(-1,-1), range(t->q0, t->q1), r, 0, nr, rgetc, &eval, &aq, TRUE);
			print("addr %d %d eval=%d\n", res.q0, res.q1, eval);
			free(r);
		}else if(strcmp(line, "addrlim") == 0){
			/* addrlim WHO LO HI ADDR: forward address bounded by [LO,HI) */
			Range res;
			int eval, lo, hi;
			uint aq;
			t = gettext(firsttok(&p), &w);
			lo = atoi(firsttok(&p));
			hi = atoi(firsttok(&p));
			r = torunes(p, &nr);
			eval = TRUE;
			res = address(TRUE, t, range(lo,hi), range(t->q0, t->q1), r, 0, nr, rgetc, &eval, &aq, FALSE);
			print("addr %d %d eval=%d\n", res.q0, res.q1, eval);
			free(r);
		}else if(strcmp(line, "type") == 0){
			t = gettext(firsttok(&p), &w);
			r = torunes(p, &nr);
			winlock(w, 'D');
			for(i=0; i<nr; i++)
				wintype(w, t, r[i]);
			dcommittyping(t);	/* flush the typing cache (all texts) */
			winunlock(w);
			free(r);
		}else if(strcmp(line, "key") == 0){
			n = tokenize(p, args, nelem(args));
			if(n < 2)
				sysfatal("key WHO NAME...");
			t = gettext(args[0], &w);
			winlock(w, 'D');
			for(i=1; i<n; i++)
				wintype(w, t, keysym(args[i]));
			dcommittyping(t);	/* flush the typing cache (all texts) */
			winunlock(w);
		}else if(strcmp(line, "b1") == 0){
			if(tokenize(p, args, 3) != 3)
				sysfatal("b1 WHO Q0 Q1");
			t = gettext(args[0], &w);
			q0 = atoi(args[1]);
			q1 = atoi(args[2]);
			textsetselect(t, q0, q1);
			seltext = t;
			argtext = t;
			mousetext = t;
			if(t->col)
				activecol = t->col;
			if(t == &w->body)
				activewin = w;
		}else if(strcmp(line, "m1") == 0){
			/* button-1 sweep selecting [q0,q1): press at q0, drag to q1,
			 * release.  The events are fed to the real mouse channel so the
			 * unmodified mousethread dispatches them (textselect); each
			 * trailing cut/paste token chords button 2/3 while 1 is down.
			 * row.lk must be released first because mousethread takes it; the
			 * reacquire at the end blocks until mousethread is done (resync). */
			Mouse mv;
			Point p0, p1;
			int nm1, i, chord;
			nm1 = tokenize(p, args, nelem(args));
			if(nm1 < 3)
				sysfatal("m1 WHO Q0 Q1 [cut|paste...]");
			t = gettext(args[0], &w);
			q0 = atoi(args[1]);
			q1 = atoi(args[2]);
			p0 = frptofchar(&t->fr, q0 - t->org);
			p1 = frptofchar(&t->fr, q1 - t->org);
			qunlock(&row.lk);
			mv.msec = 1000;
			mv.xy = p0; mv.buttons = 1; send(mousectl->c, &mv);	/* press */
			mv.xy = p1; mv.buttons = 1; send(mousectl->c, &mv);	/* drag to q1 */
			for(i=3; i<nm1; i++){
				chord = 0;
				if(strcmp(args[i], "cut") == 0)
					chord = 2;	/* button 2: buttons 1|2 = 3 */
				else if(strcmp(args[i], "paste") == 0)
					chord = 4;	/* button 3: buttons 1|4 = 5 */
				mv.buttons = 1|chord;
				send(mousectl->c, &mv);
			}
			mv.buttons = 0;
			send(mousectl->c, &mv);	/* release */
			qlock(&row.lk);
		}else if(strcmp(line, "m1drag")==0 || strcmp(line, "m2drag")==0 || strcmp(line, "m3drag")==0){
			/* mNdrag WHO Q0 X1 Y1 [X2 Y2 ...]: press button N at char Q0, drag
			 * through raw pixel waypoints (which may be above/below the frame),
			 * release.  button 1 -> textselect (frselect auto-scrolls off the
			 * edge); button 2/3 -> textselect2/3 sweep, which xselect clamps to
			 * the frame edge (no scroll), then execute/look3 via mousedispatch. */
			Mouse mv;
			Point p0;
			int nm, i, mask;
			mask = 1 << (line[1]-'1');	/* m1drag->1, m2drag->2, m3drag->4 */
			nm = tokenize(p, args, nelem(args));
			if(nm < 4 || (nm % 2) != 0)
				sysfatal("%s WHO Q0 X Y [X Y...]", line);
			t = gettext(args[0], &w);
			p0 = frptofchar(&t->fr, atoi(args[1]) - t->org);
			qunlock(&row.lk);
			mv.msec = 1000;
			mv.xy = p0; mv.buttons = mask; send(mousectl->c, &mv);	/* press */
			for(i=2; i+1<nm; i+=2){
				mv.xy = Pt(atoi(args[i]), atoi(args[i+1]));
				mv.buttons = mask;
				send(mousectl->c, &mv);	/* drag */
			}
			mv.buttons = 0;
			send(mousectl->c, &mv);	/* release at the last waypoint */
			qlock(&row.lk);
		}else if(strcmp(line, "m2") == 0 || strcmp(line, "m3") == 0){
			/* button-2/3 sweep through the waypoints Q0 Q1 [Qi...]: press at
			 * Q0, drag through the rest, release.  mousethread dispatches:
			 * button 2 executes the swept text, button 3 looks it up. */
			Mouse mv;
			int but, nm, i;
			nm = tokenize(p, args, nelem(args));
			if(nm < 3)
				sysfatal("%s WHO Q0 Q1 [Qi...]", line);
			t = gettext(args[0], &w);
			but = strcmp(line, "m2")==0 ? 2 : 4;
			qunlock(&row.lk);
			mv.msec = 1000;
			/* press at args[1], drag through args[2..nm-1] */
			for(i=1; i<nm; i++){
				mv.xy = frptofchar(&t->fr, atoi(args[i]) - t->org);
				mv.buttons = but;
				send(mousectl->c, &mv);
			}
			mv.buttons = 0;
			send(mousectl->c, &mv);	/* release at the last waypoint */
			qlock(&row.lk);
		}else if(strcmp(line, "b2") == 0){
			if(tokenize(p, args, 3) != 3)
				sysfatal("b2 WHO Q0 Q1");
			t = gettext(args[0], &w);
			q0 = atoi(args[1]);
			q1 = atoi(args[2]);
			winlock(w, 'D');
			execute(t, q0, q1, FALSE, nil);
			winunlock(w);
		}else if(strcmp(line, "b2arg") == 0){
			/* button-2 with a 2-1 chord argument selected in another text */
			if(tokenize(p, args, 6) != 6)
				sysfatal("b2arg WHO Q0 Q1 AWHO AQ0 AQ1");
			t = gettext(args[0], &w);
			q0 = atoi(args[1]);
			q1 = atoi(args[2]);
			at = gettext(args[3], &aw);
			textsetselect(at, atoi(args[4]), atoi(args[5]));
			winlock(w, 'D');
			execute(t, q0, q1, FALSE, at);
			winunlock(w);
		}else if(strcmp(line, "which") == 0){
			/* which X Y: print the Text under the point (rowwhich) */
			Text *wt;
			if(tokenize(p, args, 2) != 2)
				sysfatal("which X Y");
			wt = rowwhich(&row, Pt(atoi(args[0]), atoi(args[1])));
			if(wt == nil)
				print("which none\n");
			else if(wt->what == Rowtag)
				print("which rowtag\n");
			else if(wt->what == Columntag)
				print("which coltag\n");
			else if(wt->what == Tag)
				print("which %d tag\n", wt->w->id);
			else
				print("which %d body\n", wt->w->id);
		}else if(strcmp(line, "dragwin") == 0 || strcmp(line, "dragcol") == 0){
			/* dragwin WHO BUT FX FY TX TY / dragcol COL BUT FX FY TX TY:
			 * press button BUT in the tag's button at (FX,FY), drag to (TX,TY),
			 * release -> coldragwin / rowdragcol, mirroring the mousethread's
			 * scrollbar-press dispatch (acme.c) */
			Mouse mv;
			int but, mask, nm, rel;
			nm = tokenize(p, args, 7);
			if(nm < 6)
				sysfatal("%s WHO BUT FX FY TX TY [CHORD]", line);
			but = atoi(args[1]);
			mask = 1 << (but-1);
			rel = mask;
			if(nm > 6)	/* optional chord button held at release (abort path) */
				rel = mask | (1 << (atoi(args[6])-1));
			qunlock(&row.lk);
			mv.msec = 1000;
			/* press in a tag's scrollbar; mousethread hit-tests it and routes
			 * Tag->coldragwin, Columntag->rowdragcol (acme.c).  WHO/COL is
			 * kept only for script readability. */
			mv.xy = Pt(atoi(args[2]), atoi(args[3]));
			mv.buttons = mask;
			send(mousectl->c, &mv);	/* press */
			mv.xy = Pt(atoi(args[4]), atoi(args[5]));
			mv.buttons = rel;
			send(mousectl->c, &mv);	/* drag (maybe chord) */
			mv.buttons = 0;
			send(mousectl->c, &mv);	/* release */
			qlock(&row.lk);
		}else if(strcmp(line, "scroll") == 0){
			/* scroll WHO BUT Y [Y...]: press button BUT in WHO's body scrollbar
			 * at vertical position Y (x = scrollbar centre, so textscroll's
			 * cursor-warp absorb is skipped), drag through any further Y, then
			 * release.  mousethread routes the press to textscroll.  but 1 =
			 * back, 2 = jump, 3 = forward; extra Y exercise the drag/auto-repeat
			 * (button 2 re-jumps to each, button 1/3 absorb them in scrsleep). */
			Mouse mv;
			int but, mask, x, nm, i;
			nm = tokenize(p, args, nelem(args));
			if(nm < 3)
				sysfatal("scroll WHO BUT Y [Y...]");
			t = gettext(args[0], &w);
			but = atoi(args[1]);
			mask = 1 << (but-1);
			x = (t->scrollr.min.x + t->scrollr.max.x) / 2;
			qunlock(&row.lk);
			mv.msec = 1000;
			for(i=2; i<nm; i++){	/* press at args[2], drag through args[3..] */
				mv.xy = Pt(x, atoi(args[i]));
				mv.buttons = mask;
				send(mousectl->c, &mv);
			}
			mv.buttons = 0;
			send(mousectl->c, &mv);	/* release at the last Y */
			qlock(&row.lk);
		}else if(strcmp(line, "wheel") == 0){
			/* wheel WHO N X Y: a scroll-wheel notch at (X,Y) in WHO -- buttons
			 * 8 = wheel up, 16 = wheel down.  mousethread maps it to a
			 * Kscrolloneup/Kscrollonedown texttype.  A trailing buttons=0 event
			 * lets the next alt iteration settle (no-op dispatch). */
			Mouse mv;
			int n;
			if(tokenize(p, args, 4) != 4)
				sysfatal("wheel WHO N X Y");
			(void)gettext(args[0], &w);
			n = atoi(args[1]);
			qunlock(&row.lk);
			mv.msec = 1000;
			mv.xy = Pt(atoi(args[2]), atoi(args[3]));
			mv.buttons = n;
			send(mousectl->c, &mv);	/* wheel notch */
			mv.buttons = 0;
			send(mousectl->c, &mv);	/* settle */
			qlock(&row.lk);
		}else if(strcmp(line, "catdump") == 0){
			/* catdump PATH: print the dump file PATH to stdout, replacing the
			 * -D temp root (the -T prefix) with "$ROOT" so the cwd/path lines
			 * are stable across runs (each run uses a fresh temp dir). */
			int fd, n, tl, oi;
			static char dbuf[1<<16], out[1<<16];
			char *s, *e;
			fd = open(p, OREAD);
			if(fd < 0)
				sysfatal("catdump: open %s: %r", p);
			n = readn(fd, dbuf, sizeof dbuf);
			close(fd);
			if(n < 0)
				sysfatal("catdump: read %s: %r", p);
			tl = trimpre ? strlen(trimpre) : 0;
			e = dbuf + n;
			oi = 0;
			for(s = dbuf; s < e; ){
				if(tl > 0 && e-s >= tl && strncmp(s, trimpre, tl) == 0){
					memmove(out+oi, "$ROOT", 5);
					oi += 5;
					s += tl;
				}else
					out[oi++] = *s++;
			}
			write(1, out, oi);	/* raw bytes: the dump may contain 0xff markers */
		}else if(strcmp(line, "dumpto") == 0){
			/* dumpto PATH: write the session dump to PATH (rowdump directly). */
			rowdump(&row, p);
		}else if(strcmp(line, "resize") == 0){
			/* resize W H: simulate a window resize. drawresizewindow asks
			 * devdraw to resize the window; under -D the test backend swaps the
			 * screen quietly (no echo on the mouse/resize channels), so the live
			 * mousethread is not triggered and we run its MResize body here:
			 * getwindow re-reads the new screen image, then redraw and reflow
			 * everything (acme.c mousethread MResize). */
			int rw, rh;
			if(tokenize(p, args, 2) != 2)
				sysfatal("resize W H");
			rw = atoi(args[0]);
			rh = atoi(args[1]);
			drawresizewindow(Rect(0, 0, rw, rh));
			if(getwindow(display, Refnone) < 0)
				sysfatal("resize: can't reattach to window");
			draw(screen, screen->r, display->white, nil, ZP);
			iconinit();
			scrlresize();
			rowresize(&row, screen->clipr);
		}else if(strcmp(line, "load") == 0){
			/* load FILE's bytes into WHO's buffer (bufload/cvttorunes) */
			int fd, nulls;
			t = gettext(firsttok(&p), &w);
			fd = open(p, OREAD);
			if(fd < 0)
				sysfatal("load: can't open %s", p);
			nulls = FALSE;
			n = bufload(&t->file->b, t->file->b.nc, fd, &nulls, nil);
			close(fd);
			print("load n=%d nulls=%d\n", n, nulls);
		}else if(strcmp(line, "mtime") == 0){
			/* mtime FILE SEC: set FILE's modification time, so get/put
			 * mtime-recheck tests can control it deterministically. */
			Dir nd;
			if(tokenize(p, args, 2) != 2)
				sysfatal("mtime FILE SEC");
			nulldir(&nd);
			nd.mtime = atoi(args[1]);
			if(dirwstat(args[0], &nd) < 0)
				sysfatal("mtime: %r");
		}else if(strcmp(line, "writefile") == 0){
			/* writefile NAME SEC TEXT: (over)write NAME with the raw remainder
			 * TEXT and set its mtime to SEC. */
			char *name, *secs;
			int fd, len;
			Dir nd;
			name = firsttok(&p);
			secs = firsttok(&p);
			fd = create(name, OWRITE, 0644);
			if(fd < 0)
				sysfatal("writefile: %r");
			len = strlen(p);
			if(write(fd, p, len) != len)
				sysfatal("writefile: write %s", name);
			close(fd);
			nulldir(&nd);
			nd.mtime = atoi(secs);
			if(dirwstat(name, &nd) < 0)
				sysfatal("writefile: %r");
		}else if(strcmp(line, "undo") == 0 || strcmp(line, "redo") == 0){
			/* run the Undo/Redo builtin on WHO's window */
			t = gettext(firsttok(&p), &w);
			winlock(w, 'D');
			undo(t, nil, nil, strcmp(line, "undo")==0, FALSE, nil, 0);
			winunlock(w);
		}else if(strcmp(line, "b3") == 0 || strcmp(line, "b3r") == 0){
			if(tokenize(p, args, 3) != 3)
				sysfatal("b3 WHO Q0 Q1");
			t = gettext(args[0], &w);
			q0 = atoi(args[1]);
			q1 = atoi(args[2]);
			winlock(w, 'D');
			look3(t, q0, q1, FALSE, strcmp(line, "b3r")==0);
			winunlock(w);
		}else if(strcmp(line, "print") == 0){
			t = gettext(p, &w);
			if(t->what == Body)
				printwin(w);
			else
				printtext(t, "tag");
		}else if(strcmp(line, "colgrow") == 0){
			if(tokenize(p, args, 2) != 2)
				sysfatal("colgrow WINID BUT");
			w = lookid(atoi(args[0]), FALSE);
			if(w == nil)
				sysfatal("no window %d", atoi(args[0]));
			colgrow(w->col, w, atoi(args[1]));
		}else if(strcmp(line, "colclose") == 0){
			w = lookid(atoi(p), FALSE);
			if(w == nil)
				sysfatal("no window %d", atoi(p));
			colclose(w->col, w, TRUE);
		}else if(strcmp(line, "coladd") == 0){
			if(tokenize(p, args, 2) != 2)
				sysfatal("coladd COLIDX Y");
			i = atoi(args[0]);
			if(i < 0 || i >= row.ncol)
				sysfatal("no column %d", i);
			coladd(row.col[i], nil, nil, atoi(args[1]));
		}else if(strcmp(line, "colresize") == 0){
			if(tokenize(p, args, 5) != 5)
				sysfatal("colresize COLIDX X0 Y0 X1 Y1");
			i = atoi(args[0]);
			if(i < 0 || i >= row.ncol)
				sysfatal("no column %d", i);
			colresize(row.col[i], Rect(atoi(args[1]), atoi(args[2]), atoi(args[3]), atoi(args[4])));
		}else if(strcmp(line, "rowresize") == 0){
			if(tokenize(p, args, 4) != 4)
				sysfatal("rowresize X0 Y0 X1 Y1");
			rowresize(&row, Rect(atoi(args[0]), atoi(args[1]), atoi(args[2]), atoi(args[3])));
		}else if(strcmp(line, "rowadd") == 0){
			rowadd(&row, nil, atoi(p));
		}else if(strcmp(line, "rects") == 0){
			cmdrects();
		}else if(strcmp(line, "dump") == 0){
			cmddump();
		}else
			sysfatal("unknown command: %s", line);
		flushwarnings();
		qunlock(&row.lk);
		flushimage(display, 1);
	}
	threadexitsall(nil);
}
