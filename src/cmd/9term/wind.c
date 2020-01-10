#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
#include <9pclient.h>
#include <plumb.h>
#include <complete.h>
#include "dat.h"
#include "fns.h"

#define MOVEIT if(0)

enum
{
	HiWater	= 64000000,	/* max size of history */
	LoWater	= 400000,	/* min size of history after max'ed */
	MinWater	= 20000	/* room to leave available when reallocating */
};

static	int		topped;
static	int		id;

static	Image	*cols[NCOL];
static	Image	*grey;
static	Image	*darkgrey;
static	Cursor	*lastcursor;
static	Image	*titlecol;
static	Image	*lighttitlecol;
static	Image	*holdcol;
static	Image	*lightholdcol;
static	Image	*paleholdcol;

static int
wscale(Window *w, int n)
{
	if(w == nil || w->i == nil)
		return n;
	return scalesize(w->i->display, n);
}

Window*
wmk(Image *i, Mousectl *mc, Channel *ck, Channel *cctl, int scrolling)
{
	Window *w;
	Rectangle r;

	if(cols[0] == nil){
		/* greys are multiples of 0x11111100+0xFF, 14* being palest */
		grey = allocimage(display, Rect(0,0,1,1), CMAP8, 1, 0xEEEEEEFF);
		darkgrey = allocimage(display, Rect(0,0,1,1), CMAP8, 1, 0x666666FF);
		cols[BACK] = display->white;
		cols[HIGH] = allocimage(display, Rect(0,0,1,1), CMAP8, 1, 0xCCCCCCFF);
		cols[BORD] = allocimage(display, Rect(0,0,1,1), CMAP8, 1, 0x999999FF);
		cols[TEXT] = display->black;
		cols[HTEXT] = display->black;
		titlecol = allocimage(display, Rect(0,0,1,1), CMAP8, 1, DGreygreen);
		lighttitlecol = allocimage(display, Rect(0,0,1,1), CMAP8, 1, DPalegreygreen);
		holdcol = allocimage(display, Rect(0,0,1,1), CMAP8, 1, DMedblue);
		lightholdcol = allocimage(display, Rect(0,0,1,1), CMAP8, 1, DGreyblue);
		paleholdcol = allocimage(display, Rect(0,0,1,1), CMAP8, 1, DPalegreyblue);
	}
	w = emalloc(sizeof(Window));
	w->screenr = i->r;
	r = insetrect(i->r, wscale(w, Selborder)+wscale(w, 1));
	w->i = i;
	w->mc = *mc;
	w->ck = ck;
	w->cctl = cctl;
	w->cursorp = nil;
	w->conswrite = chancreate(sizeof(Conswritemesg), 0);
	w->consread =  chancreate(sizeof(Consreadmesg), 0);
	w->mouseread =  chancreate(sizeof(Mousereadmesg), 0);
	w->wctlread =  chancreate(sizeof(Consreadmesg), 0);
	w->scrollr = r;
	w->scrollr.max.x = r.min.x+wscale(w, Scrollwid);
	w->lastsr = ZR;
	r.min.x += wscale(w, Scrollwid)+wscale(w, Scrollgap);
	frinit(&w->f, r, font, i, cols);
	w->f.maxtab = maxtab*stringwidth(font, "0");
	w->topped = ++topped;
	w->id = ++id;
	w->notefd = -1;
	w->scrolling = scrolling;
	w->dir = estrdup(startdir);
	w->label = estrdup("<unnamed>");
	r = insetrect(w->i->r, wscale(w, Selborder));
	draw(w->i, r, cols[BACK], nil, w->f.entire.min);
	wborder(w, wscale(w, Selborder));
	wscrdraw(w);
	incref(&w->ref);	/* ref will be removed after mounting; avoids delete before ready to be deleted */
	return w;
}

void
wsetname(Window *w)
{
	int i, n;
	char err[ERRMAX];

	n = sprint(w->name, "window.%d.%d", w->id, w->namecount++);
	for(i='A'; i<='Z'; i++){
		if(nameimage(w->i, w->name, 1) > 0)
			return;
		errstr(err, sizeof err);
		if(strcmp(err, "image name in use") != 0)
			break;
		w->name[n] = i;
		w->name[n+1] = 0;
	}
	w->name[0] = 0;
	fprint(2, "rio: setname failed: %s\n", err);
}

void
wresize(Window *w, Image *i, int move)
{
	Rectangle r, or;

	or = w->i->r;
	if(move || (Dx(or)==Dx(i->r) && Dy(or)==Dy(i->r)))
		draw(i, i->r, w->i, nil, w->i->r.min);
	if(w->i != i){
fprint(2, "res %p %p\n", w->i, i);
		freeimage(w->i);
		w->i = i;
	}
/*	wsetname(w); */
/*XXX	w->mc.image = i; */
	r = insetrect(i->r, wscale(w, Selborder)+wscale(w, 1));
	w->scrollr = r;
	w->scrollr.max.x = r.min.x+wscale(w, Scrollwid);
	w->lastsr = ZR;
	r.min.x += wscale(w, Scrollwid)+wscale(w, Scrollgap);
	if(move)
		frsetrects(&w->f, r, w->i);
	else{
		frclear(&w->f, FALSE);
		frinit(&w->f, r, w->f.font, w->i, cols);
		wsetcols(w);
		w->f.maxtab = maxtab*stringwidth(w->f.font, "0");
		r = insetrect(w->i->r, wscale(w, Selborder));
		draw(w->i, r, cols[BACK], nil, w->f.entire.min);
		wfill(w);
		wsetselect(w, w->q0, w->q1);
		wscrdraw(w);
	}
	wborder(w, wscale(w, Selborder));
	w->topped = ++topped;
	w->resized = TRUE;
	w->mouse.counter++;
}

void
wrefresh(Window *w, Rectangle r)
{
	/* USED(r); */

	/* BUG: rectangle is ignored */
	if(w == input)
		wborder(w, wscale(w, Selborder));
	else
		wborder(w, wscale(w, Unselborder));
	if(w->mouseopen)
		return;
	draw(w->i, insetrect(w->i->r, Borderwidth), w->f.cols[BACK], nil, w->i->r.min);
	w->f.ticked = 0;
	if(w->f.p0 > 0)
		frdrawsel(&w->f, frptofchar(&w->f, 0), 0, w->f.p0, 0);
	if(w->f.p1 < w->f.nchars)
		frdrawsel(&w->f, frptofchar(&w->f, w->f.p1), w->f.p1, w->f.nchars, 0);
	frdrawsel(&w->f, frptofchar(&w->f, w->f.p0), w->f.p0, w->f.p1, 1);
	w->lastsr = ZR;
	wscrdraw(w);
}

int
wclose(Window *w)
{
	int i;

	i = decref(&w->ref);
	if(i > 0)
		return 0;
	if(i < 0)
		error("negative ref count");
	if(!w->deleted)
		wclosewin(w);
	wsendctlmesg(w, Exited, ZR, nil);
	return 1;
}


void
winctl(void *arg)
{
	Rune *rp, *bp, *up, *kbdr;
	uint qh;
	int nr, nb, c, wid, i, npart, initial, lastb, scrolling;
	char *s, *t, part[UTFmax];
	Window *w;
	Mousestate *mp, m;
	enum { WKey, WMouse, WMouseread, WCtl, WCwrite, WCread, WWread, NWALT };
	Alt alts[NWALT+1];
	Mousereadmesg mrm;
	Conswritemesg cwm;
	Consreadmesg crm;
	Consreadmesg cwrm;
	Stringpair pair;
	Wctlmesg wcm;
	char buf[4*12+1];

	w = arg;
	snprint(buf, sizeof buf, "winctl-id%d", w->id);
	threadsetname(buf);

	mrm.cm = chancreate(sizeof(Mouse), 0);
	cwm.cw = chancreate(sizeof(Stringpair), 0);
	crm.c1 = chancreate(sizeof(Stringpair), 0);
	crm.c2 = chancreate(sizeof(Stringpair), 0);
	cwrm.c1 = chancreate(sizeof(Stringpair), 0);
	cwrm.c2 = chancreate(sizeof(Stringpair), 0);


	alts[WKey].c = w->ck;
	alts[WKey].v = &kbdr;
	alts[WKey].op = CHANRCV;
	alts[WMouse].c = w->mc.c;
	alts[WMouse].v = &w->mc.m;
	alts[WMouse].op = CHANRCV;
	alts[WMouseread].c = w->mouseread;
	alts[WMouseread].v = &mrm;
	alts[WMouseread].op = CHANSND;
	alts[WCtl].c = w->cctl;
	alts[WCtl].v = &wcm;
	alts[WCtl].op = CHANRCV;
	alts[WCwrite].c = w->conswrite;
	alts[WCwrite].v = &cwm;
	alts[WCwrite].op = CHANSND;
	alts[WCread].c = w->consread;
	alts[WCread].v = &crm;
	alts[WCread].op = CHANSND;
	alts[WWread].c = w->wctlread;
	alts[WWread].v = &cwrm;
	alts[WWread].op = CHANSND;
	alts[NWALT].op = CHANEND;

	npart = 0;
	lastb = -1;
	for(;;){
		if(w->mouseopen && w->mouse.counter != w->mouse.lastcounter)
			alts[WMouseread].op = CHANSND;
		else
			alts[WMouseread].op = CHANNOP;
		//	if(!w->scrolling && !w->mouseopen && w->qh>w->org+w->f.nchars)
		//		alts[WCwrite].op = CHANNOP;
		//	else
		alts[WCwrite].op = CHANSND;
		if(w->deleted || !w->wctlready)
			alts[WWread].op = CHANNOP;
		else
			alts[WWread].op = CHANSND;
		/* this code depends on NL and EOT fitting in a single byte */
		/* kind of expensive for each loop; worth precomputing? */
		if(w->holding)
			alts[WCread].op = CHANNOP;
		else if(npart || (w->rawing && w->nraw>0))
			alts[WCread].op = CHANSND;
		else{
			alts[WCread].op = CHANNOP;
			for(i=w->qh; i<w->nr; i++){
				c = w->r[i];
				if(c=='\n' || c=='\004'){
					alts[WCread].op = CHANSND;
					break;
				}
			}
		}
		switch(alt(alts)){
		case WKey:
			for(i=0; kbdr[i]!=L'\0'; i++)
				wkeyctl(w, kbdr[i]);
/*			wkeyctl(w, r); */
/*			while(nbrecv(w->ck, &r)) */
/*				wkeyctl(w, r); */
			break;
		case WMouse:
			if(w->mouseopen) {
				w->mouse.counter++;

				/* queue click events */
				if(!w->mouse.qfull && lastb != w->mc.m.buttons) {	/* add to ring */
					mp = &w->mouse.queue[w->mouse.wi];
					if(++w->mouse.wi == nelem(w->mouse.queue))
						w->mouse.wi = 0;
					if(w->mouse.wi == w->mouse.ri)
						w->mouse.qfull = TRUE;
					mp->m = w->mc.m;
					mp->counter = w->mouse.counter;
					lastb = w->mc.m.buttons;
				}
			} else
				wmousectl(w);
			break;
		case WMouseread:
			/* send a queued event or, if the queue is empty, the current state */
			/* if the queue has filled, we discard all the events it contained. */
			/* the intent is to discard frantic clicking by the user during long latencies. */
			w->mouse.qfull = FALSE;
			if(w->mouse.wi != w->mouse.ri) {
				m = w->mouse.queue[w->mouse.ri];
				if(++w->mouse.ri == nelem(w->mouse.queue))
					w->mouse.ri = 0;
			} else {
				m.m = w->mc.m;
				m.counter = w->mouse.counter;
			}
			w->mouse.lastcounter = m.counter;
			send(mrm.cm, &m.m);
			continue;
		case WCtl:
			if(wctlmesg(w, wcm.type, wcm.r, wcm.image) == Exited){
				chanfree(crm.c1);
				chanfree(crm.c2);
				chanfree(mrm.cm);
				chanfree(cwm.cw);
				chanfree(cwrm.c1);
				chanfree(cwrm.c2);
				threadexits(nil);
			}
			continue;
		case WCwrite:
			recv(cwm.cw, &pair);
			rp = pair.s;
			nr = pair.ns;
			bp = rp;
			up = rp;
			initial = 0;
			for(i=0; i<nr; i++){
				switch(*bp){
				case 0:
					break;
				case '\b':
					if(up == rp)
						initial++;
					else
						--up;
					break;
				case '\r':
					while(i<nr-1 && *(bp+1) == '\r'){
						bp++;
						i++;
					}
					if(i<nr-1 && *(bp+1) != '\n'){
						while(up > rp && *(up-1) != '\n')
							up--;
						if(up == rp)
							initial = wbswidth(w, '\r');
					}else if(i == nr-1)
						*up++ = '\n';
					break;
				default:
					*up++ = *bp;
					break;
				}
				bp++;
			}
			if(initial){
				if(initial > w->qh)
					initial = w->qh;
				qh = w->qh - initial;
				wdelete(w, qh, qh+initial);
				w->qh = qh;
			}
			nr = up - rp;
			scrolling = w->scrolling && w->org <= w->qh && w->qh <= w->org + w->f.nchars;
			w->qh = winsert(w, rp, nr, w->qh)+nr;
			if(scrolling)
				wshow(w, w->qh);
			wsetselect(w, w->q0, w->q1);
			wscrdraw(w);
			free(rp);
			break;
		case WCread:
			recv(crm.c1, &pair);
			t = pair.s;
			nb = pair.ns;
			i = npart;
			npart = 0;
			if(i)
				memmove(t, part, i);
			while(i<nb && (w->qh<w->nr || w->nraw>0)){
				if(w->qh == w->nr){
					wid = runetochar(t+i, &w->raw[0]);
					w->nraw--;
					runemove(w->raw, w->raw+1, w->nraw);
				}else
					wid = runetochar(t+i, &w->r[w->qh++]);
				c = t[i];	/* knows break characters fit in a byte */
				i += wid;
				if(!w->rawing && (c == '\n' || c=='\004')){
				/*	if(c == '\004') */
				/*		i--; */
					break;
				}
			}
		/*	if(i==nb && w->qh<w->nr && w->r[w->qh]=='\004') */
		/*		w->qh++; */
			if(i > nb){
				npart = i-nb;
				memmove(part, t+nb, npart);
				i = nb;
			}
			pair.s = t;
			pair.ns = i;
			send(crm.c2, &pair);
			continue;
		case WWread:
			w->wctlready = 0;
			recv(cwrm.c1, &pair);
			if(w->deleted || w->i==nil)
				pair.ns = sprint(pair.s, "");
			else{
				s = "visible";
				for(i=0; i<nhidden; i++)
					if(hidden[i] == w){
						s = "hidden";
						break;
					}
				t = "notcurrent";
				if(w == input)
					t = "current";
				pair.ns = snprint(pair.s, pair.ns, "%11d %11d %11d %11d %s %s ",
					w->i->r.min.x, w->i->r.min.y, w->i->r.max.x, w->i->r.max.y, t, s);
			}
			send(cwrm.c2, &pair);
			continue;
		}
		if(!w->deleted)
			flushimage(display, 1);
	}
}

void
waddraw(Window *w, Rune *r, int nr)
{
	w->raw = runerealloc(w->raw, w->nraw+nr);
	runemove(w->raw+w->nraw, r, nr);
	w->nraw += nr;
}

/*
 * Need to do this in a separate proc because if process we're interrupting
 * is dying and trying to print tombstone, kernel is blocked holding p->debug lock.
 */
void
interruptproc(void *v)
{
	int *notefd;

	notefd = v;
	write(*notefd, "interrupt", 9);
	free(notefd);
}

int
windfilewidth(Window *w, uint q0, int oneelement)
{
	uint q;
	Rune r;

	q = q0;
	while(q > 0){
		r = w->r[q-1];
		if(r<=' ')
			break;
		if(oneelement && r=='/')
			break;
		--q;
	}
	return q0-q;
}

void
showcandidates(Window *w, Completion *c)
{
	int i;
	Fmt f;
	Rune *rp;
	uint nr, qline, q0;
	char *s;

	runefmtstrinit(&f);
	if (c->nmatch == 0)
		s = "[no matches in ";
	else
		s = "[";
	if(c->nfile > 32)
		fmtprint(&f, "%s%d files]\n", s, c->nfile);
	else{
		fmtprint(&f, "%s", s);
		for(i=0; i<c->nfile; i++){
			if(i > 0)
				fmtprint(&f, " ");
			fmtprint(&f, "%s", c->filename[i]);
		}
		fmtprint(&f, "]\n");
	}
	/* place text at beginning of line before host point */
	qline = w->qh;
	while(qline>0 && w->r[qline-1] != '\n')
		qline--;

	rp = runefmtstrflush(&f);
	nr = runestrlen(rp);

	q0 = w->q0;
	q0 += winsert(w, rp, runestrlen(rp), qline) - qline;
	free(rp);
	wsetselect(w, q0+nr, q0+nr);
}

Rune*
namecomplete(Window *w)
{
	int nstr, npath;
	Rune *rp, *path, *str;
	Completion *c;
	char *s, *dir, *root;

	/* control-f: filename completion; works back to white space or / */
	if(w->q0<w->nr && w->r[w->q0]>' ')	/* must be at end of word */
		return nil;
	nstr = windfilewidth(w, w->q0, TRUE);
	str = runemalloc(nstr);
	runemove(str, w->r+(w->q0-nstr), nstr);
	npath = windfilewidth(w, w->q0-nstr, FALSE);
	path = runemalloc(npath);
	runemove(path, w->r+(w->q0-nstr-npath), npath);
	rp = nil;

	/* is path rooted? if not, we need to make it relative to window path */
	if(npath>0 && path[0]=='/'){
		dir = malloc(UTFmax*npath+1);
		sprint(dir, "%.*S", npath, path);
	}else{
		if(strcmp(w->dir, "") == 0)
			root = ".";
		else
			root = w->dir;
		dir = malloc(strlen(root)+1+UTFmax*npath+1);
		sprint(dir, "%s/%.*S", root, npath, path);
	}
	dir = cleanname(dir);

	s = smprint("%.*S", nstr, str);
	c = complete(dir, s);
	free(s);
	if(c == nil)
		goto Return;

	if(!c->advance)
		showcandidates(w, c);

	if(c->advance)
		rp = runesmprint("%s", c->string);

  Return:
	freecompletion(c);
	free(dir);
	free(path);
	free(str);
	return rp;
}

void
wkeyctl(Window *w, Rune r)
{
	uint q0 ,q1;
	int n, nb, nr;
	Rune *rp;

	if(r == 0)
		return;
	if(w->deleted)
		return;
	w->rawing = rawon();
	/* navigation keys work only when mouse is not open */
	if(!w->mouseopen)
		switch(r){
		case Kdown:
			n = w->f.maxlines/3;
			goto case_Down;
		case Kscrollonedown:
			n = mousescrollsize(w->f.maxlines);
			if(n <= 0)
				n = 1;
			goto case_Down;
		case Kpgdown:
			n = 2*w->f.maxlines/3;
		case_Down:
			q0 = w->org+frcharofpt(&w->f, Pt(w->f.r.min.x, w->f.r.min.y+n*w->f.font->height));
			wsetorigin(w, q0, TRUE);
			return;
		case Kup:
			n = w->f.maxlines/3;
			goto case_Up;
		case Kscrolloneup:
			n = mousescrollsize(w->f.maxlines);
			if(n <= 0)
				n = 1;
			goto case_Up;
		case Kpgup:
			n = 2*w->f.maxlines/3;
		case_Up:
			q0 = wbacknl(w, w->org, n);
			wsetorigin(w, q0, TRUE);
			return;
		case Kleft:
			if(w->q0 > 0){
				q0 = w->q0-1;
				wsetselect(w, q0, q0);
				wshow(w, q0);
			}
			return;
		case Kright:
			if(w->q1 < w->nr){
				q1 = w->q1+1;
				wsetselect(w, q1, q1);
				wshow(w, q1);
			}
			return;
		case Khome:
			if(w->org > w->iq1) {
				q0 = wbacknl(w, w->iq1, 1);
				wsetorigin(w, q0, TRUE);
			} else
				wshow(w, 0);
			return;
		case Kend:
			if(w->iq1 > w->org+w->f.nchars) {
				q0 = wbacknl(w, w->iq1, 1);
				wsetorigin(w, q0, TRUE);
			} else
				wshow(w, w->nr);
			return;
		case 0x01:	/* ^A: beginning of line */
			if(w->q0==0 || w->q0==w->qh || w->r[w->q0-1]=='\n')
				return;
			nb = wbswidth(w, 0x15 /* ^U */);
			wsetselect(w, w->q0-nb, w->q0-nb);
			wshow(w, w->q0);
			return;
		case 0x05:	/* ^E: end of line */
			q0 = w->q0;
			while(q0 < w->nr && w->r[q0]!='\n')
				q0++;
			wsetselect(w, q0, q0);
			wshow(w, w->q0);
			return;
		}
	/*
	 * This if used to be below the if(w->rawing ...),
	 * but let's try putting it here.  This will allow ESC-processing
	 * to toggle hold mode even in remote SSH connections.
	 * The drawback is that vi-style processing gets harder.
	 * If you find yourself in some weird readline mode, good
	 * luck getting out without ESC.  Let's see who complains.
	 */
	if(r==0x1B || (w->holding && r==0x7F)){	/* toggle hold */
		if(w->holding)
			--w->holding;
		else
			w->holding++;
		wrepaint(w);
		if(r == 0x1B)
			return;
	}
	if(!w->holding && w->rawing && (w->q0==w->nr || w->mouseopen)){
		waddraw(w, &r, 1);
		return;
	}
	if(r == Kcmd+'x'){
		wsnarf(w);
		wcut(w);
		wscrdraw(w);
		return;
	}
	if(r == Kcmd+'c'){
		wsnarf(w);
		return;
	}
	if(r == Kcmd+'v'){
		riogetsnarf();
		wpaste(w);
		wscrdraw(w);
		return;
	}
	if(r != 0x7F){
		wsnarf(w);
		wcut(w);
	}
	switch(r){
	case 0x03:		/* maybe send interrupt */
		/* since ^C is so commonly used as interrupt, special case it */
		if (intrc() != 0x03)
			break;
		/* fall through */
	case 0x7F:		/* send interrupt */
		w->qh = w->nr;
		wshow(w, w->qh);
		winterrupt(w);
		w->iq1 = w->q0;
		return;
	case 0x06:	/* ^F: file name completion */
	case Kins:		/* Insert: file name completion */
		rp = namecomplete(w);
		if(rp == nil)
			return;
		nr = runestrlen(rp);
		q0 = w->q0;
		q0 = winsert(w, rp, nr, q0);
		wshow(w, q0+nr);
		w->iq1 = w->q0;
		free(rp);
		return;
	case 0x08:	/* ^H: erase character */
	case 0x15:	/* ^U: erase line */
	case 0x17:	/* ^W: erase word */
		if(w->q0==0 || w->q0==w->qh)
			return;
		nb = wbswidth(w, r);
		q1 = w->q0;
		q0 = q1-nb;
		if(q0 < w->org){
			q0 = w->org;
			nb = q1-q0;
		}
		if(nb > 0){
			wdelete(w, q0, q0+nb);
			wsetselect(w, q0, q0);
		}
		w->iq1 = w->q0;
		return;
	}
	/* otherwise ordinary character; just insert */
	q0 = w->q0;
	q0 = winsert(w, &r, 1, q0);
	wshow(w, q0+1);
	w->iq1 = w->q0;
}

void
wsetcols(Window *w)
{
	if(w->holding)
		if(w == input)
			w->f.cols[TEXT] = w->f.cols[HTEXT] = holdcol;
		else
			w->f.cols[TEXT] = w->f.cols[HTEXT] = lightholdcol;
	else
		if(w == input)
			w->f.cols[TEXT] = w->f.cols[HTEXT] = display->black;
		else
			w->f.cols[TEXT] = w->f.cols[HTEXT] = darkgrey;
}

void
wrepaint(Window *w)
{
	wsetcols(w);
	if(!w->mouseopen){
		frredraw(&w->f);
	}
	if(w == input){
		wborder(w, wscale(w, Selborder));
		wsetcursor(w, 0);
	}else
		wborder(w, wscale(w, Unselborder));
}

int
wbswidth(Window *w, Rune c)
{
	uint q, eq, stop;
	Rune r;
	int skipping;

	/* there is known to be at least one character to erase */
	if(c == 0x08)	/* ^H: erase character */
		return 1;
	q = w->q0;
	stop = 0;
	if(q > w->qh)
		stop = w->qh;
	skipping = TRUE;
	while(q > stop){
		r = w->r[q-1];
		if(r == '\n'){		/* eat at most one more character */
			if(q == w->q0 && c != '\r')	/* eat the newline */
				--q;
			break;
		}
		if(c == 0x17){
			eq = isalnum(r);
			if(eq && skipping)	/* found one; stop skipping */
				skipping = FALSE;
			else if(!eq && !skipping)
				break;
		}
		--q;
	}
	return w->q0-q;
}

void
wsnarf(Window *w)
{
	if(w->q1 == w->q0)
		return;
	nsnarf = w->q1-w->q0;
	snarf = runerealloc(snarf, nsnarf);
	snarfversion++;	/* maybe modified by parent */
	runemove(snarf, w->r+w->q0, nsnarf);
	rioputsnarf();
}

void
wcut(Window *w)
{
	if(w->q1 == w->q0)
		return;
	wdelete(w, w->q0, w->q1);
	wsetselect(w, w->q0, w->q0);
}

void
wpaste(Window *w)
{
	uint q0;

	if(nsnarf == 0)
		return;
	wcut(w);
	q0 = w->q0;
	if(w->rawing && !w->holding && q0==w->nr){
		waddraw(w, snarf, nsnarf);
		wsetselect(w, q0, q0);
	}else{
		q0 = winsert(w, snarf, nsnarf, w->q0);
		wsetselect(w, q0, q0+nsnarf);
	}
}

void
wplumb(Window *w)
{
	Plumbmsg *m;
	static CFid *fd;
	char buf[32];
	uint p0, p1;
	Cursor *c;

	if(fd == nil)
		fd = plumbopenfid("send", OWRITE);
	if(fd == nil)
		return;
	m = emalloc(sizeof(Plumbmsg));
	m->src = estrdup("rio");
	m->dst = nil;
	m->wdir = estrdup(w->dir);
	m->type = estrdup("text");
	p0 = w->q0;
	p1 = w->q1;
	if(w->q1 > w->q0)
		m->attr = nil;
	else{
		while(p0>0 && w->r[p0-1]!=' ' && w->r[p0-1]!='\t' && w->r[p0-1]!='\n')
			p0--;
		while(p1<w->nr && w->r[p1]!=' ' && w->r[p1]!='\t' && w->r[p1]!='\n')
			p1++;
		sprint(buf, "click=%d", w->q0-p0);
		m->attr = plumbunpackattr(buf);
	}
	if(p1-p0 > messagesize-1024){
		plumbfree(m);
		return;	/* too large for 9P */
	}
	m->data = runetobyte(w->r+p0, p1-p0, &m->ndata);
	if(plumbsendtofid(fd, m) < 0){
		c = lastcursor;
		riosetcursor(&query, 1);
		sleep(300);
		riosetcursor(c, 1);
	}
	plumbfree(m);
}

int
winborder(Window *w, Point xy)
{
	return ptinrect(xy, w->screenr) && !ptinrect(xy, insetrect(w->screenr, wscale(w, Selborder)));
}

void
wlook(Window *w)
{
	int i, n, e;

	i = w->q1;
	n = i - w->q0;
	e = w->nr - n;
	if(n <= 0 || e < n)
		return;

	if(i > e)
		i = 0;

	while(runestrncmp(w->r+w->q0, w->r+i, n) != 0){
		if(i < e)
			i++;
		else
			i = 0;
	}

	wsetselect(w, i, i+n);
	wshow(w, i);
}

void
wmousectl(Window *w)
{
	int but;

	if(w->mc.m.buttons == 1)
		but = 1;
	else if(w->mc.m.buttons == 2)
		but = 2;
	else if(w->mc.m.buttons == 4)
		but = 3;
	else{
		if(w->mc.m.buttons == 8)
			wkeyctl(w, Kscrolloneup);
		if(w->mc.m.buttons == 16)
			wkeyctl(w, Kscrollonedown);
		return;
	}

	incref(&w->ref);		/* hold up window while we track */
	if(w->deleted)
		goto Return;
	if(ptinrect(w->mc.m.xy, w->scrollr)){
		if(but)
			wscroll(w, but);
		goto Return;
	}
	if(but == 1)
		wselect(w);
	/* else all is handled by main process */
   Return:
	wclose(w);
}

void
wdelete(Window *w, uint q0, uint q1)
{
	uint n, p0, p1;

	n = q1-q0;
	if(n == 0)
		return;
	runemove(w->r+q0, w->r+q1, w->nr-q1);
	w->nr -= n;
	if(q0 < w->iq1)
		w->iq1 -= min(n, w->iq1-q0);
	if(q0 < w->q0)
		w->q0 -= min(n, w->q0-q0);
	if(q0 < w->q1)
		w->q1 -= min(n, w->q1-q0);
	if(q1 < w->qh)
		w->qh -= n;
	else if(q0 < w->qh)
		w->qh = q0;
	if(q1 <= w->org)
		w->org -= n;
	else if(q0 < w->org+w->f.nchars){
		p1 = q1 - w->org;
		if(p1 > w->f.nchars)
			p1 = w->f.nchars;
		if(q0 < w->org){
			w->org = q0;
			p0 = 0;
		}else
			p0 = q0 - w->org;
		frdelete(&w->f, p0, p1);
		wfill(w);
	}
}


static Window	*clickwin;
static uint	clickmsec;
static Window	*selectwin;
static uint	selectq;

/*
 * called from frame library
 */
void
framescroll(Frame *f, int dl)
{
	if(f != &selectwin->f)
		error("frameselect not right frame");
	wframescroll(selectwin, dl);
}

void
wframescroll(Window *w, int dl)
{
	uint q0;

	if(dl == 0){
		wscrsleep(w, 100);
		return;
	}
	if(dl < 0){
		q0 = wbacknl(w, w->org, -dl);
		if(selectq > w->org+w->f.p0)
			wsetselect(w, w->org+w->f.p0, selectq);
		else
			wsetselect(w, selectq, w->org+w->f.p0);
	}else{
		if(w->org+w->f.nchars == w->nr)
			return;
		q0 = w->org+frcharofpt(&w->f, Pt(w->f.r.min.x, w->f.r.min.y+dl*w->f.font->height));
		if(selectq >= w->org+w->f.p1)
			wsetselect(w, w->org+w->f.p1, selectq);
		else
			wsetselect(w, selectq, w->org+w->f.p1);
	}
	wsetorigin(w, q0, TRUE);
}

void
wselect(Window *w)
{
	uint q0, q1;
	int b, x, y, first;

	first = 1;
	selectwin = w;
	/*
	 * Double-click immediately if it might make sense.
	 */
	b = w->mc.m.buttons;
	q0 = w->q0;
	q1 = w->q1;
	selectq = w->org+frcharofpt(&w->f, w->mc.m.xy);
	if(clickwin==w && w->mc.m.msec-clickmsec<500)
	if(q0==q1 && selectq==w->q0){
		wdoubleclick(w, &q0, &q1);
		wsetselect(w, q0, q1);
		flushimage(display, 1);
		x = w->mc.m.xy.x;
		y = w->mc.m.xy.y;
		/* stay here until something interesting happens */
		do
			readmouse(&w->mc);
		while(w->mc.m.buttons==b && abs(w->mc.m.xy.x-x)<3 && abs(w->mc.m.xy.y-y)<3);
		w->mc.m.xy.x = x;	/* in case we're calling frselect */
		w->mc.m.xy.y = y;
		q0 = w->q0;	/* may have changed */
		q1 = w->q1;
		selectq = q0;
	}
	if(w->mc.m.buttons == b){
		w->f.scroll = framescroll;
		frselect(&w->f, &w->mc);
		/* horrible botch: while asleep, may have lost selection altogether */
		if(selectq > w->nr)
			selectq = w->org + w->f.p0;
		w->f.scroll = nil;
		if(selectq < w->org)
			q0 = selectq;
		else
			q0 = w->org + w->f.p0;
		if(selectq > w->org+w->f.nchars)
			q1 = selectq;
		else
			q1 = w->org+w->f.p1;
	}
	if(q0 == q1){
		if(q0==w->q0 && clickwin==w && w->mc.m.msec-clickmsec<500){
			wdoubleclick(w, &q0, &q1);
			clickwin = nil;
		}else{
			clickwin = w;
			clickmsec = w->mc.m.msec;
		}
	}else
		clickwin = nil;
	wsetselect(w, q0, q1);
	flushimage(display, 1);
	while(w->mc.m.buttons){
		w->mc.m.msec = 0;
		b = w->mc.m.buttons;
		if(b & 6){
			if(b & 2){
				wsnarf(w);
				wcut(w);
			}else{
				if(first){
					first = 0;
					riogetsnarf();
				}
				wpaste(w);
			}
		}
		wscrdraw(w);
		flushimage(display, 1);
		while(w->mc.m.buttons == b)
			readmouse(&w->mc);
		clickwin = nil;
	}
}

void
wsendctlmesg(Window *w, int type, Rectangle r, Image *image)
{
	Wctlmesg wcm;

	wcm.type = type;
	wcm.r = r;
	wcm.image = image;
	send(w->cctl, &wcm);
}

int
wctlmesg(Window *w, int m, Rectangle r, Image *i)
{
	char buf[64];

	switch(m){
	default:
		error("unknown control message");
		break;
	case Wakeup:
		break;
	case Moved:
	case Reshaped:
		if(w->deleted){
			freeimage(i);
			break;
		}
		w->screenr = r;
		strcpy(buf, w->name);
		wresize(w, i, m==Moved);
		w->wctlready = 1;
		if(Dx(r) > 0){
			if(w != input)
				wcurrent(w);
		}else if(w == input)
			wcurrent(nil);
		flushimage(display, 1);
		break;
	case Refresh:
		if(w->deleted || Dx(w->screenr)<=0 || !rectclip(&r, w->i->r))
			break;
		if(!w->mouseopen)
			wrefresh(w, r);
		flushimage(display, 1);
		break;
	case Movemouse:
		if(sweeping || !ptinrect(r.min, w->i->r))
			break;
		wmovemouse(w, r.min);
	case Rawon:
		break;
	case Rawoff:
		if(w->deleted)
			break;
		while(w->nraw > 0){
			wkeyctl(w, w->raw[0]);
			--w->nraw;
			runemove(w->raw, w->raw+1, w->nraw);
		}
		break;
	case Holdon:
	case Holdoff:
		if(w->deleted)
			break;
		wrepaint(w);
		flushimage(display, 1);
		break;
	case Deleted:
		if(w->deleted)
			break;
		write(w->notefd, "hangup", 6);
		wclosewin(w);
		break;
	case Exited:
		frclear(&w->f, TRUE);
		close(w->notefd);
		chanfree(w->mc.c);
		chanfree(w->ck);
		chanfree(w->cctl);
		chanfree(w->conswrite);
		chanfree(w->consread);
		chanfree(w->mouseread);
		chanfree(w->wctlread);
		free(w->raw);
		free(w->r);
		free(w->dir);
		free(w->label);
		free(w);
		break;
	}
	return m;
}

/*
 * Convert back to physical coordinates
 */
void
wmovemouse(Window *w, Point p)
{
	p.x += w->screenr.min.x-w->i->r.min.x;
	p.y += w->screenr.min.y-w->i->r.min.y;
	moveto(mousectl, p);
}

void
wcurrent(Window *w)
{
	Window *oi;

	if(wkeyboard!=nil && w==wkeyboard)
		return;
	oi = input;
	input = w;
	if(oi!=w && oi!=nil)
		wrepaint(oi);
	if(w !=nil){
		wrepaint(w);
		wsetcursor(w, 0);
	}
	if(w != oi){
		if(oi){
			oi->wctlready = 1;
			wsendctlmesg(oi, Wakeup, ZR, nil);
		}
		if(w){
			w->wctlready = 1;
			wsendctlmesg(w, Wakeup, ZR, nil);
		}
	}
}

void
wsetcursor(Window *w, int force)
{
	Cursor *p;

	if(w==nil || /*w!=input || */ w->i==nil || Dx(w->screenr)<=0)
		p = nil;
	else if(wpointto(mouse->xy) == w){
		p = w->cursorp;
		if(p==nil && w->holding)
			p = &whitearrow;
	}else
		p = nil;
	if(!menuing)
		riosetcursor(p, force && !menuing);
}

void
riosetcursor(Cursor *p, int force)
{
	if(!force && p==lastcursor)
		return;
	setcursor(mousectl, p);
	lastcursor = p;
}

Window*
wtop(Point pt)
{
	Window *w;

	w = wpointto(pt);
	if(w){
		if(w->topped == topped)
			return nil;
		topwindow(w->i);
		wcurrent(w);
		flushimage(display, 1);
		w->topped = ++topped;
	}
	return w;
}

void
wtopme(Window *w)
{
	if(w!=nil && w->i!=nil && !w->deleted && w->topped!=topped){
		topwindow(w->i);
		flushimage(display, 1);
		w->topped = ++ topped;
	}
}

void
wbottomme(Window *w)
{
	if(w!=nil && w->i!=nil && !w->deleted){
		bottomwindow(w->i);
		flushimage(display, 1);
		w->topped = 0;
	}
}

Window*
wlookid(int id)
{
	int i;

	for(i=0; i<nwindow; i++)
		if(window[i]->id == id)
			return window[i];
	return nil;
}

void
wclosewin(Window *w)
{
	Rectangle r;
	int i;

	w->deleted = TRUE;
	if(w == input){
		input = nil;
		wsetcursor(w, 0);
	}
	if(w == wkeyboard)
		wkeyboard = nil;
	for(i=0; i<nhidden; i++)
		if(hidden[i] == w){
			--nhidden;
			memmove(hidden+i, hidden+i+1, (nhidden-i)*sizeof(hidden[0]));
			break;
		}
	for(i=0; i<nwindow; i++)
		if(window[i] == w){
			--nwindow;
			memmove(window+i, window+i+1, (nwindow-i)*sizeof(Window*));
			w->deleted = TRUE;
			r = w->i->r;
			/* move it off-screen to hide it, in case client is slow in letting it go */
			MOVEIT originwindow(w->i, r.min, view->r.max);
			freeimage(w->i);
			w->i = nil;
			return;
		}
	error("unknown window in closewin");
}

void
wsetpid(Window *w, int pid, int dolabel)
{
	char buf[128];

	w->pid = pid;
	if(dolabel){
		sprint(buf, "rc %d", pid);
		free(w->label);
		w->label = estrdup(buf);
		drawsetlabel(w->label);
	}
}

static Rune left1[] =  {
	'{', '[', '(', '<', 0xAB,
	0x207d, 0x2329, 0x27e6, 0x27e8, 0x27ea,
	0xfe59, 0xfe5b, 0xfe5d, 0xff08, 0xff3b, 0xff5b,
	0
};
static Rune right1[] = {
	'}', ']', ')', '>', 0xBB,
	0x207e, 0x232a, 0x27e7, 0x27e9, 0x27eb,
	0xfe5a, 0xfe5c, 0xfe5e, 0xff09, 0xff3d, 0xff5d,
	0
};
static Rune left2[] =  { '\n', 0 };
static Rune left3[] =  { '\'', '"', '`', 0 };

Rune *left[] = {
	left1,
	left2,
	left3,
	nil
};
Rune *right[] = {
	right1,
	left2,
	left3,
	nil
};

void
wdoubleclick(Window *w, uint *q0, uint *q1)
{
	int c, i;
	Rune *r, *l, *p;
	uint q;

	for(i=0; left[i]!=nil; i++){
		q = *q0;
		l = left[i];
		r = right[i];
		/* try matching character to left, looking right */
		if(q == 0)
			c = '\n';
		else
			c = w->r[q-1];
		p = strrune(l, c);
		if(p != nil){
			if(wclickmatch(w, c, r[p-l], 1, &q))
				*q1 = q-(c!='\n');
			return;
		}
		/* try matching character to right, looking left */
		if(q == w->nr)
			c = '\n';
		else
			c = w->r[q];
		p = strrune(r, c);
		if(p != nil){
			if(wclickmatch(w, c, l[p-r], -1, &q)){
				*q1 = *q0+(*q0<w->nr && c=='\n');
				*q0 = q;
				if(c!='\n' || q!=0 || w->r[0]=='\n')
					(*q0)++;
			}
			return;
		}
	}
	/* try filling out word to right */
	while(*q1<w->nr && isalnum(w->r[*q1]))
		(*q1)++;
	/* try filling out word to left */
	while(*q0>0 && isalnum(w->r[*q0-1]))
		(*q0)--;
}

int
wclickmatch(Window *w, int cl, int cr, int dir, uint *q)
{
	Rune c;
	int nest;

	nest = 1;
	for(;;){
		if(dir > 0){
			if(*q == w->nr)
				break;
			c = w->r[*q];
			(*q)++;
		}else{
			if(*q == 0)
				break;
			(*q)--;
			c = w->r[*q];
		}
		if(c == cr){
			if(--nest==0)
				return 1;
		}else if(c == cl)
			nest++;
	}
	return cl=='\n' && nest==1;
}


uint
wbacknl(Window *w, uint p, uint n)
{
	int i, j;

	/* look for start of this line if n==0 */
	if(n==0 && p>0 && w->r[p-1]!='\n')
		n = 1;
	i = n;
	while(i-->0 && p>0){
		--p;	/* it's at a newline now; back over it */
		if(p == 0)
			break;
		/* at 128 chars, call it a line anyway */
		for(j=128; --j>0 && p>0; p--)
			if(w->r[p-1]=='\n')
				break;
	}
	return p;
}

void
wshow(Window *w, uint q0)
{
	int qe;
	int nl;
	uint q;

	qe = w->org+w->f.nchars;
	if(w->org<=q0 && (q0<qe || (q0==qe && qe==w->nr)))
		wscrdraw(w);
	else{
		nl = 4*w->f.maxlines/5;
		q = wbacknl(w, q0, nl);
		/* avoid going backwards if trying to go forwards - long lines! */
		if(!(q0>w->org && q<w->org))
			wsetorigin(w, q, TRUE);
		while(q0 > w->org+w->f.nchars)
			wsetorigin(w, w->org+1, FALSE);
	}
}

void
wsetorigin(Window *w, uint org, int exact)
{
	int i, a, fixup;
	Rune *r;
	uint n;

	if(org>0 && !exact){
		/* org is an estimate of the char posn; find a newline */
		/* don't try harder than 256 chars */
		for(i=0; i<256 && org<w->nr; i++){
			if(w->r[org] == '\n'){
				org++;
				break;
			}
			org++;
		}
	}
	a = org-w->org;
	fixup = 0;
	if(a>=0 && a<w->f.nchars){
		frdelete(&w->f, 0, a);
		fixup = 1;	/* frdelete can leave end of last line in wrong selection mode; it doesn't know what follows */
	}else if(a<0 && -a<w->f.nchars){
		n = w->org - org;
		r = runemalloc(n);
		runemove(r, w->r+org, n);
		frinsert(&w->f, r, r+n, 0);
		free(r);
	}else
		frdelete(&w->f, 0, w->f.nchars);
	w->org = org;
	wfill(w);
	wscrdraw(w);
	wsetselect(w, w->q0, w->q1);
	if(fixup && w->f.p1 > w->f.p0)
		frdrawsel(&w->f, frptofchar(&w->f, w->f.p1-1), w->f.p1-1, w->f.p1, 1);
}

void
wsetselect(Window *w, uint q0, uint q1)
{
	int p0, p1;

	/* w->f.p0 and w->f.p1 are always right; w->q0 and w->q1 may be off */
	w->q0 = q0;
	w->q1 = q1;
	/* compute desired p0,p1 from q0,q1 */
	p0 = q0-w->org;
	p1 = q1-w->org;
	if(p0 < 0)
		p0 = 0;
	if(p1 < 0)
		p1 = 0;
	if(p0 > w->f.nchars)
		p0 = w->f.nchars;
	if(p1 > w->f.nchars)
		p1 = w->f.nchars;
	if(p0==w->f.p0 && p1==w->f.p1)
		return;
	/* screen disagrees with desired selection */
	if(w->f.p1<=p0 || p1<=w->f.p0 || p0==p1 || w->f.p1==w->f.p0){
		/* no overlap or too easy to bother trying */
		frdrawsel(&w->f, frptofchar(&w->f, w->f.p0), w->f.p0, w->f.p1, 0);
		frdrawsel(&w->f, frptofchar(&w->f, p0), p0, p1, 1);
		goto Return;
	}
	/* overlap; avoid unnecessary painting */
	if(p0 < w->f.p0){
		/* extend selection backwards */
		frdrawsel(&w->f, frptofchar(&w->f, p0), p0, w->f.p0, 1);
	}else if(p0 > w->f.p0){
		/* trim first part of selection */
		frdrawsel(&w->f, frptofchar(&w->f, w->f.p0), w->f.p0, p0, 0);
	}
	if(p1 > w->f.p1){
		/* extend selection forwards */
		frdrawsel(&w->f, frptofchar(&w->f, w->f.p1), w->f.p1, p1, 1);
	}else if(p1 < w->f.p1){
		/* trim last part of selection */
		frdrawsel(&w->f, frptofchar(&w->f, p1), p1, w->f.p1, 0);
	}

    Return:
	w->f.p0 = p0;
	w->f.p1 = p1;
}

uint
winsert(Window *w, Rune *r, int n, uint q0)
{
	uint m;

	if(n == 0)
		return q0;
	if(w->nr+n>HiWater && q0>=w->org && q0>=w->qh){
		m = min(HiWater-LoWater, min(w->org, w->qh));
		w->org -= m;
		w->qh -= m;
		if(w->q0 > m)
			w->q0 -= m;
		else
			w->q0 = 0;
		if(w->q1 > m)
			w->q1 -= m;
		else
			w->q1 = 0;
		w->nr -= m;
		runemove(w->r, w->r+m, w->nr);
		q0 -= m;
	}
	if(w->nr+n > w->maxr){
		/*
		 * Minimize realloc breakage:
		 *	Allocate at least MinWater
		 * 	Double allocation size each time
		 *	But don't go much above HiWater
		 */
		m = max(min(2*(w->nr+n), HiWater), w->nr+n)+MinWater;
		if(m > HiWater)
			m = max(HiWater+MinWater, w->nr+n);
		if(m > w->maxr){
			w->r = runerealloc(w->r, m);
			w->maxr = m;
		}
	}
	runemove(w->r+q0+n, w->r+q0, w->nr-q0);
	runemove(w->r+q0, r, n);
	w->nr += n;
	/* if output touches, advance selection, not qh; works best for keyboard and output */
	if(q0 <= w->q1)
		w->q1 += n;
	if(q0 <= w->q0)
		w->q0 += n;
	if(q0 < w->qh)
		w->qh += n;
	if(q0 < w->iq1)
		w->iq1 += n;
	if(q0 < w->org)
		w->org += n;
	else if(q0 <= w->org+w->f.nchars)
		frinsert(&w->f, r, r+n, q0-w->org);
	return q0;
}

void
wfill(Window *w)
{
	Rune *rp;
	int i, n, m, nl;

	if(w->f.lastlinefull)
		return;
	rp = malloc(messagesize);
	do{
		n = w->nr-(w->org+w->f.nchars);
		if(n == 0)
			break;
		if(n > 2000)	/* educated guess at reasonable amount */
			n = 2000;
		runemove(rp, w->r+(w->org+w->f.nchars), n);
		/*
		 * it's expensive to frinsert more than we need, so
		 * count newlines.
		 */
		nl = w->f.maxlines-w->f.nlines;
		m = 0;
		for(i=0; i<n; ){
			if(rp[i++] == '\n'){
				m++;
				if(m >= nl)
					break;
			}
		}
		frinsert(&w->f, rp, rp+i, w->f.nchars);
	}while(w->f.lastlinefull == FALSE);
	free(rp);
}

char*
wcontents(Window *w, int *ip)
{
	return runetobyte(w->r, w->nr, ip);
}
