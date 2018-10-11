#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
#include <plumb.h>
#include <libsec.h>
#include "dat.h"
#include "fns.h"

static	Point		prevmouse;
static	Window	*mousew;

Range
range(int q0, int q1)
{
	Range r;

	r.q0 = q0;
	r.q1 = q1;
	return r;
}

Runestr
runestr(Rune *r, uint n)
{
	Runestr rs;

	rs.r = r;
	rs.nr = n;
	return rs;
}

void
cvttorunes(char *p, int n, Rune *r, int *nb, int *nr, int *nulls)
{
	uchar *q;
	Rune *s;
	int j, w;

	/*
	 * Always guaranteed that n bytes may be interpreted
	 * without worrying about partial runes.  This may mean
	 * reading up to UTFmax-1 more bytes than n; the caller
	 * knows this.  If n is a firm limit, the caller should
	 * set p[n] = 0.
	 */
	q = (uchar*)p;
	s = r;
	for(j=0; j<n; j+=w){
		if(*q < Runeself){
			w = 1;
			*s = *q++;
		}else{
			w = chartorune(s, (char*)q);
			q += w;
		}
		if(*s)
			s++;
		else if(nulls)
			*nulls = TRUE;
	}
	*nb = (char*)q-p;
	*nr = s-r;
}

void
error(char *s)
{
	fprint(2, "acme: %s: %r\n", s);
	threadexitsall(nil);
}

Window*
errorwin1(Rune *dir, int ndir, Rune **incl, int nincl)
{
	Window *w;
	Rune *r;
	int i, n;
	static Rune Lpluserrors[] = { '+', 'E', 'r', 'r', 'o', 'r', 's', 0 };

	r = runemalloc(ndir+8);
	if((n = ndir) != 0){
		runemove(r, dir, ndir);
		r[n++] = L'/';
	}
	runemove(r+n, Lpluserrors, 7);
	n += 7;
	w = lookfile(r, n);
	if(w == nil){
		if(row.ncol == 0)
			if(rowadd(&row, nil, -1) == nil)
				error("can't create column to make error window");
		w = coladd(row.col[row.ncol-1], nil, nil, -1);
		w->filemenu = FALSE;
		winsetname(w, r, n);
		xfidlog(w, "new");
	}
	free(r);
	for(i=nincl; --i>=0; ){
		n = runestrlen(incl[i]);
		r = runemalloc(n);
		runemove(r, incl[i], n);
		winaddincl(w, r, n);
	}
	for(i=0; i<NINDENT; i++)
		w->indent[i] = globalindent[i];
	return w;
}

/* make new window, if necessary; return with it locked */
Window*
errorwin(Mntdir *md, int owner)
{
	Window *w;

	for(;;){
		if(md == nil)
			w = errorwin1(nil, 0, nil, 0);
		else
			w = errorwin1(md->dir, md->ndir, md->incl, md->nincl);
		winlock(w, owner);
		if(w->col != nil)
			break;
		/* window was deleted too fast */
		winunlock(w);
	}
	return w;
}

/*
 * Incoming window should be locked. 
 * It will be unlocked and returned window
 * will be locked in its place.
 */
Window*
errorwinforwin(Window *w)
{
	int i, n, nincl, owner;
	Rune **incl;
	Runestr dir;
	Text *t;

	t = &w->body;
	dir = dirname(t, nil, 0);
	if(dir.nr==1 && dir.r[0]=='.'){	/* sigh */
		free(dir.r);
		dir.r = nil;
		dir.nr = 0;
	}
	incl = nil;
	nincl = w->nincl;
	if(nincl > 0){
		incl = emalloc(nincl*sizeof(Rune*));
		for(i=0; i<nincl; i++){
			n = runestrlen(w->incl[i]);
			incl[i] = runemalloc(n+1);
			runemove(incl[i], w->incl[i], n);
		}
	}
	owner = w->owner;
	winunlock(w);
	for(;;){
		w = errorwin1(dir.r, dir.nr, incl, nincl);
		winlock(w, owner);
		if(w->col != nil)
			break;
		/* window deleted too fast */
		winunlock(w);
	}
	return w;
}

typedef struct Warning Warning;

struct Warning{
	Mntdir *md;
	Buffer buf;
	Warning *next;
};

static Warning *warnings;

static
void
addwarningtext(Mntdir *md, Rune *r, int nr)
{
	Warning *warn;
	
	for(warn = warnings; warn; warn=warn->next){
		if(warn->md == md){
			bufinsert(&warn->buf, warn->buf.nc, r, nr);
			return;
		}
	}
	warn = emalloc(sizeof(Warning));
	warn->next = warnings;
	warn->md = md;
	if(md)
		fsysincid(md);
	warnings = warn;
	bufinsert(&warn->buf, 0, r, nr);
	nbsendp(cwarn, 0);
}

/* called while row is locked */
void
flushwarnings(void)
{
	Warning *warn, *next;
	Window *w;
	Text *t;
	int owner, nr, q0, n;
	Rune *r;

	for(warn=warnings; warn; warn=next) {
		w = errorwin(warn->md, 'E');
		t = &w->body;
		owner = w->owner;
		if(owner == 0)
			w->owner = 'E';
		wincommit(w, t);
		/*
		 * Most commands don't generate much output. For instance,
		 * Edit ,>cat goes through /dev/cons and is already in blocks
		 * because of the i/o system, but a few can.  Edit ,p will
		 * put the entire result into a single hunk.  So it's worth doing
		 * this in blocks (and putting the text in a buffer in the first
		 * place), to avoid a big memory footprint.
		 */
		r = fbufalloc();
		q0 = t->file->b.nc;
		for(n = 0; n < warn->buf.nc; n += nr){
			nr = warn->buf.nc - n;
			if(nr > RBUFSIZE)
				nr = RBUFSIZE;
			bufread(&warn->buf, n, r, nr);
			textbsinsert(t, t->file->b.nc, r, nr, TRUE, &nr);
		}
		textshow(t, q0, t->file->b.nc, 1);
		free(r);
		winsettag(t->w);
		textscrdraw(t);
		w->owner = owner;
		w->dirty = FALSE;
		winunlock(w);
		bufclose(&warn->buf);
		next = warn->next;
		if(warn->md)
			fsysdelid(warn->md);
		free(warn);
	}
	warnings = nil;
}

void
warning(Mntdir *md, char *s, ...)
{
	Rune *r;
	va_list arg;

	va_start(arg, s);
	r = runevsmprint(s, arg);
	va_end(arg);
	if(r == nil)
		error("runevsmprint failed");
	addwarningtext(md, r, runestrlen(r));
	free(r);
}

int
runeeq(Rune *s1, uint n1, Rune *s2, uint n2)
{
	if(n1 != n2)
		return FALSE;
	return memcmp(s1, s2, n1*sizeof(Rune)) == 0;
}

uint
min(uint a, uint b)
{
	if(a < b)
		return a;
	return b;
}

uint
max(uint a, uint b)
{
	if(a > b)
		return a;
	return b;
}

char*
runetobyte(Rune *r, int n)
{
	char *s;

	if(r == nil)
		return nil;
	s = emalloc(n*UTFmax+1);
	setmalloctag(s, getcallerpc(&r));
	snprint(s, n*UTFmax+1, "%.*S", n, r);
	return s;
}

Rune*
bytetorune(char *s, int *ip)
{
	Rune *r;
	int nb, nr;

	nb = strlen(s);
	r = runemalloc(nb+1);
	cvttorunes(s, nb, r, &nb, &nr, nil);
	r[nr] = '\0';
	*ip = nr;
	return r;
}

int
isalnum(Rune c)
{
	/*
	 * Hard to get absolutely right.  Use what we know about ASCII
	 * and assume anything above the Latin control characters is
	 * potentially an alphanumeric.
	 */
	if(c <= ' ')
		return FALSE;
	if(0x7F<=c && c<=0xA0)
		return FALSE;
	if(utfrune("!\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~", c))
		return FALSE;
	return TRUE;
}

int
rgetc(void *v, uint n)
{
	return ((Rune*)v)[n];
}

int
tgetc(void *a, uint n)
{
	Text *t;

	t = a;
	if(n >= t->file->b.nc)
		return 0;
	return textreadc(t, n);
}

Rune*
skipbl(Rune *r, int n, int *np)
{
	while(n>0 && (*r==' ' || *r=='\t' || *r=='\n')){
		--n;
		r++;
	}
	*np = n;
	return r;
}

Rune*
findbl(Rune *r, int n, int *np)
{
	while(n>0 && *r!=' ' && *r!='\t' && *r!='\n'){
		--n;
		r++;
	}
	*np = n;
	return r;
}

void
savemouse(Window *w)
{
	prevmouse = mouse->xy;
	mousew = w;
}

int
restoremouse(Window *w)
{
	int did;

	did = 0;
	if(mousew!=nil && mousew==w) {
		moveto(mousectl, prevmouse);
		did = 1;
	}
	mousew = nil;
	return did;
}

void
clearmouse()
{
	mousew = nil;
}

char*
estrdup(char *s)
{
	char *t;

	t = strdup(s);
	if(t == nil)
		error("strdup failed");
	setmalloctag(t, getcallerpc(&s));
	return t;
}

void*
emalloc(uint n)
{
	void *p;

	p = malloc(n);
	if(p == nil)
		error("malloc failed");
	setmalloctag(p, getcallerpc(&n));
	memset(p, 0, n);
	return p;
}

void*
erealloc(void *p, uint n)
{
	p = realloc(p, n);
	if(p == nil)
		error("realloc failed");
	setmalloctag(p, getcallerpc(&n));
	return p;
}

/*
 * Heuristic city.
 */
Window*
makenewwindow(Text *t)
{
	Column *c;
	Window *w, *bigw, *emptyw;
	Text *emptyb;
	int i, y, el;

	if(activecol)
		c = activecol;
	else if(seltext && seltext->col)
		c = seltext->col;
	else if(t && t->col)
		c = t->col;
	else{
		if(row.ncol==0 && rowadd(&row, nil, -1)==nil)
			error("can't make column");
		c = row.col[row.ncol-1];
	}
	activecol = c;
	if(t==nil || t->w==nil || c->nw==0)
		return coladd(c, nil, nil, -1);

	/* find biggest window and biggest blank spot */
	emptyw = c->w[0];
	bigw = emptyw;
	for(i=1; i<c->nw; i++){
		w = c->w[i];
		/* use >= to choose one near bottom of screen */
		if(w->body.fr.maxlines >= bigw->body.fr.maxlines)
			bigw = w;
		if(w->body.fr.maxlines-w->body.fr.nlines >= emptyw->body.fr.maxlines-emptyw->body.fr.nlines)
			emptyw = w;
	}
	emptyb = &emptyw->body;
	el = emptyb->fr.maxlines-emptyb->fr.nlines;
	/* if empty space is big, use it */
	if(el>15 || (el>3 && el>(bigw->body.fr.maxlines-1)/2))
		y = emptyb->fr.r.min.y+emptyb->fr.nlines*font->height;
	else{
		/* if this window is in column and isn't much smaller, split it */
		if(t->col==c && Dy(t->w->r)>2*Dy(bigw->r)/3)
			bigw = t->w;
		y = (bigw->r.min.y + bigw->r.max.y)/2;
	}
	w = coladd(c, nil, nil, y);
	if(w->body.fr.maxlines < 2)
		colgrow(w->col, w, 1);
	return w;
}
