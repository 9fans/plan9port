#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <cursor.h>
#include <keyboard.h>
#include <frame.h>
#include "flayer.h"
#include "samterm.h"

#define	DELTA	10

static Flayer	**llist;	/* front to back */
static int	nllist;
static int	nlalloc;
static Rectangle lDrect;

Vis		visibility(Flayer *);
void		newvisibilities(int);
void		llinsert(Flayer*);
void		lldelete(Flayer*);

Image	*maincols[NCOL];
Image	*cmdcols[NCOL];

void
flstart(Rectangle r)
{
	lDrect = r;

	/* Main text is yellowish */
	maincols[BACK] = allocimagemix(display, DPaleyellow, DWhite);
	maincols[HIGH] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DDarkyellow);
	maincols[BORD] = allocimage(display, Rect(0,0,2,2), screen->chan, 1, DYellowgreen);
	maincols[TEXT] = display->black;
	maincols[HTEXT] = display->black;

	/* Command text is blueish */
	cmdcols[BACK] = allocimagemix(display, DPalebluegreen, DWhite);
	cmdcols[HIGH] = allocimage(display, Rect(0,0,1,1), screen->chan, 1, DPalegreygreen);
	cmdcols[BORD] = allocimage(display, Rect(0,0,2,2), screen->chan, 1, DPurpleblue);
	cmdcols[TEXT] = display->black;
	cmdcols[HTEXT] = display->black;
}

void
flnew(Flayer *l, Rune *(*fn)(Flayer*, long, ulong*), int u0, void *u1)
{
	if(nllist == nlalloc){
		nlalloc += DELTA;
		llist = realloc(llist, nlalloc*sizeof(Flayer**));
		if(llist == 0)
			panic("flnew");
	}
	l->textfn = fn;
	l->user0 = u0;
	l->user1 = u1;
	l->lastsr = ZR;
	llinsert(l);
}

Rectangle
flrect(Flayer *l, Rectangle r)
{
	rectclip(&r, lDrect);
	l->entire = r;
	l->scroll = insetrect(r, FLMARGIN(l));
	r.min.x =
	 l->scroll.max.x = r.min.x+FLMARGIN(l)+FLSCROLLWID(l)+(FLGAP(l)-FLMARGIN(l));
	return r;
}

void
flinit(Flayer *l, Rectangle r, Font *ft, Image **cols)
{
	lldelete(l);
	llinsert(l);
	l->visible = All;
	l->origin = l->p0 = l->p1 = 0;
	l->f.display = display; // for FLMARGIN
	frinit(&l->f, insetrect(flrect(l, r), FLMARGIN(l)), ft, screen, cols);
	l->f.maxtab = maxtab*stringwidth(ft, "0");
	newvisibilities(1);
	draw(screen, l->entire, l->f.cols[BACK], nil, ZP);
	scrdraw(l, 0L);
	flborder(l, 0);
}

void
flclose(Flayer *l)
{
	if(l->visible == All)
		draw(screen, l->entire, display->white, nil, ZP);
	else if(l->visible == Some){
		if(l->f.b == 0)
			l->f.b = allocimage(display, l->entire, screen->chan, 0, DNofill);
		if(l->f.b){
			draw(l->f.b, l->entire, display->white, nil, ZP);
			flrefresh(l, l->entire, 0);
		}
	}
	frclear(&l->f, 1);
	lldelete(l);
	if(l->f.b && l->visible!=All)
		freeimage(l->f.b);
	l->textfn = 0;
	newvisibilities(1);
}

void
flborder(Flayer *l, int wide)
{
	if(flprepare(l)){
		border(l->f.b, l->entire, FLMARGIN(l), l->f.cols[BACK], ZP);
		border(l->f.b, l->entire, wide? FLMARGIN(l) : 1, l->f.cols[BORD], ZP);
		if(l->visible==Some)
			flrefresh(l, l->entire, 0);
	}
}

Flayer *
flwhich(Point p)
{
	int i;

	if(p.x==0 && p.y==0)
		return nllist? llist[0] : 0;
	for(i=0; i<nllist; i++)
		if(ptinrect(p, llist[i]->entire))
			return llist[i];
	return 0;
}

void
flupfront(Flayer *l)
{
	int v = l->visible;

	lldelete(l);
	llinsert(l);
	if(v!=All)
		newvisibilities(0);
}

void
newvisibilities(int redraw)
	/* if redraw false, we know it's a flupfront, and needn't
	 * redraw anyone becoming partially covered */
{
	int i;
	Vis ov;
	Flayer *l;

	for(i = 0; i<nllist; i++){
		l = llist[i];
		l->lastsr = ZR;	/* make sure scroll bar gets redrawn */
		ov = l->visible;
		l->visible = visibility(l);
#define	V(a, b)	(((a)<<2)|((b)))
		switch(V(ov, l->visible)){
		case V(Some, None):
			if(l->f.b)
				freeimage(l->f.b);
		case V(All, None):
		case V(All, Some):
			l->f.b = 0;
			frclear(&l->f, 0);
			break;

		case V(Some, Some):
		case V(None, Some):
			if(ov == None || (l->f.b==0 && redraw))
				flprepare(l);
			if(l->f.b && redraw){
				flrefresh(l, l->entire, 0);
				freeimage(l->f.b);
				l->f.b = 0;
				frclear(&l->f, 0);
			}
		case V(None, None):
		case V(All, All):
			break;

		case V(Some, All):
			if(l->f.b){
				draw(screen, l->entire, l->f.b, nil, l->entire.min);
				freeimage(l->f.b);
				l->f.b = screen;
				break;
			}
		case V(None, All):
			flprepare(l);
			break;
		}
		if(ov==None && l->visible!=None)
			flnewlyvisible(l);
	}
}

void
llinsert(Flayer *l)
{
	int i;
	for(i=nllist; i>0; --i)
		llist[i]=llist[i-1];
	llist[0]=l;
	nllist++;
}

void
lldelete(Flayer *l)
{
	int i;

	for(i=0; i<nllist; i++)
		if(llist[i]==l){
			--nllist;
			for(; i<nllist; i++)
				llist[i] = llist[i+1];
			return;
		}
	panic("lldelete");
}

void
flinsert(Flayer *l, Rune *sp, Rune *ep, long p0)
{
	if(flprepare(l)){
		frinsert(&l->f, sp, ep, p0-l->origin);
		scrdraw(l, scrtotal(l));
		if(l->visible==Some)
			flrefresh(l, l->entire, 0);
	}
}

void
fldelete(Flayer *l, long p0, long p1)
{
	if(flprepare(l)){
		p0 -= l->origin;
		if(p0 < 0)
			p0 = 0;
		p1 -= l->origin;
		if(p1<0)
			p1 = 0;
		frdelete(&l->f, p0, p1);
		scrdraw(l, scrtotal(l));
		if(l->visible==Some)
			flrefresh(l, l->entire, 0);
	}
}

int
flselect(Flayer *l)
{
	int ret;
	if(l->visible!=All)
		flupfront(l);
	frselect(&l->f, mousectl);
	ret = 0;
	if(l->f.p0==l->f.p1){
		if(mousep->msec-l->click<Clicktime && l->f.p0+l->origin==l->p0){
			ret = 1;
			l->click = 0;
		}else
			l->click = mousep->msec;
	}else
		l->click = 0;
	l->p0 = l->f.p0+l->origin, l->p1 = l->f.p1+l->origin;
	return ret;
}

void
flsetselect(Flayer *l, long p0, long p1)
{
	ulong fp0, fp1;
	int ticked;

	l->click = 0;
	if(l->visible==None || !flprepare(l)){
		l->p0 = p0, l->p1 = p1;
		return;
	}
	l->p0 = p0, l->p1 = p1;
	flfp0p1(l, &fp0, &fp1, &ticked);
	if(fp0==l->f.p0 && fp1==l->f.p1){
		if(l->f.ticked != ticked)
			frtick(&l->f, frptofchar(&l->f, fp0), ticked);
		return;
	}

	if(fp1<=l->f.p0 || fp0>=l->f.p1 || l->f.p0==l->f.p1 || fp0==fp1){
		/* no overlap or trivial repainting */
		frdrawsel(&l->f, frptofchar(&l->f, l->f.p0), l->f.p0, l->f.p1, 0);
		if(fp0 != fp1 || ticked)
			frdrawsel(&l->f, frptofchar(&l->f, fp0), fp0, fp1, 1);
		goto Refresh;
	}
	/* the current selection and the desired selection overlap and are both non-empty */
	if(fp0 < l->f.p0){
		/* extend selection backwards */
		frdrawsel(&l->f, frptofchar(&l->f, fp0), fp0, l->f.p0, 1);
	}else if(fp0 > l->f.p0){
		/* trim first part of selection */
		frdrawsel(&l->f, frptofchar(&l->f, l->f.p0), l->f.p0, fp0, 0);
	}
	if(fp1 > l->f.p1){
		/* extend selection forwards */
		frdrawsel(&l->f, frptofchar(&l->f, l->f.p1), l->f.p1, fp1, 1);
	}else if(fp1 < l->f.p1){
		/* trim last part of selection */
		frdrawsel(&l->f, frptofchar(&l->f, fp1), fp1, l->f.p1, 0);
	}

    Refresh:
	l->f.p0 = fp0;
	l->f.p1 = fp1;
	if(l->visible==Some)
		flrefresh(l, l->entire, 0);
}

void
flfp0p1(Flayer *l, ulong *pp0, ulong *pp1, int *ticked)
{
	long p0 = l->p0-l->origin, p1 = l->p1-l->origin;

	*ticked = p0 == p1;
	if(p0 < 0){
		*ticked = 0;
		p0 = 0;
	}
	if(p1 < 0)
		p1 = 0;
	if(p0 > l->f.nchars)
		p0 = l->f.nchars;
	if(p1 > l->f.nchars){
		*ticked = 0;
		p1 = l->f.nchars;
	}
	*pp0 = p0;
	*pp1 = p1;
}

Rectangle
rscale(Rectangle r, Point old, Point new)
{
	r.min.x = r.min.x*new.x/old.x;
	r.min.y = r.min.y*new.y/old.y;
	r.max.x = r.max.x*new.x/old.x;
	r.max.y = r.max.y*new.y/old.y;
	return r;
}

void
flresize(Rectangle dr)
{
	int i;
	Flayer *l;
	Frame *f;
	Rectangle r, olDrect;
	int move;

	olDrect = lDrect;
	lDrect = dr;
	move = 0;
	/* no moving on rio; must repaint */
	if(0 && Dx(dr)==Dx(olDrect) && Dy(dr)==Dy(olDrect))
		move = 1;
	else
		draw(screen, lDrect, display->white, nil, ZP);
	for(i=0; i<nllist; i++){
		l = llist[i];
		l->lastsr = ZR;
		f = &l->f;
		if(move)
			r = rectaddpt(rectsubpt(l->entire, olDrect.min), dr.min);
		else{
			r = rectaddpt(rscale(rectsubpt(l->entire, olDrect.min),
				subpt(olDrect.max, olDrect.min),
				subpt(dr.max, dr.min)), dr.min);
			if(l->visible==Some && f->b){
				freeimage(f->b);
				frclear(f, 0);
			}
			f->b = 0;
			if(l->visible!=None)
				frclear(f, 0);
		}
		if(!rectclip(&r, dr))
			panic("flresize");
		if(r.max.x-r.min.x<100)
			r.min.x = dr.min.x;
		if(r.max.x-r.min.x<100)
			r.max.x = dr.max.x;
		if(r.max.y-r.min.y<2*FLMARGIN(l)+f->font->height)
			r.min.y = dr.min.y;
		if(r.max.y-r.min.y<2*FLMARGIN(l)+f->font->height)
			r.max.y = dr.max.y;
		if(!move)
			l->visible = None;
		frsetrects(f, insetrect(flrect(l, r), FLMARGIN(l)), f->b);
		if(!move && f->b)
			scrdraw(l, scrtotal(l));
	}
	newvisibilities(1);
}

int
flprepare(Flayer *l)
{
	Frame *f;
	ulong n;
	Rune *r;
	int ticked;

	if(l->visible == None)
		return 0;
	f = &l->f;
	if(f->b == 0){
		if(l->visible == All)
			f->b = screen;
		else if((f->b = allocimage(display, l->entire, screen->chan, 0, 0))==0)
			return 0;
		draw(f->b, l->entire, f->cols[BACK], nil, ZP);
		border(f->b, l->entire, l==llist[0]? FLMARGIN(l) : 1, f->cols[BORD], ZP);
		n = f->nchars;
		frinit(f, f->entire, f->font, f->b, 0);
		f->maxtab = maxtab*stringwidth(f->font, "0");
		r = (*l->textfn)(l, n, &n);
		frinsert(f, r, r+n, (ulong)0);
		frdrawsel(f, frptofchar(f, f->p0), f->p0, f->p1, 0);
		flfp0p1(l, &f->p0, &f->p1, &ticked);
		if(f->p0 != f->p1 || ticked)
			frdrawsel(f, frptofchar(f, f->p0), f->p0, f->p1, 1);
		l->lastsr = ZR;
		scrdraw(l, scrtotal(l));
	}
	return 1;
}

static	int	somevis, someinvis, justvis;

Vis
visibility(Flayer *l)
{
	somevis = someinvis = 0;
	justvis = 1;
	flrefresh(l, l->entire, 0);
	justvis = 0;
	if(somevis==0)
		return None;
	if(someinvis==0)
		return All;
	return Some;
}

void
flrefresh(Flayer *l, Rectangle r, int i)
{
	Flayer *t;
	Rectangle s;

    Top:
	if((t=llist[i++]) == l){
		if(!justvis)
			draw(screen, r, l->f.b, nil, r.min);
		somevis = 1;
	}else{
		if(!rectXrect(t->entire, r))
			goto Top;	/* avoid stacking unnecessarily */
		if(t->entire.min.x>r.min.x){
			s = r;
			s.max.x = t->entire.min.x;
			flrefresh(l, s, i);
			r.min.x = t->entire.min.x;
		}
		if(t->entire.min.y>r.min.y){
			s = r;
			s.max.y = t->entire.min.y;
			flrefresh(l, s, i);
			r.min.y = t->entire.min.y;
		}
		if(t->entire.max.x<r.max.x){
			s = r;
			s.min.x = t->entire.max.x;
			flrefresh(l, s, i);
			r.max.x = t->entire.max.x;
		}
		if(t->entire.max.y<r.max.y){
			s = r;
			s.min.y = t->entire.max.y;
			flrefresh(l, s, i);
			r.max.y = t->entire.max.y;
		}
		/* remaining piece of r is blocked by t; forget about it */
		someinvis = 1;
	}
}

int
flscale(Flayer *l, int n)
{
	if(l == nil)
		return n;
	return scalesize(l->f.display, n);
}
