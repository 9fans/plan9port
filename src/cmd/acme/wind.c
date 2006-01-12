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
#include "dat.h"
#include "fns.h"

int	winid;

void
wininit(Window *w, Window *clone, Rectangle r)
{
	Rectangle r1, br;
	File *f;
	Reffont *rf;
	Rune *rp;
	int nc;

	w->tag.w = w;
	w->taglines = 1;
	w->tagexpand = TRUE;
	w->body.w = w;
	w->id = ++winid;
	incref(&w->ref);
	if(globalincref)
		incref(&w->ref);
	w->ctlfid = ~0;
	w->utflastqid = -1;
	r1 = r;

	w->tagtop = r;
	w->tagtop.max.y = r.min.y + font->height;

	r1.max.y = r1.min.y + w->taglines*font->height;
	incref(&reffont.ref);
	f = fileaddtext(nil, &w->tag);
	textinit(&w->tag, f, r1, &reffont, tagcols);
	w->tag.what = Tag;
	/* tag is a copy of the contents, not a tracked image */
	if(clone){
		textdelete(&w->tag, 0, w->tag.file->b.nc, TRUE);
		nc = clone->tag.file->b.nc;
		rp = runemalloc(nc);
		bufread(&clone->tag.file->b, 0, rp, nc);
		textinsert(&w->tag, 0, rp, nc, TRUE);
		free(rp);
		filereset(w->tag.file);
		textsetselect(&w->tag, nc, nc);
	}
//assert(w->body.w == w);
	r1 = r;
	r1.min.y += w->taglines*font->height + 1;
	if(r1.max.y < r1.min.y)
		r1.max.y = r1.min.y;
	f = nil;
	if(clone){
		f = clone->body.file;
		w->body.org = clone->body.org;
		w->isscratch = clone->isscratch;
		rf = rfget(FALSE, FALSE, FALSE, clone->body.reffont->f->name);
	}else
		rf = rfget(FALSE, FALSE, FALSE, nil);
//assert(w->body.w == w);
	f = fileaddtext(f, &w->body);
	w->body.what = Body;
	textinit(&w->body, f, r1, rf, textcols);
	r1.min.y -= 1;
	r1.max.y = r1.min.y+1;
	draw(screen, r1, tagcols[BORD], nil, ZP);
	textscrdraw(&w->body);
	w->r = r;
	br.min = w->tag.scrollr.min;
	br.max.x = br.min.x + Dx(button->r);
	br.max.y = br.min.y + Dy(button->r);
	draw(screen, br, button, nil, button->r.min);
	w->filemenu = TRUE;
	w->maxlines = w->body.fr.maxlines;
	w->autoindent = globalautoindent;
//assert(w->body.w == w);
	if(clone){
		w->dirty = clone->dirty;
		w->autoindent = clone->autoindent;
		textsetselect(&w->body, clone->body.q0, clone->body.q1);
		winsettag(w);
	}
}

/*
 * Compute number of tag lines required
 * to display entire tag text.
 */
int
wintaglines(Window *w, Rectangle r)
{
	int n;
	Rune rune;

/* TAG policy here */

	if(!w->tagexpand)
		return 1;
	w->tag.fr.noredraw = 1;
	textresize(&w->tag, r, TRUE);
	w->tag.fr.noredraw = 0;
	
	/* can't use more than we have */
	if(w->tag.fr.nlines >= w->tag.fr.maxlines)
		return w->tag.fr.maxlines;

	/* if tag ends with \n, include empty line at end for typing */
	n = w->tag.fr.nlines;
	bufread(&w->tag.file->b, w->tag.file->b.nc-1, &rune, 1);
	if(rune == '\n')
		n++;
	return n;
}

int
winresize(Window *w, Rectangle r, int safe, int keepextra)
{
	int oy, y, mouseintag, tagresized;
	Image *b;
	Point p;
	Rectangle br, r1;

if(0) fprint(2, "winresize %d %R safe=%d keep=%d h=%d\n", w->id, r, safe, keepextra, font->height);
	w->tagtop = r;
	w->tagtop.max.y = r.min.y+font->height;

/* 
 * TAG If necessary, recompute the number of lines that should
 * be in the tag.
 */
	r1 = r;
	r1.max.y = min(r.max.y, r1.min.y + w->taglines*font->height);
	y = r1.max.y;
	mouseintag = ptinrect(mouse->xy, w->tag.all);
	if(!safe || !w->tagsafe || !eqrect(w->tag.all, r1)){

		w->taglines = wintaglines(w, r);
		w->tagsafe = TRUE;
	}
/* END TAG */

	r1 = r;
	r1.max.y = min(r.max.y, r1.min.y + w->taglines*font->height);
	y = r1.max.y;
	tagresized = 0;
	if(1 || !safe || !eqrect(w->tag.all, r1)){
		tagresized = 1;
if(0) fprint(2, "resize tag %R => %R\n", w->tag.all, r1);
		textresize(&w->tag, r1, TRUE);
if(0) fprint(2, "=> %R (%R)\n", w->tag.all, w->tag.fr.r);
		y = w->tag.fr.r.max.y;
		b = button;
		if(w->body.file->mod && !w->isdir && !w->isscratch)
			b = modbutton;
		br.min = w->tag.scrollr.min;
		br.max.x = br.min.x + Dx(b->r);
		br.max.y = br.min.y + Dy(b->r);
		draw(screen, br, b, nil, b->r.min);
/* TAG */
		if(mouseintag && !ptinrect(mouse->xy, w->tag.all)){
			p = mouse->xy;
			p.y = w->tag.all.max.y-3;
			moveto(mousectl, p);
		}
/* END TAG */
	}

	
	r1 = r;
	r1.min.y = y;
	if(tagresized || !safe || !eqrect(w->body.all, r1)){
		oy = y;
if(0) fprint(2, "resizing body; safe=%d all=%R r1=%R\n", safe, w->body.all, r1);
		if(y+1+w->body.fr.font->height <= r.max.y){	/* room for one line */
			r1.min.y = y;
			r1.max.y = y+1;
			draw(screen, r1, tagcols[BORD], nil, ZP);
			y++;
			r1.min.y = min(y, r.max.y);
			r1.max.y = r.max.y;
		}else{
			r1.min.y = y;
			r1.max.y = y;
		}
if(0) fprint(2, "resizing body; new r=%R; r1=%R\n", r, r1);
		w->r = r;
		w->r.max.y = textresize(&w->body, r1, keepextra);
if(0) fprint(2, "after textresize: body.all=%R\n", w->body.all);
		textscrdraw(&w->body);
		w->body.all.min.y = oy;
	}
	w->maxlines = min(w->body.fr.nlines, max(w->maxlines, w->body.fr.maxlines));
	return w->r.max.y;
}

void
winlock1(Window *w, int owner)
{
	incref(&w->ref);
	qlock(&w->lk);
	w->owner = owner;
}

void
winlock(Window *w, int owner)
{
	int i;
	File *f;

	f = w->body.file;
	for(i=0; i<f->ntext; i++)
		winlock1(f->text[i]->w, owner);
}

void
winunlock(Window *w)
{
	int i;
	File *f;

	/*
	 * subtle: loop runs backwards to avoid tripping over
	 * winclose indirectly editing f->text and freeing f
	 * on the last iteration of the loop.
	 */
	f = w->body.file;
	for(i=f->ntext-1; i>=0; i--){
		w = f->text[i]->w;
		w->owner = 0;
		qunlock(&w->lk);
		winclose(w);
	}
}

void
winmousebut(Window *w)
{
	moveto(mousectl, addpt(w->tag.scrollr.min, 
		divpt(Pt(Dx(w->tag.scrollr), font->height), 2)));
}

void
windirfree(Window *w)
{
	int i;
	Dirlist *dl;

	if(w->isdir){
		for(i=0; i<w->ndl; i++){
			dl = w->dlp[i];
			free(dl->r);
			free(dl);
		}
		free(w->dlp);
	}
	w->dlp = nil;
	w->ndl = 0;
}

void
winclose(Window *w)
{
	int i;

	if(decref(&w->ref) == 0){
		windirfree(w);
		textclose(&w->tag);
		textclose(&w->body);
		if(activewin == w)
			activewin = nil;
		for(i=0; i<w->nincl; i++)
			free(w->incl[i]);
		free(w->incl);
		free(w->events);
		free(w);
	}
}

void
windelete(Window *w)
{
	Xfid *x;

	x = w->eventx;
	if(x){
		w->nevents = 0;
		free(w->events);
		w->events = nil;
		w->eventx = nil;
		sendp(x->c, nil);	/* wake him up */
	}
}

void
winundo(Window *w, int isundo)
{
	Text *body;
	int i;
	File *f;
	Window *v;

	w->utflastqid = -1;
	body = &w->body;
	fileundo(body->file, isundo, &body->q0, &body->q1);
	textshow(body, body->q0, body->q1, 1);
	f = body->file;
	for(i=0; i<f->ntext; i++){
		v = f->text[i]->w;
		v->dirty = (f->seq != v->putseq);
		if(v != w){
			v->body.q0 = v->body.fr.p0+v->body.org;
			v->body.q1 = v->body.fr.p1+v->body.org;
		}
	}
	winsettag(w);
}

void
winsetname(Window *w, Rune *name, int n)
{
	Text *t;
	Window *v;
	int i;
	static Rune Lslashguide[] = { '/', 'g', 'u', 'i', 'd', 'e', 0 };
	static Rune Lpluserrors[] = { '+', 'E', 'r', 'r', 'o', 'r', 's', 0 };
	t = &w->body;
	if(runeeq(t->file->name, t->file->nname, name, n) == TRUE)
		return;
	w->isscratch = FALSE;
	if(n>=6 && runeeq(Lslashguide, 6, name+(n-6), 6))
		w->isscratch = TRUE;
	else if(n>=7 && runeeq(Lpluserrors, 7, name+(n-7), 7))
		w->isscratch = TRUE;
	filesetname(t->file, name, n);
	for(i=0; i<t->file->ntext; i++){
		v = t->file->text[i]->w;
		winsettag(v);
		v->isscratch = w->isscratch;
	}
}

void
wintype(Window *w, Text *t, Rune r)
{
	int i;

	texttype(t, r);
	if(t->what == Body)
		for(i=0; i<t->file->ntext; i++)
			textscrdraw(t->file->text[i]);
	winsettag(w);
}

void
wincleartag(Window *w)
{
	int i, n;
	Rune *r;

	/* w must be committed */
	n = w->tag.file->b.nc;
	r = runemalloc(n);
	bufread(&w->tag.file->b, 0, r, n);
	for(i=0; i<n; i++)
		if(r[i]==' ' || r[i]=='\t')
			break;
	for(; i<n; i++)
		if(r[i] == '|')
			break;
	if(i == n)
		return;
	i++;
	textdelete(&w->tag, i, n, TRUE);
	free(r);
	w->tag.file->mod = FALSE;
	if(w->tag.q0 > i)
		w->tag.q0 = i;
	if(w->tag.q1 > i)
		w->tag.q1 = i;
	textsetselect(&w->tag, w->tag.q0, w->tag.q1);
}

void
winsettag1(Window *w)
{
	int bar, dirty, i, j, k, n, ntagname, resize;
	Rune *new, *old, *r, *tagname;
	Image *b;
	uint q0, q1;
	Rectangle br;
	static Rune Ldelsnarf[] = { ' ', 'D', 'e', 'l', ' ',
		'S', 'n', 'a', 'r', 'f', 0 };
	static Rune Lundo[] = { ' ', 'U', 'n', 'd', 'o', 0 };
	static Rune Lredo[] = { ' ', 'R', 'e', 'd', 'o', 0 };
	static Rune Lget[] = { ' ', 'G', 'e', 't', 0 };
	static Rune Lput[] = { ' ', 'P', 'u', 't', 0 };
	static Rune Llook[] = { ' ', 'L', 'o', 'o', 'k', ' ', 0 };
	static Rune Lpipe[] = { ' ', '|', 0 };
	/* there are races that get us here with stuff in the tag cache, so we take extra care to sync it */
	if(w->tag.ncache!=0 || w->tag.file->mod)
		wincommit(w, &w->tag);	/* check file name; also guarantees we can modify tag contents */
	old = runemalloc(w->tag.file->b.nc+1);
	bufread(&w->tag.file->b, 0, old, w->tag.file->b.nc);
	old[w->tag.file->b.nc] = '\0';
	for(i=0; i<w->tag.file->b.nc; i++)
		if(old[i]==' ' || old[i]=='\t')
			break;
	
	/* make sure the file name is set correctly in the tag */
	ntagname = w->body.file->nname;
	tagname = runemalloc(ntagname);
	runemove(tagname, w->body.file->name, ntagname);
	abbrevenv(&tagname, (uint*)&ntagname);

	/*
	 * XXX Why is this here instead of letting the code
	 * down below do the work?
	 */
	if(runeeq(old, i, tagname, ntagname) == FALSE){
		q0 = w->tag.q0;
		q1 = w->tag.q1;
		textdelete(&w->tag, 0, i, TRUE);
		textinsert(&w->tag, 0, tagname, ntagname, TRUE);
		free(old);
		old = runemalloc(w->tag.file->b.nc+1);
		bufread(&w->tag.file->b, 0, old, w->tag.file->b.nc);
		old[w->tag.file->b.nc] = '\0';
		if(q0 >= i){
			/*
			 * XXX common case - typing at end of name
			 */
			w->tag.q0 = q0+ntagname-i;
			w->tag.q1 = q1+ntagname-i;
		}
	}
	
	/* compute the text for the whole tag, replacing current only if it differs */
	new = runemalloc(ntagname+100);
	i = 0;
	runemove(new+i, tagname, ntagname);
	i += ntagname;
	runemove(new+i, Ldelsnarf, 10);
	i += 10;
	if(w->filemenu){
		if(w->body.needundo || w->body.file->delta.nc>0 || w->body.ncache){
			runemove(new+i, Lundo, 5);
			i += 5;
		}
		if(w->body.file->epsilon.nc > 0){
			runemove(new+i, Lredo, 5);
			i += 5;
		}
		dirty = w->body.file->nname && (w->body.ncache || w->body.file->seq!=w->putseq);
		if(!w->isdir && dirty){
			runemove(new+i, Lput, 4);
			i += 4;
		}
	}
	if(w->isdir){
		runemove(new+i, Lget, 4);
		i += 4;
	}
	runemove(new+i, Lpipe, 2);
	i += 2;
	r = runestrchr(old, '|');
	if(r)
		k = r-old+1;
	else{
		k = w->tag.file->b.nc;
		if(w->body.file->seq == 0){
			runemove(new+i, Llook, 6);
			i += 6;
		}
	}
	new[i] = 0;
	if(runestrlen(new) != i)
		fprint(2, "s '%S' len not %d\n", new, i);
	assert(i==runestrlen(new));
	
	/* replace tag if the new one is different */
	resize = 0;
	if(runeeq(new, i, old, k) == FALSE){
		resize = 1;
		n = k;
		if(n > i)
			n = i;
		for(j=0; j<n; j++)
			if(old[j] != new[j])
				break;
		q0 = w->tag.q0;
		q1 = w->tag.q1;
		textdelete(&w->tag, j, k, TRUE);
		textinsert(&w->tag, j, new+j, i-j, TRUE);
		/* try to preserve user selection */
		r = runestrchr(old, '|');
		if(r){
			bar = r-old;
			if(q0 > bar){
				bar = (runestrchr(new, '|')-new)-bar;
				w->tag.q0 = q0+bar;
				w->tag.q1 = q1+bar;
			}
		}
	}
	free(tagname);
	free(old);
	free(new);
	w->tag.file->mod = FALSE;
	n = w->tag.file->b.nc+w->tag.ncache;
	if(w->tag.q0 > n)
		w->tag.q0 = n;
	if(w->tag.q1 > n)
		w->tag.q1 = n;
	textsetselect(&w->tag, w->tag.q0, w->tag.q1);
	b = button;
	if(!w->isdir && !w->isscratch && (w->body.file->mod || w->body.ncache))
		b = modbutton;
	br.min = w->tag.scrollr.min;
	br.max.x = br.min.x + Dx(b->r);
	br.max.y = br.min.y + Dy(b->r);
	draw(screen, br, b, nil, b->r.min);
	if(resize){
		w->tagsafe = 0;
		winresize(w, w->r, TRUE, TRUE);
	}
}

void
winsettag(Window *w)
{
	int i;
	File *f;
	Window *v;

	f = w->body.file;
	for(i=0; i<f->ntext; i++){
		v = f->text[i]->w;
		if(v->col->safe || v->body.fr.maxlines>0)
			winsettag1(v);
	}
}

void
wincommit(Window *w, Text *t)
{
	Rune *r;
	int i;
	File *f;

	textcommit(t, TRUE);
	f = t->file;
	if(f->ntext > 1)
		for(i=0; i<f->ntext; i++)
			textcommit(f->text[i], FALSE);	/* no-op for t */
	if(t->what == Body)
		return;
	r = runemalloc(w->tag.file->b.nc);
	bufread(&w->tag.file->b, 0, r, w->tag.file->b.nc);
	for(i=0; i<w->tag.file->b.nc; i++)
		if(r[i]==' ' || r[i]=='\t')
			break;
	expandenv(&r, (uint*)&i);
	if(runeeq(r, i, w->body.file->name, w->body.file->nname) == FALSE){
		seq++;
		filemark(w->body.file);
		w->body.file->mod = TRUE;
		w->dirty = TRUE;
		winsetname(w, r, i);
		winsettag(w);
	}
	free(r);
}

void
winaddincl(Window *w, Rune *r, int n)
{
	char *a;
	Dir *d;
	Runestr rs;

	a = runetobyte(r, n);
	d = dirstat(a);
	if(d == nil){
		if(a[0] == '/')
			goto Rescue;
		rs = dirname(&w->body, r, n);
		r = rs.r;
		n = rs.nr;
		free(a);
		a = runetobyte(r, n);
		d = dirstat(a);
		if(d == nil)
			goto Rescue;
		r = runerealloc(r, n+1);
		r[n] = 0;
	}
	free(a);
	if((d->qid.type&QTDIR) == 0){
		free(d);
		warning(nil, "%s: not a directory\n", a);
		free(r);
		return;
	}
	free(d);
	w->nincl++;
	w->incl = realloc(w->incl, w->nincl*sizeof(Rune*));
	memmove(w->incl+1, w->incl, (w->nincl-1)*sizeof(Rune*));
	w->incl[0] = runemalloc(n+1);
	runemove(w->incl[0], r, n);
	free(r);
	return;

Rescue:
	warning(nil, "%s: %r\n", a);
	free(r);
	free(a);
	return;
}

int
winclean(Window *w, int conservative)	/* as it stands, conservative is always TRUE */
{
	if(w->isscratch || w->isdir)	/* don't whine if it's a guide file, error window, etc. */
		return TRUE;
	if(!conservative && w->nopen[QWevent]>0)
		return TRUE;
	if(w->dirty){
		if(w->body.file->nname)
			warning(nil, "%.*S modified\n", w->body.file->nname, w->body.file->name);
		else{
			if(w->body.file->b.nc < 100)	/* don't whine if it's too small */
				return TRUE;
			warning(nil, "unnamed file modified\n");
		}
		w->dirty = FALSE;
		return FALSE;
	}
	return TRUE;
}

char*
winctlprint(Window *w, char *buf, int fonts)
{
	sprint(buf, "%11d %11d %11d %11d %11d ", w->id, w->tag.file->b.nc,
		w->body.file->b.nc, w->isdir, w->dirty);
	if(fonts)
		return smprint("%s%11d %q %11d ", buf, Dx(w->body.fr.r), 
			w->body.reffont->f->name, w->body.fr.maxtab);
	return buf;
}

void
winevent(Window *w, char *fmt, ...)
{
	int n;
	char *b;
	Xfid *x;
	va_list arg;

	if(w->nopen[QWevent] == 0)
		return;
	if(w->owner == 0)
		error("no window owner");
	va_start(arg, fmt);
	b = vsmprint(fmt, arg);
	va_end(arg);
	if(b == nil)
		error("vsmprint failed");
	n = strlen(b);
	w->events = realloc(w->events, w->nevents+1+n);
	w->events[w->nevents++] = w->owner;
	memmove(w->events+w->nevents, b, n);
	free(b);
	w->nevents += n;
	x = w->eventx;
	if(x){
		w->eventx = nil;
		sendp(x->c, nil);
	}
}

/*
 * This is here as a first stab at something. 
 * Run acme with the -'$' flag to enable it.
 * 
 * This code isn't quite right, in that it doesn't play well with 
 * the plumber and with other applications.  For example:
 *
 * If the window tag is $home/bin and you execute script, then acme runs
 * script in $home/bin, via the shell, so everything is fine.  If you do
 * execute "echo $home", it too goes to the shell so you see the value
 * of $home.  And if you right-click on script, then acme plumbs "script"
 * in the directory "/home/you/bin", so that works, but if you right-click
 * on "$home/bin/script", then what?  It's not correct to expand in acme
 * since what you're plumbing might be a price tag for all we know.  So the
 * plumber has to expand it, but in order to do that the plumber should
 * probably publish (and allow users to change) the set of variables it is
 * using in expansions.
 * 
 * Rob has suggested that a better solution is to make tag lines expand
 * automatically to fit the necessary number of lines.
 * 
 * The best solution, of course, is to use nice short path names, but this
 * is not always possible.
 */

int
expandenv(Rune **rp, uint *np)
{
	char *s, *t;
	Rune *p, *r, *q;
	uint n, pref;
	int nb, nr, slash;
	Runestr rs;

	if(!dodollarsigns)
		return FALSE;

	r = *rp;
	n = *np;
	if(n == 0 || r[0] != '$')
		return FALSE;
	for(p=r+1; *p && *p != '/'; p++)
		;
	pref = p-r;
	s = runetobyte(r+1, pref-1);
	if((t = getenv(s)) == nil){
		free(s);
		return FALSE;
	}

	q = runemalloc(utflen(t)+(n-pref));
	cvttorunes(t, strlen(t), q, &nb, &nr, nil);
	assert(nr==utflen(t));
	runemove(q+nr, p, n-pref);
	free(r);
	rs = runestr(q, nr+(n-pref));
	slash = rs.nr>0 && q[rs.nr-1] == '/';
	rs = cleanrname(rs);
	if(slash){
		rs.r = runerealloc(rs.r, rs.nr+1);
		rs.r[rs.nr++] = '/';
	}
	*rp = rs.r;
	*np = rs.nr;
	free(t);
	return TRUE;
}

extern char **environ;
Rune **runeenv;

/*
 * Weird sort order -- shorter names first, 
 * prefer lowercase over uppercase ($home over $HOME),
 * then do normal string comparison.
 */
int
runeenvcmp(const void *va, const void *vb)
{
	Rune *a, *b;
	int na, nb;

	a = *(Rune**)va;
	b = *(Rune**)vb;
	na = runestrchr(a, '=') - a;
	nb = runestrchr(b, '=') -  b;
	if(na < nb)
		return -1;
	if(nb > na)
		return 1;
	if(na == 0)
		return 0;
	if(islowerrune(a[0]) && !islowerrune(b[0]))
		return -1;
	if(islowerrune(b[0]) && !islowerrune(a[0]))
		return 1;
	return runestrncmp(a, b, na);
}

void
mkruneenv(void)
{
	int i, bad, n, nr;
	char *p;
	Rune *r, *q;

	n = 0;
	for(i=0; environ[i]; i++){
		/*
		 * Ignore some pollution.
		 */
		if(environ[i][0] == '_')
			continue;
		if(strncmp(environ[i], "PWD=", 4) == 0)
			continue;
		if(strncmp(environ[i], "OLDPWD=", 7) == 0)
			continue;

		/*
		 * Must be a rooted path.
		 */
		if((p = strchr(environ[i], '=')) == nil || *(p+1) != '/')
			continue;

		/*
		 * Only use the ones that we accept in look - all isfilec
		 */
		bad = 0;
		r = bytetorune(environ[i], &nr);
		for(q=r; *q != '='; q++)
			if(!isfilec(*q)){
				free(r);
				bad = 1;
				break;
			}
		if(!bad){
			runeenv = erealloc(runeenv, (n+1)*sizeof(runeenv[0]));
			runeenv[n++] = r;
		}
	}
	runeenv = erealloc(runeenv, (n+1)*sizeof(runeenv[0]));
	runeenv[n] = nil;
	qsort(runeenv, n, sizeof(runeenv[0]), runeenvcmp);
}

int
abbrevenv(Rune **rp, uint *np)
{
	int i, len, alen;
	Rune *r, *p, *q;
	uint n;

	if(!dodollarsigns)
		return FALSE;

	r = *rp;
	n = *np;
	if(n == 0 || r[0] != '/')
		return FALSE;

	if(runeenv == nil)
		mkruneenv();

	for(i=0; runeenv[i]; i++){
		p = runestrchr(runeenv[i], '=')+1;
		len = runestrlen(p);
		if(len <= n && (r[len]==0 || r[len]=='/') && runeeq(r, len, p, len)==TRUE){
			alen = (p-1) - runeenv[i];
			q = runemalloc(1+alen+n-len);
			q[0] = '$';
			runemove(q+1, runeenv[i], alen);
			runemove(q+1+alen, r+len, n-len);
			free(r);
			*rp = q;
			*np = 1+alen+n-len;
			return TRUE;
		}
	}
	return FALSE;
}

