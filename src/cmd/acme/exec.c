#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
#include <plumb.h>
#include <libsec.h>
#include <9pclient.h>
#include "dat.h"
#include "fns.h"

Buffer	snarfbuf;

/*
 * These functions get called as:
 *
 *	fn(et, t, argt, flag1, flag1, flag2, s, n);
 *
 * Where the arguments are:
 *
 *	et: the Text* in which the executing event (click) occurred
 *	t: the Text* containing the current selection (Edit, Cut, Snarf, Paste)
 *	argt: the Text* containing the argument for a 2-1 click.
 *	e->flag1: from Exectab entry
 * 	e->flag2: from Exectab entry
 *	s: the command line remainder (e.g., "x" if executing "Dump x")
 *	n: length of s  (s is *not* NUL-terminated)
 */

void doabort(Text*, Text*, Text*, int, int, Rune*, int);
void	del(Text*, Text*, Text*, int, int, Rune*, int);
void	delcol(Text*, Text*, Text*, int, int, Rune*, int);
void	dotfiles(Text*, Text*, Text*, int, int, Rune*, int);
void	dump(Text*, Text*, Text*, int, int, Rune*, int);
void	edit(Text*, Text*, Text*, int, int, Rune*, int);
void	xexit(Text*, Text*, Text*, int, int, Rune*, int);
void	fontx(Text*, Text*, Text*, int, int, Rune*, int);
void	get(Text*, Text*, Text*, int, int, Rune*, int);
void	id(Text*, Text*, Text*, int, int, Rune*, int);
void	incl(Text*, Text*, Text*, int, int, Rune*, int);
void	indent(Text*, Text*, Text*, int, int, Rune*, int);
void	xkill(Text*, Text*, Text*, int, int, Rune*, int);
void	local(Text*, Text*, Text*, int, int, Rune*, int);
void	look(Text*, Text*, Text*, int, int, Rune*, int);
void	newcol(Text*, Text*, Text*, int, int, Rune*, int);
void	paste(Text*, Text*, Text*, int, int, Rune*, int);
void	put(Text*, Text*, Text*, int, int, Rune*, int);
void	putall(Text*, Text*, Text*, int, int, Rune*, int);
void	sendx(Text*, Text*, Text*, int, int, Rune*, int);
void	sort(Text*, Text*, Text*, int, int, Rune*, int);
void	tab(Text*, Text*, Text*, int, int, Rune*, int);
void	zeroxx(Text*, Text*, Text*, int, int, Rune*, int);

typedef struct Exectab Exectab;
struct Exectab
{
	Rune	*name;
	void	(*fn)(Text*, Text*, Text*, int, int, Rune*, int);
	int		mark;
	int		flag1;
	int		flag2;
};

static Rune LAbort[] = { 'A', 'b', 'o', 'r', 't', 0 };
static Rune LCut[] = { 'C', 'u', 't', 0 };
static Rune LDel[] = { 'D', 'e', 'l', 0 };
static Rune LDelcol[] = { 'D', 'e', 'l', 'c', 'o', 'l', 0 };
static Rune LDelete[] = { 'D', 'e', 'l', 'e', 't', 'e', 0 };
static Rune LDump[] = { 'D', 'u', 'm', 'p', 0 };
static Rune LEdit[] = { 'E', 'd', 'i', 't', 0 };
static Rune LExit[] = { 'E', 'x', 'i', 't', 0 };
static Rune LFont[] = { 'F', 'o', 'n', 't', 0 };
static Rune LGet[] = { 'G', 'e', 't', 0 };
static Rune LID[] = { 'I', 'D', 0 };
static Rune LIncl[] = { 'I', 'n', 'c', 'l', 0 };
static Rune LIndent[] = { 'I', 'n', 'd', 'e', 'n', 't', 0 };
static Rune LKill[] = { 'K', 'i', 'l', 'l', 0 };
static Rune LLoad[] = { 'L', 'o', 'a', 'd', 0 };
static Rune LLocal[] = { 'L', 'o', 'c', 'a', 'l', 0 };
static Rune LLook[] = { 'L', 'o', 'o', 'k', 0 };
static Rune LNew[] = { 'N', 'e', 'w', 0 };
static Rune LNewcol[] = { 'N', 'e', 'w', 'c', 'o', 'l', 0 };
static Rune LPaste[] = { 'P', 'a', 's', 't', 'e', 0 };
static Rune LPut[] = { 'P', 'u', 't', 0 };
static Rune LPutall[] = { 'P', 'u', 't', 'a', 'l', 'l', 0 };
static Rune LRedo[] = { 'R', 'e', 'd', 'o', 0 };
static Rune LSend[] = { 'S', 'e', 'n', 'd', 0 };
static Rune LSnarf[] = { 'S', 'n', 'a', 'r', 'f', 0 };
static Rune LSort[] = { 'S', 'o', 'r', 't', 0 };
static Rune LTab[] = { 'T', 'a', 'b', 0 };
static Rune LUndo[] = { 'U', 'n', 'd', 'o', 0 };
static Rune LZerox[] = { 'Z', 'e', 'r', 'o', 'x', 0 };

Exectab exectab[] = {
	{ LAbort,		doabort,	FALSE,	XXX,		XXX,		},
	{ LCut,		cut,		TRUE,	TRUE,	TRUE	},
	{ LDel,		del,		FALSE,	FALSE,	XXX		},
	{ LDelcol,		delcol,	FALSE,	XXX,		XXX		},
	{ LDelete,		del,		FALSE,	TRUE,	XXX		},
	{ LDump,		dump,	FALSE,	TRUE,	XXX		},
	{ LEdit,		edit,		FALSE,	XXX,		XXX		},
	{ LExit,		xexit,	FALSE,	XXX,		XXX		},
	{ LFont,		fontx,	FALSE,	XXX,		XXX		},
	{ LGet,		get,		FALSE,	TRUE,	XXX		},
	{ LID,		id,		FALSE,	XXX,		XXX		},
	{ LIncl,		incl,		FALSE,	XXX,		XXX		},
	{ LIndent,		indent,	FALSE,	XXX,		XXX		},
	{ LKill,		xkill,		FALSE,	XXX,		XXX		},
	{ LLoad,		dump,	FALSE,	FALSE,	XXX		},
	{ LLocal,		local,	FALSE,	XXX,		XXX		},
	{ LLook,		look,		FALSE,	XXX,		XXX		},
	{ LNew,		new,		FALSE,	XXX,		XXX		},
	{ LNewcol,	newcol,	FALSE,	XXX,		XXX		},
	{ LPaste,		paste,	TRUE,	TRUE,	XXX		},
	{ LPut,		put,		FALSE,	XXX,		XXX		},
	{ LPutall,		putall,	FALSE,	XXX,		XXX		},
	{ LRedo,		undo,	FALSE,	FALSE,	XXX		},
	{ LSend,		sendx,	TRUE,	XXX,		XXX		},
	{ LSnarf,		cut,		FALSE,	TRUE,	FALSE	},
	{ LSort,		sort,		FALSE,	XXX,		XXX		},
	{ LTab,		tab,		FALSE,	XXX,		XXX		},
	{ LUndo,		undo,	FALSE,	TRUE,	XXX		},
	{ LZerox,		zeroxx,	FALSE,	XXX,		XXX		},
	{ nil, 			0,		0,		0,		0		}
};

Exectab*
lookup(Rune *r, int n)
{
	Exectab *e;
	int nr;

	r = skipbl(r, n, &n);
	if(n == 0)
		return nil;
	findbl(r, n, &nr);
	nr = n-nr;
	for(e=exectab; e->name; e++)
		if(runeeq(r, nr, e->name, runestrlen(e->name)) == TRUE)
			return e;
	return nil;
}

int
isexecc(int c)
{
	if(isfilec(c))
		return 1;
	return c=='<' || c=='|' || c=='>';
}

void
execute(Text *t, uint aq0, uint aq1, int external, Text *argt)
{
	uint q0, q1;
	Rune *r, *s;
	char *b, *a, *aa;
	Exectab *e;
	int c, n, f;
	Runestr dir;

	q0 = aq0;
	q1 = aq1;
	if(q1 == q0){	/* expand to find word (actually file name) */
		/* if in selection, choose selection */
		if(t->q1>t->q0 && t->q0<=q0 && q0<=t->q1){
			q0 = t->q0;
			q1 = t->q1;
		}else{
			while(q1<t->file->b.nc && isexecc(c=textreadc(t, q1)) && c!=':')
				q1++;
			while(q0>0 && isexecc(c=textreadc(t, q0-1)) && c!=':')
				q0--;
			if(q1 == q0)
				return;
		}
	}
	r = runemalloc(q1-q0);
	bufread(&t->file->b, q0, r, q1-q0);
	e = lookup(r, q1-q0);
	if(!external && t->w!=nil && t->w->nopen[QWevent]>0){
		f = 0;
		if(e)
			f |= 1;
		if(q0!=aq0 || q1!=aq1){
			bufread(&t->file->b, aq0, r, aq1-aq0);
			f |= 2;
		}
		aa = getbytearg(argt, TRUE, TRUE, &a);
		if(a){
			if(strlen(a) > EVENTSIZE){	/* too big; too bad */
				free(r);
				free(aa);
				free(a);
				warning(nil, "argument string too long\n");
				return;
			}
			f |= 8;
		}
		c = 'x';
		if(t->what == Body)
			c = 'X';
		n = aq1-aq0;
		if(n <= EVENTSIZE)
			winevent(t->w, "%c%d %d %d %d %.*S\n", c, aq0, aq1, f, n, n, r);
		else
			winevent(t->w, "%c%d %d %d 0 \n", c, aq0, aq1, f);
		if(q0!=aq0 || q1!=aq1){
			n = q1-q0;
			bufread(&t->file->b, q0, r, n);
			if(n <= EVENTSIZE)
				winevent(t->w, "%c%d %d 0 %d %.*S\n", c, q0, q1, n, n, r);
			else
				winevent(t->w, "%c%d %d 0 0 \n", c, q0, q1);
		}
		if(a){
			winevent(t->w, "%c0 0 0 %d %s\n", c, utflen(a), a);
			if(aa)
				winevent(t->w, "%c0 0 0 %d %s\n", c, utflen(aa), aa);
			else
				winevent(t->w, "%c0 0 0 0 \n", c);
		}
		free(r);
		free(aa);
		free(a);
		return;
	}
	if(e){
		if(e->mark && seltext!=nil)
		if(seltext->what == Body){
			seq++;
			filemark(seltext->w->body.file);
		}
		s = skipbl(r, q1-q0, &n);
		s = findbl(s, n, &n);
		s = skipbl(s, n, &n);
		(*e->fn)(t, seltext, argt, e->flag1, e->flag2, s, n);
		free(r);
		return;
	}

	b = runetobyte(r, q1-q0);
	free(r);
	dir = dirname(t, nil, 0);
	if(dir.nr==1 && dir.r[0]=='.'){	/* sigh */
		free(dir.r);
		dir.r = nil;
		dir.nr = 0;
	}
	aa = getbytearg(argt, TRUE, TRUE, &a);
	if(t->w)
		incref(&t->w->ref);
	run(t->w, b, dir.r, dir.nr, TRUE, aa, a, FALSE);
}

char*
printarg(Text *argt, uint q0, uint q1)
{
	char *buf;

	if(argt->what!=Body || argt->file->name==nil)
		return nil;
	buf = emalloc(argt->file->nname+32);
	if(q0 == q1)
		sprint(buf, "%.*S:#%d", argt->file->nname, argt->file->name, q0);
	else
		sprint(buf, "%.*S:#%d,#%d", argt->file->nname, argt->file->name, q0, q1);
	return buf;
}

char*
getarg(Text *argt, int doaddr, int dofile, Rune **rp, int *nrp)
{
	int n;
	Expand e;
	char *a;

	memset(&e, 0, sizeof e);
	*rp = nil;
	*nrp = 0;
	if(argt == nil)
		return nil;
	a = nil;
	textcommit(argt, TRUE);
	if(expand(argt, argt->q0, argt->q1, &e, FALSE)){
		free(e.bname);
		if(e.nname && dofile){
			e.name = runerealloc(e.name, e.nname+1);
			if(doaddr)
				a = printarg(argt, e.q0, e.q1);
			*rp = e.name;
			*nrp = e.nname;
			return a;
		}
		free(e.name);
	}else{
		e.q0 = argt->q0;
		e.q1 = argt->q1;
	}
	n = e.q1 - e.q0;
	*rp = runemalloc(n+1);
	bufread(&argt->file->b, e.q0, *rp, n);
	if(doaddr)
		a = printarg(argt, e.q0, e.q1);
	*nrp = n;
	return a;
}

char*
getbytearg(Text *argt, int doaddr, int dofile, char **bp)
{
	Rune *r;
	int n;
	char *aa;

	*bp = nil;
	aa = getarg(argt, doaddr, dofile, &r, &n);
	if(r == nil)
		return nil;
	*bp = runetobyte(r, n);
	free(r);
	return aa;
}

void
doabort(Text *__0, Text *_0, Text *_1, int _2, int _3, Rune *_4, int _5)
{
	static int n;

	USED(__0);
	USED(_0);
	USED(_1);
	USED(_2);
	USED(_3);
	USED(_4);
	USED(_5);

	if(n++ == 0)
		warning(nil, "executing Abort again will call abort()\n");
	else
		abort();
}

void
newcol(Text *et, Text *_0, Text *_1, int _2, int _3, Rune *_4, int _5)
{
	Column *c;
	Window *w;

	USED(_0);
	USED(_1);
	USED(_2);
	USED(_3);
	USED(_4);
	USED(_5);

	c = rowadd(et->row, nil, -1);
	if(c) {
		w = coladd(c, nil, nil, -1);
		winsettag(w);
		xfidlog(w, "new");
	}
}

void
delcol(Text *et, Text *_0, Text *_1, int _2, int _3, Rune *_4, int _5)
{
	int i;
	Column *c;
	Window *w;

	USED(_0);
	USED(_1);
	USED(_2);
	USED(_3);
	USED(_4);
	USED(_5);

	c = et->col;
	if(c==nil || colclean(c)==0)
		return;
	for(i=0; i<c->nw; i++){
		w = c->w[i];
		if(w->nopen[QWevent]+w->nopen[QWaddr]+w->nopen[QWdata]+w->nopen[QWxdata] > 0){
			warning(nil, "can't delete column; %.*S is running an external command\n", w->body.file->nname, w->body.file->name);
			return;
		}
	}
	rowclose(et->col->row, et->col, TRUE);
}

void
del(Text *et, Text *_0, Text *_1, int flag1, int _2, Rune *_3, int _4)
{
	USED(_0);
	USED(_1);
	USED(_2);
	USED(_3);
	USED(_4);

	if(et->col==nil || et->w == nil)
		return;
	if(flag1 || et->w->body.file->ntext>1 || winclean(et->w, FALSE))
		colclose(et->col, et->w, TRUE);
}

void
sort(Text *et, Text *_0, Text *_1, int _2, int _3, Rune *_4, int _5)
{
	USED(_0);
	USED(_1);
	USED(_2);
	USED(_3);
	USED(_4);
	USED(_5);

	if(et->col)
		colsort(et->col);
}

uint
seqof(Window *w, int isundo)
{
	/* if it's undo, see who changed with us */
	if(isundo)
		return w->body.file->seq;
	/* if it's redo, see who we'll be sync'ed up with */
	return fileredoseq(w->body.file);
}

void
undo(Text *et, Text *_0, Text *_1, int flag1, int _2, Rune *_3, int _4)
{
	int i, j;
	Column *c;
	Window *w;
	uint seq;

	USED(_0);
	USED(_1);
	USED(_2);
	USED(_3);
	USED(_4);

	if(et==nil || et->w== nil)
		return;
	seq = seqof(et->w, flag1);
	if(seq == 0){
		/* nothing to undo */
		return;
	}
	/*
	 * Undo the executing window first. Its display will update. other windows
	 * in the same file will not call show() and jump to a different location in the file.
	 * Simultaneous changes to other files will be chaotic, however.
	 */
	winundo(et->w, flag1);
	for(i=0; i<row.ncol; i++){
		c = row.col[i];
		for(j=0; j<c->nw; j++){
			w = c->w[j];
			if(w == et->w)
				continue;
			if(seqof(w, flag1) == seq)
				winundo(w, flag1);
		}
	}
}

char*
getname(Text *t, Text *argt, Rune *arg, int narg, int isput)
{
	char *s;
	Rune *r;
	int i, n, promote;
	Runestr dir;

	getarg(argt, FALSE, TRUE, &r, &n);
	promote = FALSE;
	if(r == nil)
		promote = TRUE;
	else if(isput){
		/* if are doing a Put, want to synthesize name even for non-existent file */
		/* best guess is that file name doesn't contain a slash */
		promote = TRUE;
		for(i=0; i<n; i++)
			if(r[i] == '/'){
				promote = FALSE;
				break;
			}
		if(promote){
			t = argt;
			arg = r;
			narg = n;
		}
	}
	if(promote){
		n = narg;
		if(n <= 0){
			s = runetobyte(t->file->name, t->file->nname);
			return s;
		}
		/* prefix with directory name if necessary */
		dir.r = nil;
		dir.nr = 0;
		if(n>0 && arg[0]!='/'){
			dir = dirname(t, nil, 0);
			if(dir.nr==1 && dir.r[0]=='.'){	/* sigh */
				free(dir.r);
				dir.r = nil;
				dir.nr = 0;
			}
		}
		if(dir.r){
			r = runemalloc(dir.nr+n+1);
			runemove(r, dir.r, dir.nr);
			free(dir.r);
			if(dir.nr>0 && r[dir.nr]!='/' && n>0 && arg[0]!='/')
				r[dir.nr++] = '/';
			runemove(r+dir.nr, arg, n);
			n += dir.nr;
		}else{
			r = runemalloc(n+1);
			runemove(r, arg, n);
		}
	}
	s = runetobyte(r, n);
	free(r);
	if(strlen(s) == 0){
		free(s);
		s = nil;
	}
	return s;
}

void
zeroxx(Text *et, Text *t, Text *_1, int _2, int _3, Rune *_4, int _5)
{
	Window *nw;
	int c, locked;

	USED(_1);
	USED(_2);
	USED(_3);
	USED(_4);
	USED(_5);

	locked = FALSE;
	if(t!=nil && t->w!=nil && t->w!=et->w){
		locked = TRUE;
		c = 'M';
		if(et->w)
			c = et->w->owner;
		winlock(t->w, c);
	}
	if(t == nil)
		t = et;
	if(t==nil || t->w==nil)
		return;
	t = &t->w->body;
	if(t->w->isdir)
		warning(nil, "%.*S is a directory; Zerox illegal\n", t->file->nname, t->file->name);
	else{
		nw = coladd(t->w->col, nil, t->w, -1);
		/* ugly: fix locks so w->unlock works */
		winlock1(nw, t->w->owner);
		xfidlog(nw, "zerox");
	}
	if(locked)
		winunlock(t->w);
}

typedef struct TextAddr TextAddr;
struct TextAddr {
	long lorigin; // line+rune for origin
	long rorigin;
	long lq0; // line+rune for q0
	long rq0;
	long lq1; // line+rune for q1
	long rq1;
};

void
get(Text *et, Text *t, Text *argt, int flag1, int _0, Rune *arg, int narg)
{
	char *name;
	Rune *r;
	int i, n, dirty, samename, isdir;
	TextAddr *addr, *a;
	Window *w;
	Text *u;
	Dir *d;
	long q0, q1;

	USED(_0);

	if(flag1)
		if(et==nil || et->w==nil)
			return;
	if(!et->w->isdir && (et->w->body.file->b.nc>0 && !winclean(et->w, TRUE)))
		return;
	w = et->w;
	t = &w->body;
	name = getname(t, argt, arg, narg, FALSE);
	if(name == nil){
		warning(nil, "no file name\n");
		return;
	}
	if(t->file->ntext>1){
		d = dirstat(name);
		isdir = (d!=nil && (d->qid.type & QTDIR));
		free(d);
		if(isdir){
			warning(nil, "%s is a directory; can't read with multiple windows on it\n", name);
			return;
		}
	}
	addr = emalloc((t->file->ntext)*sizeof(TextAddr));
	for(i=0; i<t->file->ntext; i++) {
		a = &addr[i];
		u = t->file->text[i];
		a->lorigin = nlcount(u, 0, u->org, &a->rorigin);
		a->lq0 = nlcount(u, 0, u->q0, &a->rq0);
		a->lq1 = nlcount(u, u->q0, u->q1, &a->rq1);
	}
	r = bytetorune(name, &n);
	for(i=0; i<t->file->ntext; i++){
		u = t->file->text[i];
		/* second and subsequent calls with zero an already empty buffer, but OK */
		textreset(u);
		windirfree(u->w);
	}
	samename = runeeq(r, n, t->file->name, t->file->nname);
	textload(t, 0, name, samename);
	if(samename){
		t->file->mod = FALSE;
		dirty = FALSE;
	}else{
		t->file->mod = TRUE;
		dirty = TRUE;
	}
	for(i=0; i<t->file->ntext; i++)
		t->file->text[i]->w->dirty = dirty;
	free(name);
	free(r);
	winsettag(w);
	t->file->unread = FALSE;
	for(i=0; i<t->file->ntext; i++){
		u = t->file->text[i];
		textsetselect(&u->w->tag, u->w->tag.file->b.nc, u->w->tag.file->b.nc);
		if(samename) {
			a = &addr[i];
			// warning(nil, "%d %d %d %d %d %d\n", a->lorigin, a->rorigin, a->lq0, a->rq0, a->lq1, a->rq1);
			q0 = nlcounttopos(u, 0, a->lq0, a->rq0);
			q1 = nlcounttopos(u, q0, a->lq1, a->rq1);
			textsetselect(u, q0, q1);
			q0 = nlcounttopos(u, 0, a->lorigin, a->rorigin);
			textsetorigin(u, q0, FALSE);
		}
		textscrdraw(u);
	}
	free(addr);
	xfidlog(w, "get");
}

static void
checksha1(char *name, File *f, Dir *d)
{
	int fd, n;
	DigestState *h;
	uchar out[20];
	uchar *buf;

	fd = open(name, OREAD);
	if(fd < 0)
		return;
	h = sha1(nil, 0, nil, nil);
	buf = emalloc(8192);
	while((n = read(fd, buf, 8192)) > 0)
		sha1(buf, n, nil, h);
	free(buf);
	close(fd);
	sha1(nil, 0, out, h);
	if(memcmp(out, f->sha1, sizeof out) == 0) {
		f->dev = d->dev;
		f->qidpath = d->qid.path;
		f->mtime = d->mtime;
	}
}

void
putfile(File *f, int q0, int q1, Rune *namer, int nname)
{
	uint n, m;
	Rune *r;
	Biobuf *b;
	char *s, *name;
	int i, fd, q, ret, retc;
	Dir *d, *d1;
	Window *w;
	int isapp;
	DigestState *h;

	w = f->curtext->w;
	name = runetobyte(namer, nname);
	d = dirstat(name);
	if(d!=nil && runeeq(namer, nname, f->name, f->nname)){
		if(f->dev!=d->dev || f->qidpath!=d->qid.path || f->mtime != d->mtime)
			checksha1(name, f, d);
		if(f->dev!=d->dev || f->qidpath!=d->qid.path || f->mtime != d->mtime) {
			if(f->unread)
				warning(nil, "%s not written; file already exists\n", name);
			else
				warning(nil, "%s modified%s%s since last read\n\twas %t; now %t\n", name, d->muid[0]?" by ":"", d->muid, f->mtime, d->mtime);
			f->dev = d->dev;
			f->qidpath = d->qid.path;
			f->mtime = d->mtime;
			goto Rescue1;
		}
	}

	fd = create(name, OWRITE, 0666);
	if(fd < 0){
		warning(nil, "can't create file %s: %r\n", name);
		goto Rescue1;
	}
	// Use bio in order to force the writes to be large and
	// block-aligned (bio's default is 8K). This is not strictly
	// necessary; it works around some buggy underlying
	// file systems that mishandle unaligned writes.
	// https://codereview.appspot.com/89550043/
	b = emalloc(sizeof *b);
	Binit(b, fd, OWRITE);
	r = fbufalloc();
	s = fbufalloc();
	free(d);
	d = dirfstat(fd);
	h = sha1(nil, 0, nil, nil);
	isapp = (d!=nil && d->length>0 && (d->qid.type&QTAPPEND));
	if(isapp){
		warning(nil, "%s not written; file is append only\n", name);
		goto Rescue2;
	}

	for(q=q0; q<q1; q+=n){
		n = q1 - q;
		if(n > BUFSIZE/UTFmax)
			n = BUFSIZE/UTFmax;
		bufread(&f->b, q, r, n);
		m = snprint(s, BUFSIZE+1, "%.*S", n, r);
		sha1((uchar*)s, m, nil, h);
		if(Bwrite(b, s, m) != m){
			warning(nil, "can't write file %s: %r\n", name);
			goto Rescue2;
		}
	}
	if(Bflush(b) < 0) {
		warning(nil, "can't write file %s: %r\n", name);
		goto Rescue2;
	}
	ret = Bterm(b);
	retc = close(fd);
	free(b);
	b = nil;
	if(ret < 0 || retc < 0) {
		warning(nil, "can't write file %s: %r\n", name);
		goto Rescue2; // flush or close failed
	}
	if(runeeq(namer, nname, f->name, f->nname)){
		if(q0!=0 || q1!=f->b.nc){
			f->mod = TRUE;
			w->dirty = TRUE;
			f->unread = TRUE;
		}else{
			// In case the file is on NFS, reopen the fd
			// before dirfstat to cause the attribute cache
			// to be updated (otherwise the mtime in the
			// dirfstat below will be stale and not match
			// what NFS sees).  The file is already written,
			// so this should be a no-op when not on NFS.
			// Opening for OWRITE (but no truncation)
			// in case we don't have read permission.
			// (The create above worked, so we probably
			// still have write permission.)
			fd = open(name, OWRITE);
			d1 = dirfstat(fd);
			close(fd);
			if(d1 != nil){
				free(d);
				d = d1;
			}
			f->qidpath = d->qid.path;
			f->dev = d->dev;
			f->mtime = d->mtime;
			sha1(nil, 0, f->sha1, h);
			h = nil;
			f->mod = FALSE;
			w->dirty = FALSE;
			f->unread = FALSE;
		}
		for(i=0; i<f->ntext; i++){
			f->text[i]->w->putseq = f->seq;
			f->text[i]->w->dirty = w->dirty;
		}
	}
	fbuffree(s);
	fbuffree(r);
	free(h);
	free(d);
	free(namer);
	free(name);
	close(fd);
	winsettag(w);
	return;

    Rescue2:
	if(b != nil) {
		Bterm(b);
		free(b);
		close(fd);
	}
	free(h);
	fbuffree(s);
	fbuffree(r);
	/* fall through */

    Rescue1:
	free(d);
	free(namer);
	free(name);
}

static void
trimspaces(Text *et)
{
	File *f;
	Rune *r;
	Text *t;
	uint q0, n, delstart;
	int c, i, marked;

	t = &et->w->body;
	f = t->file;
	marked = 0;

	if(t->w!=nil && et->w!=t->w){
		/* can this happen when t == &et->w->body? */
		c = 'M';
		if(et->w)
			c = et->w->owner;
		winlock(t->w, c);
	}

	r = fbufalloc();
	q0 = f->b.nc;
	delstart = q0; /* end of current space run, or 0 if no active run; = q0 to delete spaces before EOF */
	while(q0 > 0) {
		n = RBUFSIZE;
		if(n > q0)
			n = q0;
		q0 -= n;
		bufread(&f->b, q0, r, n);
		for(i=n; ; i--) {
			if(i == 0 || (r[i-1] != ' ' && r[i-1] != '\t')) {
				// Found non-space or start of buffer. Delete active space run.
				if(q0+i < delstart) {
					if(!marked) {
						marked = 1;
						seq++;
						filemark(f);
					}
					textdelete(t, q0+i, delstart, TRUE);
				}
				if(i == 0) {
					/* keep run active into tail of next buffer */
					if(delstart > 0)
						delstart = q0;
					break;
				}
				delstart = 0;
				if(r[i-1] == '\n')
					delstart = q0+i-1; /* delete spaces before this newline */
			}
		}
	}
	fbuffree(r);

	if(t->w!=nil && et->w!=t->w)
		winunlock(t->w);
}

void
put(Text *et, Text *_0, Text *argt, int _1, int _2, Rune *arg, int narg)
{
	int nname;
	Rune  *namer;
	Window *w;
	File *f;
	char *name;

	USED(_0);
	USED(_1);
	USED(_2);

	if(et==nil || et->w==nil || et->w->isdir)
		return;
	w = et->w;
	f = w->body.file;
	name = getname(&w->body, argt, arg, narg, TRUE);
	if(name == nil){
		warning(nil, "no file name\n");
		return;
	}
	if(w->autoindent)
		trimspaces(et);
	namer = bytetorune(name, &nname);
	putfile(f, 0, f->b.nc, namer, nname);
	xfidlog(w, "put");
	free(name);
}

void
dump(Text *_0, Text *_1, Text *argt, int isdump, int _2, Rune *arg, int narg)
{
	char *name;

	USED(_0);
	USED(_1);
	USED(_2);

	if(narg)
		name = runetobyte(arg, narg);
	else
		getbytearg(argt, FALSE, TRUE, &name);
	if(isdump)
		rowdump(&row, name);
	else
		rowload(&row, name, FALSE);
	free(name);
}

void
cut(Text *et, Text *t, Text *_0, int dosnarf, int docut, Rune *_2, int _3)
{
	uint q0, q1, n, locked, c;
	Rune *r;

	USED(_0);
	USED(_2);
	USED(_3);

	/*
	 * if not executing a mouse chord (et != t) and snarfing (dosnarf)
	 * and executed Cut or Snarf in window tag (et->w != nil),
	 * then use the window body selection or the tag selection
	 * or do nothing at all.
	 */
	if(et!=t && dosnarf && et->w!=nil){
		if(et->w->body.q1>et->w->body.q0){
			t = &et->w->body;
			if(docut)
				filemark(t->file);	/* seq has been incremented by execute */
		}else if(et->w->tag.q1>et->w->tag.q0)
			t = &et->w->tag;
		else
			t = nil;
	}
	if(t == nil)	/* no selection */
		return;

	locked = FALSE;
	if(t->w!=nil && et->w!=t->w){
		locked = TRUE;
		c = 'M';
		if(et->w)
			c = et->w->owner;
		winlock(t->w, c);
	}
	if(t->q0 == t->q1){
		if(locked)
			winunlock(t->w);
		return;
	}
	if(dosnarf){
		q0 = t->q0;
		q1 = t->q1;
		bufdelete(&snarfbuf, 0, snarfbuf.nc);
		r = fbufalloc();
		while(q0 < q1){
			n = q1 - q0;
			if(n > RBUFSIZE)
				n = RBUFSIZE;
			bufread(&t->file->b, q0, r, n);
			bufinsert(&snarfbuf, snarfbuf.nc, r, n);
			q0 += n;
		}
		fbuffree(r);
		acmeputsnarf();
	}
	if(docut){
		textdelete(t, t->q0, t->q1, TRUE);
		textsetselect(t, t->q0, t->q0);
		if(t->w){
			textscrdraw(t);
			winsettag(t->w);
		}
	}else if(dosnarf)	/* Snarf command */
		argtext = t;
	if(locked)
		winunlock(t->w);
}

void
paste(Text *et, Text *t, Text *_0, int selectall, int tobody, Rune *_1, int _2)
{
	int c;
	uint q, q0, q1, n;
	Rune *r;

	USED(_0);
	USED(_1);
	USED(_2);

	/* if(tobody), use body of executing window  (Paste or Send command) */
	if(tobody && et!=nil && et->w!=nil){
		t = &et->w->body;
		filemark(t->file);	/* seq has been incremented by execute */
	}
	if(t == nil)
		return;

	acmegetsnarf();
	if(t==nil || snarfbuf.nc==0)
		return;
	if(t->w!=nil && et->w!=t->w){
		c = 'M';
		if(et->w)
			c = et->w->owner;
		winlock(t->w, c);
	}
	cut(t, t, nil, FALSE, TRUE, nil, 0);
	q = 0;
	q0 = t->q0;
	q1 = t->q0+snarfbuf.nc;
	r = fbufalloc();
	while(q0 < q1){
		n = q1 - q0;
		if(n > RBUFSIZE)
			n = RBUFSIZE;
		if(r == nil)
			r = runemalloc(n);
		bufread(&snarfbuf, q, r, n);
		textinsert(t, q0, r, n, TRUE);
		q += n;
		q0 += n;
	}
	fbuffree(r);
	if(selectall)
		textsetselect(t, t->q0, q1);
	else
		textsetselect(t, q1, q1);
	if(t->w){
		textscrdraw(t);
		winsettag(t->w);
	}
	if(t->w!=nil && et->w!=t->w)
		winunlock(t->w);
}

void
look(Text *et, Text *t, Text *argt, int _0, int _1, Rune *arg, int narg)
{
	Rune *r;
	int n;

	USED(_0);
	USED(_1);

	if(et && et->w){
		t = &et->w->body;
		if(narg > 0){
			search(t, arg, narg, FALSE);
			return;
		}
		getarg(argt, FALSE, FALSE, &r, &n);
		if(r == nil){
			n = t->q1-t->q0;
			r = runemalloc(n);
			bufread(&t->file->b, t->q0, r, n);
		}
		search(t, r, n, FALSE);
		free(r);
	}
}

static Rune Lnl[] = { '\n', 0 };

void
sendx(Text *et, Text *t, Text *_0, int _1, int _2, Rune *_3, int _4)
{
	USED(_0);
	USED(_1);
	USED(_2);
	USED(_3);
	USED(_4);

	if(et->w==nil)
		return;
	t = &et->w->body;
	if(t->q0 != t->q1)
		cut(t, t, nil, TRUE, FALSE, nil, 0);
	textsetselect(t, t->file->b.nc, t->file->b.nc);
	paste(t, t, nil, TRUE, TRUE, nil, 0);
	if(textreadc(t, t->file->b.nc-1) != '\n'){
		textinsert(t, t->file->b.nc, Lnl, 1, TRUE);
		textsetselect(t, t->file->b.nc, t->file->b.nc);
	}
	t->iq1 = t->q1;
	textshow(t, t->q1, t->q1, 1);
}

void
edit(Text *et, Text *_0, Text *argt, int _1, int _2, Rune *arg, int narg)
{
	Rune *r;
	int len;

	USED(_0);
	USED(_1);
	USED(_2);

	if(et == nil)
		return;
	getarg(argt, FALSE, TRUE, &r, &len);
	seq++;
	if(r != nil){
		editcmd(et, r, len);
		free(r);
	}else
		editcmd(et, arg, narg);
}

void
xexit(Text *et, Text *_0, Text *_1, int _2, int _3, Rune *_4, int _5)
{
	USED(et);
	USED(_0);
	USED(_1);
	USED(_2);
	USED(_3);
	USED(_4);
	USED(_5);

	if(rowclean(&row)){
		sendul(cexit, 0);
		threadexits(nil);
	}
}

void
putall(Text *et, Text *_0, Text *_1, int _2, int _3, Rune *_4, int _5)
{
	int i, j, e;
	Window *w;
	Column *c;
	char *a;

	USED(et);
	USED(_0);
	USED(_1);
	USED(_2);
	USED(_3);
	USED(_4);
	USED(_5);

	for(i=0; i<row.ncol; i++){
		c = row.col[i];
		for(j=0; j<c->nw; j++){
			w = c->w[j];
			if(w->isscratch || w->isdir || w->body.file->nname==0)
				continue;
			if(w->nopen[QWevent] > 0)
				continue;
			a = runetobyte(w->body.file->name, w->body.file->nname);
			e = access(a, 0);
			if(w->body.file->mod || w->body.ncache)
				if(e < 0)
					warning(nil, "no auto-Put of %s: %r\n", a);
				else{
					wincommit(w, &w->body);
					put(&w->body, nil, nil, XXX, XXX, nil, 0);
				}
			free(a);
		}
	}
}


void
id(Text *et, Text *_0, Text *_1, int _2, int _3, Rune *_4, int _5)
{
	USED(_0);
	USED(_1);
	USED(_2);
	USED(_3);
	USED(_4);
	USED(_5);

	if(et && et->w)
		warning(nil, "/mnt/acme/%d/\n", et->w->id);
}

void
local(Text *et, Text *_0, Text *argt, int _1, int _2, Rune *arg, int narg)
{
	char *a, *aa;
	Runestr dir;

	USED(_0);
	USED(_1);
	USED(_2);

	aa = getbytearg(argt, TRUE, TRUE, &a);

	dir = dirname(et, nil, 0);
	if(dir.nr==1 && dir.r[0]=='.'){	/* sigh */
		free(dir.r);
		dir.r = nil;
		dir.nr = 0;
	}
	run(nil, runetobyte(arg, narg), dir.r, dir.nr, FALSE, aa, a, FALSE);
}

void
xkill(Text *_0, Text *_1, Text *argt, int _2, int _3, Rune *arg, int narg)
{
	Rune *a, *cmd, *r;
	int na;

	USED(_0);
	USED(_1);
	USED(_2);
	USED(_3);

	getarg(argt, FALSE, FALSE, &r, &na);
	if(r)
		xkill(nil, nil, nil, 0, 0, r, na);
	/* loop condition: *arg is not a blank */
	for(;;){
		a = findbl(arg, narg, &na);
		if(a == arg)
			break;
		cmd = runemalloc(narg-na+1);
		runemove(cmd, arg, narg-na);
		sendp(ckill, cmd);
		arg = skipbl(a, na, &narg);
	}
}

static Rune Lfix[] = { 'f', 'i', 'x', 0 };
static Rune Lvar[] = { 'v', 'a', 'r', 0 };

void
fontx(Text *et, Text *t, Text *argt, int _0, int _1, Rune *arg, int narg)
{
	Rune *a, *r, *flag, *file;
	int na, nf;
	char *aa;
	Reffont *newfont;
	Dirlist *dp;
	int i, fix;

	USED(_0);
	USED(_1);

	if(et==nil || et->w==nil)
		return;
	t = &et->w->body;
	flag = nil;
	file = nil;
	/* loop condition: *arg is not a blank */
	nf = 0;
	for(;;){
		a = findbl(arg, narg, &na);
		if(a == arg)
			break;
		r = runemalloc(narg-na+1);
		runemove(r, arg, narg-na);
		if(runeeq(r, narg-na, Lfix, 3) || runeeq(r, narg-na, Lvar, 3)){
			free(flag);
			flag = r;
		}else{
			free(file);
			file = r;
			nf = narg-na;
		}
		arg = skipbl(a, na, &narg);
	}
	getarg(argt, FALSE, TRUE, &r, &na);
	if(r)
		if(runeeq(r, na, Lfix, 3) || runeeq(r, na, Lvar, 3)){
			free(flag);
			flag = r;
		}else{
			free(file);
			file = r;
			nf = na;
		}
	fix = 1;
	if(flag)
		fix = runeeq(flag, runestrlen(flag), Lfix, 3);
	else if(file == nil){
		newfont = rfget(FALSE, FALSE, FALSE, nil);
		if(newfont)
			fix = strcmp(newfont->f->name, t->fr.font->name)==0;
	}
	if(file){
		aa = runetobyte(file, nf);
		newfont = rfget(fix, flag!=nil, FALSE, aa);
		free(aa);
	}else
		newfont = rfget(fix, FALSE, FALSE, nil);
	if(newfont){
		draw(screen, t->w->r, textcols[BACK], nil, ZP);
		rfclose(t->reffont);
		t->reffont = newfont;
		t->fr.font = newfont->f;
		frinittick(&t->fr);
		if(t->w->isdir){
			t->all.min.x++;	/* force recolumnation; disgusting! */
			for(i=0; i<t->w->ndl; i++){
				dp = t->w->dlp[i];
				aa = runetobyte(dp->r, dp->nr);
				dp->wid = stringwidth(newfont->f, aa);
				free(aa);
			}
		}
		/* avoid shrinking of window due to quantization */
		colgrow(t->w->col, t->w, -1);
	}
	free(file);
	free(flag);
}

void
incl(Text *et, Text *_0, Text *argt, int _1, int _2, Rune *arg, int narg)
{
	Rune *a, *r;
	Window *w;
	int na, n, len;

	USED(_0);
	USED(_1);
	USED(_2);

	if(et==nil || et->w==nil)
		return;
	w = et->w;
	n = 0;
	getarg(argt, FALSE, TRUE, &r, &len);
	if(r){
		n++;
		winaddincl(w, r, len);
	}
	/* loop condition: *arg is not a blank */
	for(;;){
		a = findbl(arg, narg, &na);
		if(a == arg)
			break;
		r = runemalloc(narg-na+1);
		runemove(r, arg, narg-na);
		n++;
		winaddincl(w, r, narg-na);
		arg = skipbl(a, na, &narg);
	}
	if(n==0 && w->nincl){
		for(n=w->nincl; --n>=0; )
			warning(nil, "%S ", w->incl[n]);
		warning(nil, "\n");
	}
}

static Rune LON[] = { 'O', 'N', 0 };
static Rune LOFF[] = { 'O', 'F', 'F', 0 };
static Rune Lon[] = { 'o', 'n', 0 };

enum {
	IGlobal = -2,
	IError = -1,
	Ion = 0,
	Ioff = 1
};

static int
indentval(Rune *s, int n)
{
	if(n < 2)
		return IError;
	if(runestrncmp(s, LON, n) == 0){
		globalautoindent = TRUE;
		warning(nil, "Indent ON\n");
		return IGlobal;
	}
	if(runestrncmp(s, LOFF, n) == 0){
		globalautoindent = FALSE;
		warning(nil, "Indent OFF\n");
		return IGlobal;
	}
	return runestrncmp(s, Lon, n) == 0;
}

static void
fixindent(Window *w, void *arg)
{
	USED(arg);
	w->autoindent = globalautoindent;
}

void
indent(Text *et, Text *_0, Text *argt, int _1, int _2, Rune *arg, int narg)
{
	Rune *a, *r;
	Window *w;
	int na, len, autoindent;

	USED(_0);
	USED(_1);
	USED(_2);

	w = nil;
	if(et!=nil && et->w!=nil)
		w = et->w;
	autoindent = IError;
	getarg(argt, FALSE, TRUE, &r, &len);
	if(r!=nil && len>0)
		autoindent = indentval(r, len);
	else{
		a = findbl(arg, narg, &na);
		if(a != arg)
			autoindent = indentval(arg, narg-na);
	}
	if(autoindent == IGlobal)
		allwindows(fixindent, nil);
	else if(w != nil && autoindent >= 0)
		w->autoindent = autoindent;
}

void
tab(Text *et, Text *_0, Text *argt, int _1, int _2, Rune *arg, int narg)
{
	Rune *a, *r;
	Window *w;
	int na, len, tab;
	char *p;

	USED(_0);
	USED(_1);
	USED(_2);

	if(et==nil || et->w==nil)
		return;
	w = et->w;
	getarg(argt, FALSE, TRUE, &r, &len);
	tab = 0;
	if(r!=nil && len>0){
		p = runetobyte(r, len);
		if('0'<=p[0] && p[0]<='9')
			tab = atoi(p);
		free(p);
	}else{
		a = findbl(arg, narg, &na);
		if(a != arg){
			p = runetobyte(arg, narg-na);
			if('0'<=p[0] && p[0]<='9')
				tab = atoi(p);
			free(p);
		}
	}
	if(tab > 0){
		if(w->body.tabstop != tab){
			w->body.tabstop = tab;
			winresize(w, w->r, FALSE, TRUE);
		}
	}else
		warning(nil, "%.*S: Tab %d\n", w->body.file->nname, w->body.file->name, w->body.tabstop);
}

void
runproc(void *argvp)
{
	/* args: */
		Window *win;
		char *s;
		Rune *rdir;
		int ndir;
		int newns;
		char *argaddr;
		char *arg;
		Command *c;
		Channel *cpid;
		int iseditcmd;
	/* end of args */
	char *e, *t, *name, *filename, *dir, **av, *news;
	Rune r, **incl;
	int ac, w, inarg, i, n, fd, nincl, winid;
	int sfd[3];
	int pipechar;
	char buf[512];
	int ret;
	/*static void *parg[2]; */
	char *rcarg[4];
	void **argv;
	CFsys *fs;
	char *shell;

	threadsetname("runproc");

	argv = argvp;
	win = argv[0];
	s = argv[1];
	rdir = argv[2];
	ndir = (uintptr)argv[3];
	newns = (uintptr)argv[4];
	argaddr = argv[5];
	arg = argv[6];
	c = argv[7];
	cpid = argv[8];
	iseditcmd = (uintptr)argv[9];
	free(argv);

	unsetenv("acmeaddr");
	unsetenv("winid");
	unsetenv("%");
	unsetenv("samfile");

	t = s;
	while(*t==' ' || *t=='\n' || *t=='\t')
		t++;
	for(e=t; *e; e++)
		if(*e==' ' || *e=='\n' || *e=='\t' )
			break;
	name = emalloc((e-t)+2);
	memmove(name, t, e-t);
	name[e-t] = 0;
	e = utfrrune(name, '/');
	if(e)
		memmove(name, e+1, strlen(e+1)+1);	/* strcpy but overlaps */
	strcat(name, " ");	/* add blank here for ease in waittask */
	c->name = bytetorune(name, &c->nname);
	free(name);
	pipechar = 0;
	if(*t=='<' || *t=='|' || *t=='>')
		pipechar = *t++;
	c->iseditcmd = iseditcmd;
	c->text = s;
	if(newns){
		nincl = 0;
		incl = nil;
		if(win){
			filename = smprint("%.*S", win->body.file->nname, win->body.file->name);
			nincl = win->nincl;
			if(nincl > 0){
				incl = emalloc(nincl*sizeof(Rune*));
				for(i=0; i<nincl; i++){
					n = runestrlen(win->incl[i]);
					incl[i] = runemalloc(n+1);
					runemove(incl[i], win->incl[i], n);
				}
			}
			winid = win->id;
		}else{
			filename = nil;
			winid = 0;
			if(activewin)
				winid = activewin->id;
		}
		rfork(RFNAMEG|RFENVG|RFFDG|RFNOTEG);
		sprint(buf, "%d", winid);
		putenv("winid", buf);

		if(filename){
			putenv("%", filename);
			putenv("samfile", filename);
			free(filename);
		}
		c->md = fsysmount(rdir, ndir, incl, nincl);
		if(c->md == nil){
			fprint(2, "child: can't allocate mntdir: %r\n");
			threadexits("fsysmount");
		}
		sprint(buf, "%d", c->md->id);
		if((fs = nsmount("acme", buf)) == nil){
			fprint(2, "child: can't mount acme: %r\n");
			fsysdelid(c->md);
			c->md = nil;
			threadexits("nsmount");
		}
		if(winid>0 && (pipechar=='|' || pipechar=='>')){
			sprint(buf, "%d/rdsel", winid);
			sfd[0] = fsopenfd(fs, buf, OREAD);
		}else
			sfd[0] = open("/dev/null", OREAD);
		if((winid>0 || iseditcmd) && (pipechar=='|' || pipechar=='<')){
			if(iseditcmd){
				if(winid > 0)
					sprint(buf, "%d/editout", winid);
				else
					sprint(buf, "editout");
			}else
				sprint(buf, "%d/wrsel", winid);
			sfd[1] = fsopenfd(fs, buf, OWRITE);
			sfd[2] = fsopenfd(fs, "cons", OWRITE);
		}else{
			sfd[1] = fsopenfd(fs, "cons", OWRITE);
			sfd[2] = sfd[1];
		}
		fsunmount(fs);
	}else{
		rfork(RFFDG|RFNOTEG);
		fsysclose();
		sfd[0] = open("/dev/null", OREAD);
		sfd[1] = open("/dev/null", OWRITE);
		sfd[2] = dup(erroutfd, -1);
	}
	if(win)
		winclose(win);

	if(argaddr)
		putenv("acmeaddr", argaddr);
	if(acmeshell != nil)
		goto Hard;
	if(strlen(t) > sizeof buf-10)	/* may need to print into stack */
		goto Hard;
	inarg = FALSE;
	for(e=t; *e; e+=w){
		w = chartorune(&r, e);
		if(r==' ' || r=='\t')
			continue;
		if(r < ' ')
			goto Hard;
		if(utfrune("#;&|^$=`'{}()<>[]*?^~`/", r))
			goto Hard;
		inarg = TRUE;
	}
	if(!inarg)
		goto Fail;

	ac = 0;
	av = nil;
	inarg = FALSE;
	for(e=t; *e; e+=w){
		w = chartorune(&r, e);
		if(r==' ' || r=='\t'){
			inarg = FALSE;
			*e = 0;
			continue;
		}
		if(!inarg){
			inarg = TRUE;
			av = realloc(av, (ac+1)*sizeof(char**));
			av[ac++] = e;
		}
	}
	av = realloc(av, (ac+2)*sizeof(char**));
	av[ac++] = arg;
	av[ac] = nil;
	c->av = av;

	dir = nil;
	if(rdir != nil)
		dir = runetobyte(rdir, ndir);
	ret = threadspawnd(sfd, av[0], av, dir);
	free(dir);
	if(ret >= 0){
		if(cpid)
			sendul(cpid, ret);
		threadexits("");
	}
/* libthread uses execvp so no need to do this */
#if 0
	e = av[0];
	if(e[0]=='/' || (e[0]=='.' && e[1]=='/'))
		goto Fail;
	if(cputype){
		sprint(buf, "%s/%s", cputype, av[0]);
		procexec(cpid, sfd, buf, av);
	}
	sprint(buf, "/bin/%s", av[0]);
	procexec(cpid, sfd, buf, av);
#endif
	goto Fail;

Hard:
	/*
	 * ugly: set path = (. $cputype /bin)
	 * should honor $path if unusual.
	 */
	if(cputype){
		n = 0;
		memmove(buf+n, ".", 2);
		n += 2;
		i = strlen(cputype)+1;
		memmove(buf+n, cputype, i);
		n += i;
		memmove(buf+n, "/bin", 5);
		n += 5;
		fd = create("/env/path", OWRITE, 0666);
		write(fd, buf, n);
		close(fd);
	}

	if(arg){
		news = emalloc(strlen(t) + 1 + 1 + strlen(arg) + 1 + 1);
		if(news){
			sprint(news, "%s '%s'", t, arg);	/* BUG: what if quote in arg? */
			free(s);
			t = news;
			c->text = news;
		}
	}
	dir = nil;
	if(rdir != nil)
		dir = runetobyte(rdir, ndir);
	shell = acmeshell;
	if(shell == nil)
		shell = "rc";
	rcarg[0] = shell;
	rcarg[1] = "-c";
	rcarg[2] = t;
	rcarg[3] = nil;
	ret = threadspawnd(sfd, rcarg[0], rcarg, dir);
	unsetenv("acmeaddr");
	unsetenv("winid");
	unsetenv("%");
	unsetenv("samfile");
	free(dir);
	if(ret >= 0){
		if(cpid)
			sendul(cpid, ret);
		threadexits(nil);
	}
	warning(nil, "exec %s: %r\n", shell);

   Fail:
	/* threadexec hasn't happened, so send a zero */
	close(sfd[0]);
	close(sfd[1]);
	if(sfd[2] != sfd[1])
		close(sfd[2]);
	sendul(cpid, 0);
	threadexits(nil);
}

void
runwaittask(void *v)
{
	Command *c;
	Channel *cpid;
	void **a;

	threadsetname("runwaittask");
	a = v;
	c = a[0];
	cpid = a[1];
	free(a);
	do
		c->pid = recvul(cpid);
	while(c->pid == ~0);
	free(c->av);
	if(c->pid != 0)	/* successful exec */
		sendp(ccommand, c);
	else{
		if(c->iseditcmd)
			sendul(cedit, 0);
		free(c->name);
		free(c->text);
		free(c);
	}
	chanfree(cpid);
}

void
run(Window *win, char *s, Rune *rdir, int ndir, int newns, char *argaddr, char *xarg, int iseditcmd)
{
	void **arg;
	Command *c;
	Channel *cpid;

	if(s == nil)
		return;

	arg = emalloc(10*sizeof(void*));
	c = emalloc(sizeof *c);
	cpid = chancreate(sizeof(ulong), 0);
	chansetname(cpid, "cpid %s", s);
	arg[0] = win;
	arg[1] = s;
	arg[2] = rdir;
	arg[3] = (void*)(uintptr)ndir;
	arg[4] = (void*)(uintptr)newns;
	arg[5] = argaddr;
	arg[6] = xarg;
	arg[7] = c;
	arg[8] = cpid;
	arg[9] = (void*)(uintptr)iseditcmd;
	threadcreate(runproc, arg, STACK);
	/* mustn't block here because must be ready to answer mount() call in run() */
	arg = emalloc(2*sizeof(void*));
	arg[0] = c;
	arg[1] = cpid;
	threadcreate(runwaittask, arg, STACK);
}
