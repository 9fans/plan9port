#include "stdinc.h"
#include "dat.h"
#include "fns.h"

enum
{
	Top = 1,
	Bottom = 1,
	Left = 40,
	Right = 0,
	MinWidth = Left+Right+2,
	MinHeight = Top+Bottom+2,
	DefaultWidth = Left+Right+500,
	DefaultHeight = Top+Bottom+40
};

QLock memdrawlock;
static Memsubfont *smallfont;
static Memimage *black;
static Memimage *blue;
static Memimage *red;
static Memimage *lofill[6];
static Memimage *hifill[6];
static Memimage *grid;

static ulong fill[] = {
	0xFFAAAAFF,	0xBB5D5DFF,	/* peach */
	DPalegreygreen, DPurpleblue,	/* aqua */
	DDarkyellow, DYellowgreen,	/* yellow */
	DMedgreen, DDarkgreen,		/* green */
	0x00AAFFFF, 0x0088CCFF,	/* blue */
	0xCCCCCCFF, 0x888888FF,	/* grey */
};

Memimage*
allocrepl(ulong color)
{
	Memimage *m;

	m = allocmemimage(Rect(0,0,1,1), RGB24);
	memfillcolor(m, color);
	m->flags |= Frepl;
	m->clipr = Rect(-1000000, -1000000, 1000000, 1000000);
	return m;
}

static void
ginit(void)
{
	static int first = 1;
	int i;

	if(!first)
		return;

	first = 0;
	memimageinit();
#ifdef PLAN9PORT
	smallfont = openmemsubfont(unsharp("#9/font/lucsans/lstr.10"));
#else
	smallfont = openmemsubfont("/lib/font/bit/lucidasans/lstr.10");
#endif
	black = memblack;
	blue = allocrepl(DBlue);
	red = allocrepl(DRed);
	grid = allocrepl(0x77777777);
	for(i=0; i<nelem(fill)/2 && i<nelem(lofill) && i<nelem(hifill); i++){
		lofill[i] = allocrepl(fill[2*i]);
		hifill[i] = allocrepl(fill[2*i+1]);
	}
}

static void
mklabel(char *str, int v)
{
	if(v < 0){
		v = -v;
		*str++ = '-';
	}
	if(v < 10000)
		sprint(str, "%d", v);
	else if(v < 10000000)
		sprint(str, "%dk", v/1000);
	else
		sprint(str, "%dM", v/1000000);
}

static void
drawlabel(Memimage *m, Point p, int n)
{
	char buf[30];
	Point w;

	mklabel(buf, n);
	w = memsubfontwidth(smallfont, buf);
	memimagestring(m, Pt(p.x-5-w.x, p.y), memblack, ZP, smallfont, buf);
}

static int
scalept(int val, int valmin, int valmax, int ptmin, int ptmax)
{
	if(val <= valmin)
		val = valmin;
	if(val >= valmax)
		val = valmax;
	if(valmax == valmin)
		valmax++;
	return ptmin + (vlong)(val-valmin)*(ptmax-ptmin)/(valmax-valmin);
}

Memimage*
statgraph(Graph *g)
{
	int i, nbin, x, lo, hi, min, max, first;
	Memimage *m;
	Rectangle r;
	Statbin *b, bin[2000];	/* 32 kB, but whack is worse */

	needstack(8192);	/* double check that bin didn't kill us */

	if(g->wid <= MinWidth)
		g->wid = DefaultWidth;
	if(g->ht <= MinHeight)
		g->ht = DefaultHeight;
	if(g->wid > nelem(bin))
		g->wid = nelem(bin);
	if(g->fill < 0)
		g->fill = ((uint)(uintptr)g->arg>>8)%nelem(lofill);
	if(g->fill > nelem(lofill))
		g->fill %= nelem(lofill);

	nbin = g->wid - (Left+Right);
	binstats(g->fn, g->arg, g->t0, g->t1, bin, nbin);

	/*
	 * compute bounds
	 */
	min = g->min;
	max = g->max;
	if(min < 0 || max <= min){
		min = max = 0;
		first = 1;
		for(i=0; i<nbin; i++){
			b = &bin[i];
			if(b->nsamp == 0)
				continue;
			if(first || b->min < min)
				min = b->min;
			if(first || b->max > max)
				max = b->max;
			first = 0;
		}
	}

	qlock(&memdrawlock);
	ginit();
	if(smallfont==nil || black==nil || blue==nil || red==nil || hifill[0]==nil || lofill[0]==nil){
		werrstr("graphics initialization failed: %r");
		qunlock(&memdrawlock);
		return nil;
	}

	/* fresh image */
	m = allocmemimage(Rect(0,0,g->wid,g->ht), ABGR32);
	if(m == nil){
		qunlock(&memdrawlock);
		return nil;
	}
	r = Rect(Left, Top, g->wid-Right, g->ht-Bottom);
	memfillcolor(m, DTransparent);

	/* x axis */
	memimagedraw(m, Rect(r.min.x, r.max.y, r.max.x, r.max.y+1), black, ZP, memopaque, ZP, S);

	/* y labels */
	drawlabel(m, r.min, max);
	if(min != 0)
		drawlabel(m, Pt(r.min.x, r.max.y-smallfont->height), min);

	/* actual data */
	for(i=0; i<nbin; i++){
		b = &bin[i];
		if(b->nsamp == 0)
			continue;
		lo = scalept(b->min, min, max, r.max.y, r.min.y);
		hi = scalept(b->max, min, max, r.max.y, r.min.y);
		x = r.min.x+i;
		hi-=2;
		memimagedraw(m, Rect(x, hi, x+1,lo), hifill[g->fill%nelem(hifill)], ZP, memopaque, ZP, S);
		memimagedraw(m, Rect(x, lo, x+1, r.max.y), lofill[g->fill%nelem(lofill)], ZP, memopaque, ZP, S);
	}

	if(bin[nbin-1].nsamp)
		drawlabel(m, Pt(r.max.x, r.min.y+(Dy(r)-smallfont->height)/2), bin[nbin-1].avg);
	qunlock(&memdrawlock);
	return m;
}
