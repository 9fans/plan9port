#include	<u.h>
#include	<libc.h>
#include	<ctype.h>
#include	<draw.h>
#include	<event.h>
#include	<cursor.h>
#include	<stdio.h>

#define Never	0xffffffff	/* Maximum ulong */
#define LOG2  0.301029995664
#define Button_bit(b)	(1 << ((b)-1))

enum {
	But1	= Button_bit(1),/* mouse buttons for events */
	But2	= Button_bit(2),
	But3	= Button_bit(3)
};
int cantmv = 1;			/* disallow rotate and move? 0..1 */
int top_border, bot_border, lft_border, rt_border;
int lft_border0;		/* lft_border for y-axis labels >0 */
int top_left, top_right;	/* edges of top line free space */
int Mv_delay = 400;		/* msec for button click vs. button hold down */
int Dotrad = 2;			/* dot radius in pixels */
int framewd=1;			/* line thickness for frame (pixels) */
int framesep=1;			/* distance between frame and surrounding text */
int outersep=1;			/* distance: surrounding text to screen edge */
Point sdigit;			/* size of a digit in the font */
Point smaxch;			/* assume any character in font fits in this */
double underscan = .05;		/* fraction of frame initially unused per side */
double fuzz = 6;		/* selection tolerance in pixels */
int tick_len = 15;		/* length of axis label tick mark in pixels */
FILE* logfil = 0;		/* dump selected points here if nonzero */

#define labdigs  3		/* allow this many sig digits in axis labels */
#define digs10pow 1000		/* pow(10,labdigs) */
#define axis_color  clr_im(DLtblue)




/********************************* Utilities  *********************************/

/* Prepend string s to null-terminated string in n-byte buffer buf[], truncating if
   necessary and using a space to separate s from the rest of buf[].
*/
char* str_insert(char* buf, char* s, int n)
{
	int blen, slen = strlen(s) + 1;
	if (slen >= n)
		{strncpy(buf,s,n); buf[n-1]='\0'; return buf;}
	blen = strlen(buf);
	if (blen >= n-slen)
		buf[blen=n-slen-1] = '\0';
	memmove(buf+slen, buf, slen+blen+1);
	memcpy(buf, s, slen-1);
	buf[slen-1] = ' ';
	return buf;
}

/* Alter string smain (without lengthening it) so as to remove the first occurrence of
   ssub, assuming ssub is ASCII.  Return nonzero (true) if string smain had to be changed.
   In spite of the ASCII-centric appearance, I think this can handle UTF in smain.
*/
int remove_substr(char* smain, char* ssub)
{
	char *ss, *s = strstr(smain, ssub);
	int n = strlen(ssub);
	if (s==0)
		return 0;
	if (islower((uchar)s[n]))
		s[0] ^= 32;			/* probably tolower(s[0]) or toupper(s[0]) */
	else {
		for (ss=s+n; *ss!=0; s++, ss++)
			*s = *ss;
		*s = '\0';
	}
	return 1;
}

void adjust_border(Font* f)
{
	int sep = framesep + outersep;
	sdigit = stringsize(f, "8");
	smaxch = stringsize(f, "MMMg");
	smaxch.x = (smaxch.x + 3)/4;
	lft_border0 = (1+labdigs)*sdigit.x + framewd + sep;
	rt_border = (lft_border0 - sep)/2 + outersep;
	bot_border = sdigit.y + framewd + sep;
	top_border = smaxch.y + framewd + sep;
	lft_border = lft_border0;		/* this gets reset later */
}


int is_off_screen(Point p)
{
	const Rectangle* r = &(screen->r);
	return p.x-r->min.x<lft_border || r->max.x-p.x<rt_border
		|| p.y-r->min.y<=top_border || r->max.y-p.y<=bot_border;
}


Cursor	bullseye =
{
	{-7, -7},
	{
		0x1F, 0xF8, 0x3F, 0xFC, 0x7F, 0xFE, 0xFB, 0xDF,
	 	0xF3, 0xCF, 0xE3, 0xC7, 0xFF, 0xFF, 0xFF, 0xFF,
	 	0xFF, 0xFF, 0xFF, 0xFF, 0xE3, 0xC7, 0xF3, 0xCF,
	 	0x7B, 0xDF, 0x7F, 0xFE, 0x3F, 0xFC, 0x1F, 0xF8,
	},
	{
		0x00, 0x00, 0x0F, 0xF0, 0x31, 0x8C, 0x21, 0x84,
		0x41, 0x82, 0x41, 0x82, 0x41, 0x82, 0x7F, 0xFE,
		0x7F, 0xFE, 0x41, 0x82, 0x41, 0x82, 0x41, 0x82,
		0x21, 0x84, 0x31, 0x8C, 0x0F, 0xF0, 0x00, 0x00,
	}
};

int get_1click(int but, Mouse* m, Cursor* curs)
{
	if (curs)
		esetcursor(curs);
	while (m->buttons==0)
		*m = emouse();
	if (curs)
		esetcursor(0);
	return (m->buttons==Button_bit(but));
}


/* Wait until but goes up or until a mouse event's msec passes tlimit.
   Return a boolean result that tells whether the button went up.
*/
int lift_button(int but, Mouse* m, int tlimit)
{
	do {	*m = emouse();
		if (m->msec >= tlimit)
			return 0;
	} while (m->buttons & Button_bit(but));
	return 1;
}


/* Set *m to the last pending mouse event, or the first one where but is up.
   If no mouse events are pending, wait for the next one.
*/
void latest_mouse(int but, Mouse* m)
{
	int bbit = Button_bit(but);
	do {	*m = emouse();
	} while ((m->buttons & bbit) && ecanmouse());
}



/*********************************** Colors ***********************************/

#define DOrange	0xFFAA00FF
#define Dgray		0xBBBBBBFF
#define DDkgreen	0x009900FF
#define DDkred	0xCC0000FF
#define DViolet		0x990099FF
#define DDkyellow	0xAAAA00FF
#define DLtblue	0xAAAAFFFF
#define DPink		0xFFAAAAFF

	/* draw.h sets DBlack, DBlue, DRed, DYellow, DGreen,
		DCyan, DMagenta, DWhite */

typedef struct color_ref {
	ulong c;			/* RGBA pixel color */
	char* nam;			/* ASCII name (matched to input, used in output)*/
	Image* im;			/* replicated solid-color image */
} color_ref;

color_ref clrtab[] = {
	DRed,		"Red",		0,
	DPink,		"Pink",		0,
	DDkred,		"Dkred",	0,
	DOrange,	"Orange",	0,
	DYellow,	"Yellow",	0,
	DDkyellow,	"Dkyellow",	0,
	DGreen,		"Green",	0,
	DDkgreen,	"Dkgreen",	0,
	DCyan,		"Cyan",		0,
	DBlue,		"Blue",		0,
	DLtblue,	"Ltblue",	0,
	DMagenta,	"Magenta",	0,
	DViolet,	"Violet",	0,
	Dgray,		"Gray",		0,
	DBlack,		"Black",	0,
	DWhite,		"White",	0,
	DNofill,	0,		0	/* DNofill means "end of data" */
};


void  init_clrtab(void)
{
	int i;
	Rectangle r = Rect(0,0,1,1);
	for (i=0; clrtab[i].c!=DNofill; i++)
		clrtab[i].im = allocimage(display, r, CMAP8, 1, clrtab[i].c);
		/* should check for 0 result? */
}


int clrim_id(Image* clr)
{
	int i;
	for (i=0; clrtab[i].im!=clr; i++)
		if (clrtab[i].c==DNofill)
			sysfatal("bad image color");
	return i;
}

int clr_id(ulong clr)
{
	int i;
	for (i=0; clrtab[i].c!=clr; i++)
		if (clrtab[i].c==DNofill)
			sysfatal("bad color %#x", clr);
	return i;
}

#define clr_im(clr)	clrtab[clr_id(clr)].im


/* This decides what color to use for a polyline based on the label it has in the
   input file.  Whichever color name comes first is the winner, otherwise return black.
*/
Image* nam2clr(const char* nam, int *idxdest)
{
	char *c, *cbest=(char*)nam;
	int i, ibest=-1;
	if (*nam!=0)
		for (i=0; clrtab[i].nam!=0; i++) {
			c = strstr(nam,clrtab[i].nam);
			if (c!=0 && (ibest<0 || c<cbest))
				{ibest=i; cbest=c;}
		}
	if (idxdest!=0)
		*idxdest = (ibest<0) ? clr_id(DBlack) : ibest;
	return (ibest<0) ? clr_im(DBlack) : clrtab[ibest].im;
}

/* A polyline is initial drawn in thick mode iff its label in the file contains "Thick" */
int nam2thick(const char* nam)
{
	return strstr(nam,"Thick")==0 ? 0 : 1;
}


/* Alter string nam so that nam2thick() and nam2clr() agree with th and clr, using
   buf[] (a buffer of length bufn) to store the result if it differs from nam.
   We go to great pains to perform this alteration in a manner that will seem natural
   to the user, i.e., we try removing a suitably isolated color name before inserting
   a new one.
*/
char* nam_with_thclr(char* nam, int th, Image* clr, char* buf, int bufn)
{
	int clr0i, th0=nam2thick(nam);
	Image* clr0 = nam2clr(nam, &clr0i);
	char *clr0s;
	if (th0==th && clr0==clr)
		return nam;
	clr0s = clrtab[clr0i].nam;
	if (strlen(nam)<bufn) strcpy(buf,nam);
	else {strncpy(buf,nam,bufn); buf[bufn-1]='\0';}
	if (clr0 != clr)
		remove_substr(buf, clr0s);
	if (th0 > th)
		while (remove_substr(buf, "Thick"))
			/* do nothing */;
	if (nam2clr(buf,0) != clr)
		str_insert(buf, clrtab[clrim_id(clr)].nam, bufn);
	if (th0 < th)
		str_insert(buf, "Thick", bufn);
	return buf;
}



/****************************** Data structures  ******************************/

Image* mv_bkgd;				/* Background image (usually 0) */

typedef struct fpoint {
	double x, y;
} fpoint;

typedef struct frectangle {
	fpoint min, max;
} frectangle;

frectangle empty_frect = {1e30, 1e30, -1e30, -1e30};


/* When *r2 is transformed by y=y-x*slant, might it intersect *r1 ?
*/
int fintersects(const frectangle* r1, const frectangle* r2, double slant)
{
	double x2min=r2->min.x, x2max=r2->max.x;
	if (r1->max.x <= x2min || x2max <= r1->min.x)
		return 0;
	if (slant >=0)
		{x2min*=slant; x2max*=slant;}
	else	{double t=x2min*slant; x2min=x2max*slant; x2max=t;}
	return r1->max.y > r2->min.y-x2max && r2->max.y-x2min > r1->min.y;
}

int fcontains(const frectangle* r, fpoint p)
{
	return r->min.x <=p.x && p.x<= r->max.x && r->min.y <=p.y && p.y<= r->max.y;
}


void grow_bb(frectangle* dest, const frectangle* r)
{
	if (r->min.x < dest->min.x) dest->min.x=r->min.x;
	if (r->min.y < dest->min.y) dest->min.y=r->min.y;
	if (r->max.x > dest->max.x) dest->max.x=r->max.x;
	if (r->max.y > dest->max.y) dest->max.y=r->max.y;
}


void slant_frect(frectangle *r, double sl)
{
	r->min.y += sl*r->min.x;
	r->max.y += sl*r->max.x;
}


fpoint fcenter(const frectangle* r)
{
	fpoint c;
	c.x = .5*(r->max.x + r->min.x);
	c.y = .5*(r->max.y + r->min.y);
	return c;
}


typedef struct fpolygon {
	fpoint* p;			/* a malloc'ed array */
	int n;				/* p[] has n elements: p[0..n] */
	frectangle bb;			/* bounding box */
	char* nam;			/* name of this polygon (malloc'ed) */
	int thick;			/* use 1+2*thick pixel wide lines */
	Image* clr;			/* Color to use when drawing this */
	struct fpolygon* link;
} fpolygon;

typedef struct fpolygons {
	fpolygon* p;			/* the head of a linked list */
	frectangle bb;			/* overall bounding box */
	frectangle disp;		/* part being mapped onto screen->r */
	double slant_ht;		/* controls how disp is slanted */
} fpolygons;


fpolygons univ = {			/* everything there is to display */
	0,
	1e30, 1e30, -1e30, -1e30,
	0, 0, 0, 0,
	2*1e30
};


void set_default_clrs(fpolygons* fps, fpolygon* fpstop)
{
	fpolygon* fp;
	for (fp=fps->p; fp!=0 && fp!=fpstop; fp=fp->link) {
		fp->clr = nam2clr(fp->nam,0);
		fp->thick = nam2thick(fp->nam);
	}
}


void fps_invert(fpolygons* fps)
{
	fpolygon *p, *r=0;
	for (p=fps->p; p!=0;) {
		fpolygon* q = p;
		p = p->link;
		q->link = r;
		r = q;
	}
	fps->p = r;
}


void fp_remove(fpolygons* fps, fpolygon* fp)
{
	fpolygon *q, **p = &fps->p;
	while (*p!=fp)
		if (*p==0)
			return;
		else	p = &(*p)->link;
	*p = fp->link;
	fps->bb = empty_frect;
	for (q=fps->p; q!=0; q=q->link)
		grow_bb(&fps->bb, &q->bb);
}


/* The transform maps abstract fpoint coordinates (the ones used in the input)
   to the current screen coordinates.  The do_untransform() macros reverses this.
   If univ.slant_ht is not the height of univ.disp, the actual region in the
   abstract coordinates is a parallelogram inscribed in univ.disp with two
   vertical edges and two slanted slanted edges: slant_ht>0 means that the
   vertical edges have height slant_ht and the parallelogram touches the lower
   left and upper right corners of univ.disp; slant_ht<0 refers to a parallelogram
   of height -slant_ht that touches the other two corners of univ.disp.
   NOTE: the ytransform macro assumes that tr->sl times the x coordinate has
   already been subtracted from yy.
*/
typedef struct transform {
	double sl;
	fpoint o, sc;		/* (x,y):->(o.x+sc.x*x, o.y+sc.y*y+sl*x) */
} transform;

#define do_transform(d,tr,s)	((d)->x = (tr)->o.x + (tr)->sc.x*(s)->x,  \
				(d)->y = (tr)->o.y + (tr)->sc.y*(s)->y    \
					+ (tr)->sl*(s)->x)
#define do_untransform(d,tr,s)	((d)->x = (.5+(s)->x-(tr)->o.x)/(tr)->sc.x,    \
				(d)->y = (.5+(s)->y-(tr)->sl*(d)->x-(tr)->o.y) \
					/(tr)->sc.y)
#define xtransform(tr,xx)	((tr)->o.x + (tr)->sc.x*(xx))
#define ytransform(tr,yy)	((tr)->o.y + (tr)->sc.y*(yy))
#define dxuntransform(tr,xx)	((xx)/(tr)->sc.x)
#define dyuntransform(tr,yy)	((yy)/(tr)->sc.y)


transform cur_trans(void)
{
	transform t;
	Rectangle d = screen->r;
	const frectangle* s = &univ.disp;
	double sh = univ.slant_ht;
	d.min.x += lft_border;
	d.min.y += top_border;
	d.max.x -= rt_border;
	d.max.y -= bot_border;
	t.sc.x = (d.max.x - d.min.x)/(s->max.x - s->min.x);
	t.sc.y = -(d.max.y - d.min.y)/fabs(sh);
	if (sh > 0) {
		t.sl = -t.sc.y*(s->max.y-s->min.y-sh)/(s->max.x - s->min.x);
		t.o.y = d.min.y - t.sc.y*s->max.y - t.sl*s->max.x;
	} else {
		t.sl = t.sc.y*(s->max.y-s->min.y+sh)/(s->max.x - s->min.x);
		t.o.y = d.min.y - t.sc.y*s->max.y - t.sl*s->min.x;
	}
	t.o.x = d.min.x - t.sc.x*s->min.x;
	return t;
}


double u_slant_amt(fpolygons *u)
{
	double sh=u->slant_ht, dy=u->disp.max.y - u->disp.min.y;
	double dx = u->disp.max.x - u->disp.min.x;
	return (sh>0) ? (dy-sh)/dx : -(dy+sh)/dx;
}


/* Set *y0 and *y1 to the lower and upper bounds of the set of y-sl*x values that
   *u says to display, where sl is the amount of slant.
*/
double set_unslanted_y(fpolygons *u, double *y0, double *y1)
{
	double yy1, sl=u_slant_amt(u);
	if (u->slant_ht > 0) {
		*y0 = u->disp.min.y - sl*u->disp.min.x;
		yy1 = *y0 + u->slant_ht;
	} else {
		yy1 = u->disp.max.y - sl*u->disp.min.x;
		*y0 = yy1 + u->slant_ht;
	}
	if (y1 != 0)
		*y1 = yy1;
	return sl;
}




/*************************** The region to display ****************************/

void nontrivial_interval(double *lo, double *hi)
{
	if (*lo >= *hi) {
		double mid = .5*(*lo + *hi);
		double tweak = 1e-6 + 1e-6*fabs(mid);
		*lo = mid - tweak;
		*hi = mid + tweak;
	}
}


void init_disp(void)
{
	double dw = (univ.bb.max.x - univ.bb.min.x)*underscan;
	double dh = (univ.bb.max.y - univ.bb.min.y)*underscan;
	univ.disp.min.x = univ.bb.min.x - dw;
	univ.disp.min.y = univ.bb.min.y - dh;
	univ.disp.max.x = univ.bb.max.x + dw;
	univ.disp.max.y = univ.bb.max.y + dh;
	nontrivial_interval(&univ.disp.min.x, &univ.disp.max.x);
	nontrivial_interval(&univ.disp.min.y, &univ.disp.max.y);
	univ.slant_ht = univ.disp.max.y - univ.disp.min.y;	/* means no slant */
}


void recenter_disp(Point c)
{
	transform tr = cur_trans();
	fpoint cc, off;
	do_untransform(&cc, &tr, &c);
	off.x = cc.x - .5*(univ.disp.min.x + univ.disp.max.x);
	off.y = cc.y - .5*(univ.disp.min.y + univ.disp.max.y);
	univ.disp.min.x += off.x;
	univ.disp.min.y += off.y;
	univ.disp.max.x += off.x;
	univ.disp.max.y += off.y;
}


/* Find the upper-left and lower-right corners of the bounding box of the
   parallelogram formed by untransforming the rectangle rminx, rminy, ... (given
   in screen coordinates), and return the height of the parallelogram (negated
   if it slopes downward).
*/
double untransform_corners(double rminx, double rminy, double rmaxx, double rmaxy,
		fpoint *ul, fpoint *lr)
{
	fpoint r_ur, r_ul, r_ll, r_lr;	/* corners of the given recangle */
	fpoint ur, ll;			/* untransformed versions of r_ur, r_ll */
	transform tr = cur_trans();
	double ht;
	r_ur.x=rmaxx;  r_ur.y=rminy;
	r_ul.x=rminx;  r_ul.y=rminy;
	r_ll.x=rminx;  r_ll.y=rmaxy;
	r_lr.x=rmaxx;  r_lr.y=rmaxy;
	do_untransform(ul, &tr, &r_ul);
	do_untransform(lr, &tr, &r_lr);
	do_untransform(&ur, &tr, &r_ur);
	do_untransform(&ll, &tr, &r_ll);
	ht = ur.y - lr->y;
	if (ll.x < ul->x)
		ul->x = ll.x;
	if (ur.y > ul->y)
		ul->y = ur.y;
	else	ht = -ht;
	if (ur.x > lr->x)
		lr->x = ur.x;
	if (ll.y < lr->y)
		lr->y = ll.y;
	return ht;
}


void disp_dozoom(double rminx, double rminy, double rmaxx, double rmaxy)
{
	fpoint ul, lr;
	double sh = untransform_corners(rminx, rminy, rmaxx, rmaxy, &ul, &lr);
	if (ul.x==lr.x || ul.y==lr.y)
		return;
	univ.slant_ht = sh;
	univ.disp.min.x = ul.x;
	univ.disp.max.y = ul.y;
	univ.disp.max.x = lr.x;
	univ.disp.min.y = lr.y;
	nontrivial_interval(&univ.disp.min.x, &univ.disp.max.x);
	nontrivial_interval(&univ.disp.min.y, &univ.disp.max.y);
}


void disp_zoomin(Rectangle r)
{
	disp_dozoom(r.min.x, r.min.y, r.max.x, r.max.y);
}


void disp_zoomout(Rectangle r)
{
	double qminx, qminy, qmaxx, qmaxy;
	double scx, scy;
	Rectangle s = screen->r;
	if (r.min.x==r.max.x || r.min.y==r.max.y)
		return;
	s.min.x += lft_border;
	s.min.y += top_border;
	s.max.x -= rt_border;
	s.max.y -= bot_border;
	scx = (s.max.x - s.min.x)/(r.max.x - r.min.x);
	scy = (s.max.y - s.min.y)/(r.max.y - r.min.y);
	qminx = s.min.x + scx*(s.min.x - r.min.x);
	qmaxx = s.max.x + scx*(s.max.x - r.max.x);
	qminy = s.min.y + scy*(s.min.y - r.min.y);
	qmaxy = s.max.y + scy*(s.max.y - r.max.y);
	disp_dozoom(qminx, qminy, qmaxx, qmaxy);
}


void expand2(double* a, double* b, double f)
{
	double mid = .5*(*a + *b);
	*a = mid + f*(*a - mid);
	*b = mid + f*(*b - mid);
}

void disp_squareup(void)
{
	double dx = univ.disp.max.x - univ.disp.min.x;
	double dy = univ.disp.max.y - univ.disp.min.y;
	dx /= screen->r.max.x - lft_border - screen->r.min.x - rt_border;
	dy /= screen->r.max.y - bot_border - screen->r.min.y - top_border;
	if (dx > dy)
		expand2(&univ.disp.min.y, &univ.disp.max.y, dx/dy);
	else	expand2(&univ.disp.min.x, &univ.disp.max.x, dy/dx);
	univ.slant_ht = univ.disp.max.y - univ.disp.min.y;
}


/* Slant so that p and q appear at the same height on the screen and the
   screen contains the smallest possible superset of what its previous contents.
*/
void slant_disp(fpoint p, fpoint q)
{
	double yll, ylr, yul, yur;	/* corner y coords of displayed parallelogram */
	double sh, dy;
	if (p.x == q.x)
		return;
	sh = univ.slant_ht;
	if (sh > 0) {
		yll=yul=univ.disp.min.y;  yul+=sh;
		ylr=yur=univ.disp.max.y;  ylr-=sh;
	} else {
		yll=yul=univ.disp.max.y;  yll+=sh;
		ylr=yur=univ.disp.min.y;  yur-=sh;
	}
	dy = (univ.disp.max.x-univ.disp.min.x)*(q.y - p.y)/(q.x - p.x);
	dy -= ylr - yll;
	if (dy > 0)
		{yll-=dy; yur+=dy;}
	else	{yul-=dy; ylr+=dy;}
	if (ylr > yll) {
		univ.disp.min.y = yll;
		univ.disp.max.y = yur;
		univ.slant_ht = yur - ylr;
	} else {
		univ.disp.max.y = yul;
		univ.disp.min.y = ylr;
		univ.slant_ht = ylr - yur;
	}
}




/******************************** Ascii input  ********************************/

void set_fbb(fpolygon* fp)
{
	fpoint lo=fp->p[0], hi=fp->p[0];
	const fpoint *q, *qtop;
	for (qtop=(q=fp->p)+fp->n; ++q<=qtop;) {
		if (q->x < lo.x) lo.x=q->x;
		if (q->y < lo.y) lo.y=q->y;
		if (q->x > hi.x) hi.x=q->x;
		if (q->y > hi.y) hi.y=q->y;
	}
	fp->bb.min = lo;
	fp->bb.max = hi;
}

char* mystrdup(char* s)
{
	char *r, *t = strrchr(s,'"');
	if (t==0) {
		t = s + strlen(s);
		while (t>s && (t[-1]=='\n' || t[-1]=='\r'))
			t--;
	}
	r = malloc(1+(t-s));
	memcpy(r, s, t-s);
	r[t-s] = 0;
	return r;
}

int is_valid_label(char* lab)
{
	char* t;
	if (lab[0]=='"')
		return (t=strrchr(lab,'"'))!=0 && t!=lab && strspn(t+1," \t\r\n")==strlen(t+1);
	return strcspn(lab," \t")==strlen(lab);
}

/* Read a polyline and update the number of lines read.  A zero result indicates bad
   syntax if *lineno increases; otherwise it indicates end of file.
*/
fpolygon* rd_fpoly(FILE* fin, int *lineno)
{
	char buf[1024], junk[2];
	fpoint q;
	fpolygon* fp;
	int allocn;
	if (!fgets(buf,sizeof buf,fin))
		return 0;
	(*lineno)++;
	if (sscanf(buf,"%lg%lg%1s",&q.x,&q.y,junk) != 2)
		return 0;
	fp = malloc(sizeof(fpolygon));
	allocn = 16;
	fp->p = malloc(allocn*sizeof(fpoint));
	fp->p[0] = q;
	fp->n = 0;
	fp->nam = "";
	fp->thick = 0;
	fp->clr = clr_im(DBlack);
	while (fgets(buf,sizeof buf,fin)) {
		(*lineno)++;
		if (sscanf(buf,"%lg%lg%1s",&q.x,&q.y,junk) != 2) {
			if (!is_valid_label(buf))
				{free(fp->p); free(fp); return 0;}
			fp->nam = (buf[0]=='"') ? buf+1 : buf;
			break;
		}
		if (++(fp->n) == allocn)
			fp->p = realloc(fp->p, (allocn<<=1)*sizeof(fpoint));
		fp->p[fp->n] = q;
	}
	fp->nam = mystrdup(fp->nam);
	set_fbb(fp);
	fp->link = 0;
	return fp;
}


/* Read input into *fps and return 0 or a line number where there's a syntax error */
int rd_fpolys(FILE* fin, fpolygons* fps)
{
	fpolygon *fp, *fp0=fps->p;
	int lineno=0, ok_upto=0;
	while ((fp=rd_fpoly(fin,&lineno)) != 0) {
		ok_upto = lineno;
		fp->link = fps->p;
		fps->p = fp;
		grow_bb(&fps->bb, &fp->bb);
	}
	set_default_clrs(fps, fp0);
	return (ok_upto==lineno) ? 0 : lineno;
}


/* Read input from file fnam and return an error line no., -1 for "can't open"
   or 0 for success.
*/
int doinput(char* fnam)
{
	FILE* fin = strcmp(fnam,"-")==0 ? stdin : fopen(fnam, "r");
	int errline_or0;
	if (fin==0)
		return -1;
	errline_or0 = rd_fpolys(fin, &univ);
	fclose(fin);
	return errline_or0;
}



/******************************** Ascii output ********************************/

fpolygon* fp_reverse(fpolygon* fp)
{
	fpolygon* r = 0;
	while (fp!=0) {
		fpolygon* q = fp->link;
		fp->link = r;
		r = fp;
		fp = q;
	}
	return r;
}

void wr_fpoly(FILE* fout, const fpolygon* fp)
{
	char buf[1024];
	int i;
	for (i=0; i<=fp->n; i++)
		fprintf(fout,"%.12g\t%.12g\n", fp->p[i].x, fp->p[i].y);
	fprintf(fout,"\"%s\"\n", nam_with_thclr(fp->nam, fp->thick, fp->clr, buf, 256));
}

void wr_fpolys(FILE* fout, fpolygons* fps)
{
	fpolygon* fp;
	fps->p = fp_reverse(fps->p);
	for (fp=fps->p; fp!=0; fp=fp->link)
		wr_fpoly(fout, fp);
	fps->p = fp_reverse(fps->p);
}


int dooutput(char* fnam)
{
	FILE* fout = fopen(fnam, "w");
	if (fout==0)
		return 0;
	wr_fpolys(fout, &univ);
	fclose(fout);
	return 1;
}




/************************ Clipping to screen rectangle ************************/

/* Find the t values, 0<=t<=1 for which x0+t*(x1-x0) is between xlo and xhi,
   or return 0 to indicate no such t values exist.  If returning 1, set *t0 and
   *t1 to delimit the t interval.
*/
int do_xory(double x0, double x1, double xlo, double xhi, double* t0, double* t1)
{
	*t1 = 1.0;
	if (x0<xlo) {
		if (x1<xlo) return 0;
		*t0 = (xlo-x0)/(x1-x0);
		if (x1>xhi) *t1 = (xhi-x0)/(x1-x0);
	} else if (x0>xhi) {
		if (x1>xhi) return 0;
		*t0 = (xhi-x0)/(x1-x0);
		if (x1<xlo) *t1 = (xlo-x0)/(x1-x0);
	} else {
		*t0 = 0.0;
		if (x1>xhi) *t1 = (xhi-x0)/(x1-x0);
		else if (x1<xlo) *t1 = (xlo-x0)/(x1-x0);
		else *t1 = 1.0;
	}
	return 1;
}


/* After mapping y to y-slope*x, what initial fraction of the *p to *q edge is
   outside of *r?  Note that the edge could start outside *r, pass through *r,
   and wind up outside again.
*/
double frac_outside(const fpoint* p, const fpoint* q, const frectangle* r,
		double slope)
{
	double t0, t1, tt0, tt1;
	double px=p->x, qx=q->x;
	if (!do_xory(px, qx, r->min.x, r->max.x, &t0, &t1))
		return 1;
	if (!do_xory(p->y-slope*px, q->y-slope*qx, r->min.y, r->max.y, &tt0, &tt1))
		return 1;
	if (tt0 > t0)
		t0 = tt0;
	if (t1<=t0 || tt1<=t0)
		return 1;
	return t0;
}


/* Think of p0..pn as piecewise-linear function F(t) for t=0..pn-p0, and find
   the maximum tt such that F(0..tt) is all inside of r, assuming p0 is inside.
   Coordinates are transformed by y=y-x*slope before testing against r.
*/
double in_length(const fpoint* p0, const fpoint* pn, frectangle r, double slope)
{
	const fpoint* p = p0;
	double px, py;
	do if (++p > pn)
		return pn - p0;
	while (r.min.x<=(px=p->x) && px<=r.max.x
			&& r.min.y<=(py=p->y-slope*px) && py<=r.max.y);
	return (p - p0) - frac_outside(p, p-1, &r, slope);
}


/* Think of p0..pn as piecewise-linear function F(t) for t=0..pn-p0, and find
   the maximum tt such that F(0..tt) is all outside of *r.  Coordinates are
   transformed by y=y-x*slope before testing against r.
*/
double out_length(const fpoint* p0, const fpoint* pn, frectangle r, double slope)
{
	const fpoint* p = p0;
	double fr;
	do {	if (p->x < r.min.x)
			do if (++p>pn) return pn-p0;
			while (p->x <= r.min.x);
		else if (p->x > r.max.x)
			do if (++p>pn) return pn-p0;
			while (p->x >= r.max.x);
		else if (p->y-slope*p->x < r.min.y)
			do if (++p>pn) return pn-p0;
			while (p->y-slope*p->x <= r.min.y);
		else if (p->y-slope*p->x > r.max.y)
			do if (++p>pn) return pn-p0;
			while (p->y-slope*p->x >= r.max.y);
		else return p - p0;
	} while ((fr=frac_outside(p-1,p,&r,slope)) == 1);
	return (p - p0) + fr-1;
}



/*********************** Drawing frame and axis labels  ***********************/

#define Nthous  7
#define Len_thous  30			/* bound on strlen(thous_nam[i]) */
char* thous_nam[Nthous] = {
	"one", "thousand", "million", "billion",
	"trillion", "quadrillion", "quintillion"
};


typedef struct lab_interval {
	double sep;			/* separation between tick marks */
	double unit;		/* power of 1000 divisor */
	int logunit;		/* log base 1000 of of this divisor */
	double off;			/* offset to subtract before dividing */
} lab_interval;


char* abbrev_num(double x, const lab_interval* iv)
{
	static char buf[16];
	double dx = x - iv->off;
	dx = iv->sep * floor(dx/iv->sep + .5);
	sprintf(buf,"%g", dx/iv->unit);
	return buf;
}


double lead_digits(double n, double r)	/* n truncated to power of 10 above r */
{
	double rr = pow(10, ceil(log10(r)));
	double nn = (n<rr) ? 0.0 : rr*floor(n/rr);
	if (n+r-nn >= digs10pow) {
		rr /= 10;
		nn = (n<rr) ? 0.0 : rr*floor(n/rr);
	}
	return nn;
}


lab_interval next_larger(double s0, double xlo, double xhi)
{
	double nlo, nhi;
	lab_interval r;
	r.logunit = (int) floor(log10(s0) + LOG2);
	r.unit = pow(10, r.logunit);
	nlo = xlo/r.unit;
	nhi = xhi/r.unit;
	if (nhi >= digs10pow)
		r.off = r.unit*lead_digits(nlo, nhi-nlo);
	else if (nlo <= -digs10pow)
		r.off = -r.unit*lead_digits(-nhi, nhi-nlo);
	else	r.off = 0;
	r.sep = (s0<=r.unit) ? r.unit : (s0<2*r.unit ? 2*r.unit : 5*r.unit);
	switch (r.logunit%3) {
	case 1:	r.unit*=.1; r.logunit--;
		break;
	case -1: case 2:
		r.unit*=10; r.logunit++;
		break;
	case -2: r.unit*=100; r.logunit+=2;
	}
	r.logunit /= 3;
	return r;
}


double min_hsep(const transform* tr)
{
	double s = (2+labdigs)*sdigit.x;
	double ss = (univ.disp.min.x<0) ? s+sdigit.x : s;
	return dxuntransform(tr, ss);
}


lab_interval mark_x_axis(const transform* tr)
{
	fpoint p = univ.disp.min;
	Point q, qtop, qbot, tmp;
	double x0=univ.disp.min.x, x1=univ.disp.max.x;
	double seps0, nseps, seps;
	lab_interval iv = next_larger(min_hsep(tr), x0, x1);
	set_unslanted_y(&univ, &p.y, 0);
	q.y = ytransform(tr, p.y) + .5;
	qtop.y = q.y - tick_len;
	qbot.y = q.y + framewd + framesep;
	seps0 = ceil(x0/iv.sep);
	for (seps=0, nseps=floor(x1/iv.sep)-seps0; seps<=nseps; seps+=1) {
		char* num = abbrev_num((p.x=iv.sep*(seps0+seps)), &iv);
		Font* f = display->defaultfont;
		q.x = qtop.x = qbot.x = xtransform(tr, p.x);
		line(screen, qtop, q, Enddisc, Enddisc, 0, axis_color, q);
		tmp = stringsize(f, num);
		qbot.x -= tmp.x/2;
		string(screen, qbot, display->black, qbot, f, num);
	}
	return iv;
}


lab_interval mark_y_axis(const transform* tr)
{
	Font* f = display->defaultfont;
	fpoint p = univ.disp.min;
	Point q, qrt, qlft;
	double y0, y1, seps0, nseps, seps;
	lab_interval iv;
	set_unslanted_y(&univ, &y0, &y1);
	iv = next_larger(dyuntransform(tr,-f->height), y0, y1);
	q.x = xtransform(tr, p.x) - .5;
	qrt.x = q.x + tick_len;
	qlft.x = q.x - (framewd + framesep);
	seps0 = ceil(y0/iv.sep);
	for (seps=0, nseps=floor(y1/iv.sep)-seps0; seps<=nseps; seps+=1) {
		char* num = abbrev_num((p.y=iv.sep*(seps0+seps)), &iv);
		Point qq = stringsize(f, num);
		q.y = qrt.y = qlft.y = ytransform(tr, p.y);
		line(screen, qrt, q, Enddisc, Enddisc, 0, axis_color, q);
		qq.x = qlft.x - qq.x;
		qq.y = qlft.y - qq.y/2;
		string(screen, qq, display->black, qq, f, num);
	}
	return iv;
}


void lab_iv_info(const lab_interval *iv, double slant, char* buf, int *n)
{
	if (iv->off > 0)
		(*n) += sprintf(buf+*n,"-%.12g",iv->off);
	else if (iv->off < 0)
		(*n) += sprintf(buf+*n,"+%.12g",-iv->off);
	if (slant>0)
		(*n) += sprintf(buf+*n,"-%.6gx", slant);
	else if (slant<0)
		(*n) += sprintf(buf+*n,"+%.6gx", -slant);
	if (abs(iv->logunit) >= Nthous)
		(*n) += sprintf(buf+*n," in 1e%d units", 3*iv->logunit);
	else if (iv->logunit > 0)
		(*n) += sprintf(buf+*n," in %ss", thous_nam[iv->logunit]);
	else if (iv->logunit < 0)
		(*n) += sprintf(buf+*n," in %sths", thous_nam[-iv->logunit]);
}


void draw_xy_ranges(const lab_interval *xiv, const lab_interval *yiv)
{
	Point p;
	char buf[2*(19+Len_thous+8)+50];
	int bufn = 0;
	buf[bufn++] = 'x';
	lab_iv_info(xiv, 0, buf, &bufn);
	bufn += sprintf(buf+bufn, "; y");
	lab_iv_info(yiv, u_slant_amt(&univ), buf, &bufn);
	buf[bufn] = '\0';
	p = stringsize(display->defaultfont, buf);
	top_left = screen->r.min.x + lft_border;
	p.x = top_right = screen->r.max.x - rt_border - p.x;
	p.y = screen->r.min.y + outersep;
	string(screen, p, display->black, p, display->defaultfont, buf);
}


transform draw_frame(void)
{
	lab_interval x_iv, y_iv;
	transform tr;
	Rectangle r = screen->r;
	lft_border = (univ.disp.min.y<0) ? lft_border0+sdigit.x : lft_border0;
	tr = cur_trans();
	r.min.x += lft_border;
	r.min.y += top_border;
	r.max.x -= rt_border;
	r.max.y -= bot_border;
	border(screen, r, -framewd, axis_color, r.min);
	x_iv = mark_x_axis(&tr);
	y_iv = mark_y_axis(&tr);
	draw_xy_ranges(&x_iv, &y_iv);
	return tr;
}



/*************************** Finding the selection  ***************************/

typedef struct pt_on_fpoly {
	fpoint p;			/* the point */
	fpolygon* fp;			/* the fpolygon it lies on */
	double t;			/* how many knots from the beginning */
} pt_on_fpoly;


static double myx, myy;
#define mydist(p,o,sl,xwt,ywt)	(myx=(p).x-(o).x, myy=(p).y-sl*(p).x-(o).y,	\
					xwt*myx*myx + ywt*myy*myy)

/* At what fraction of the way from p0[0] to p0[1] is mydist(p,ctr,slant,xwt,ywt)
   minimized?
*/
double closest_time(const fpoint* p0, const fpoint* ctr, double slant,
		double xwt, double ywt)
{
	double p00y=p0[0].y-slant*p0[0].x, p01y=p0[1].y-slant*p0[1].x;
	double dx=p0[1].x-p0[0].x, dy=p01y-p00y;
	double x0=p0[0].x-ctr->x, y0=p00y-ctr->y;
	double bot = xwt*dx*dx + ywt*dy*dy;
	if (bot==0)
		return 0;
	return -(xwt*x0*dx + ywt*y0*dy)/bot;
}


/* Scan the polygonal path of length len knots starting at p0, and find the
   point that the transformation y=y-x*slant makes closest to the center of *r,
   where *r itself defines the distance metric.  Knots get higher priority than
   points between knots.  If psel->t is negative, always update *psel; otherwise
   update *psel only if the scan can improve it.  Return a boolean that says
   whether *psel was updated.
     Note that *r is a very tiny rectangle (tiny when converted screen pixels)
   such that anything in *r is considered close enough to match the mouse click.
   The purpose of this routine is to be careful in case there is a lot of hidden
   detail in the tiny rectangle *r.
*/
int improve_pt(fpoint* p0, double len, const frectangle* r, double slant,
		pt_on_fpoly* psel)
{
	fpoint ctr = fcenter(r);
	double x_wt=2/(r->max.x-r->min.x), y_wt=2/(r->max.y-r->min.y);
	double xwt=x_wt*x_wt, ywt=y_wt*y_wt;
	double d, dbest = (psel->t <0) ? 1e30 : mydist(psel->p,ctr,slant,xwt,ywt);
	double tt, dbest0 = dbest;
	fpoint pp;
	int ilen = (int) len;
	if (len==0 || ilen>0) {
		int i;
		for (i=(len==0 ? 0 : 1); i<=ilen; i++) {
			d = mydist(p0[i], ctr, slant, xwt, ywt);
			if (d < dbest)
				{psel->p=p0[i]; psel->t=i; dbest=d;}
		}
		return (dbest < dbest0);
	}
	tt = closest_time(p0, &ctr, slant, xwt, ywt);
	if (tt > len)
		tt = len;
	pp.x = p0[0].x + tt*(p0[1].x - p0[0].x);
	pp.y = p0[0].y + tt*(p0[1].y - p0[0].y);
	if (mydist(pp, ctr, slant, xwt, ywt) < dbest) {
		psel->p = pp;
		psel->t = tt;
		return 1;
	}
	return 0;
}


/* Test *fp against *r after transforming by y=y-x*slope, and set *psel accordingly.
*/
void select_in_fpoly(fpolygon* fp, const frectangle* r, double slant,
		pt_on_fpoly* psel)
{
	fpoint *p0=fp->p, *pn=fp->p+fp->n;
	double l1, l2;
	if (p0==pn)
		{improve_pt(p0, 0, r, slant, psel); psel->fp=fp; return;}
	while ((l1=out_length(p0,pn,*r,slant)) < pn-p0) {
		fpoint p0sav;
		int i1 = (int) l1;
		p0+=i1; l1-=i1;
		p0sav = *p0;
		p0[0].x += l1*(p0[1].x - p0[0].x);
		p0[0].y += l1*(p0[1].y - p0[0].y);
		l2 = in_length(p0, pn, *r, slant);
		if (improve_pt(p0, l2, r, slant, psel)) {
			if (l1==0 && psel->t!=((int) psel->t)) {
				psel->t = 0;
				psel->p = *p0;
			} else if (psel->t < 1)
				psel->t += l1*(1 - psel->t);
			psel->t += p0 - fp->p;
			psel->fp = fp;
		}
		*p0 = p0sav;
		p0 += (l2>0) ? ((int) ceil(l2)) : 1;
	}
}


/* Test all the fpolygons against *r after transforming by y=y-x*slope, and return
   the resulting selection, if any.
*/
pt_on_fpoly* select_in_univ(const frectangle* r, double slant)
{
	static pt_on_fpoly answ;
	fpolygon* fp;
	answ.t = -1;
	for (fp=univ.p; fp!=0; fp=fp->link)
		if (fintersects(r, &fp->bb, slant))
			select_in_fpoly(fp, r, slant, &answ);
	if (answ.t < 0)
		return 0;
	return &answ;
}



/**************************** Using the selection  ****************************/

pt_on_fpoly cur_sel;			/* current selection if cur_sel.t>=0 */
pt_on_fpoly prev_sel;			/* previous selection if prev_sel.t>=0 (for slant) */
Image* sel_bkg = 0;			/* what's behind the red dot */


void clear_txt(void)
{
	Rectangle r;
	r.min = screen->r.min;
	r.min.x += lft_border;
	r.min.y += outersep;
	r.max.x = top_left;
	r.max.y = r.min.y + smaxch.y;
	draw(screen, r, display->white, display->opaque, r.min);
	top_left = r.min.x;
}


Rectangle sel_dot_box(const transform* tr)
{
	Point ctr;
	Rectangle r;
	if (tr==0)
		ctr.x = ctr.y = Dotrad;
	else	do_transform(&ctr, tr, &cur_sel.p);
	r.min.x=ctr.x-Dotrad;  r.max.x=ctr.x+Dotrad+1;
	r.min.y=ctr.y-Dotrad;  r.max.y=ctr.y+Dotrad+1;
	return r;
}


void unselect(const transform* tr)
{
	transform tra;
	if (sel_bkg==0)
		sel_bkg = allocimage(display, sel_dot_box(0), CMAP8, 0, DWhite);
	clear_txt();
	if (cur_sel.t < 0)
		return;
	prev_sel = cur_sel;
	if (tr==0)
		{tra=cur_trans(); tr=&tra;}
	draw(screen, sel_dot_box(tr), sel_bkg, display->opaque, ZP);
	cur_sel.t = -1;
}


/* Text at top right is written first and this low-level routine clobbers it if
   the new top-left text would overwrite it.  However, users of this routine should
   try to keep the new text short enough to avoid this.
*/
void show_mytext(char* msg)
{
	Point tmp, pt = screen->r.min;
	int siz;
	tmp = stringsize(display->defaultfont, msg);
	siz = tmp.x;
	pt.x=top_left;  pt.y+=outersep;
	if (top_left+siz > top_right) {
		Rectangle r;
		r.min.y = pt.y;
		r.min.x = top_right;
		r.max.y = r.min.y + smaxch.y;
		r.max.x = top_left+siz;
		draw(screen, r, display->white, display->opaque, r.min);
		top_right = top_left+siz;
	}
	string(screen, pt, display->black, ZP, display->defaultfont, msg);
	top_left += siz;
}


double rnd(double x, double tol)	/* round to enough digits for accuracy tol */
{
	double t = pow(10, floor(log10(tol)));
	return t * floor(x/t + .5);
}

double t_tol(double xtol, double ytol)
{
	int t = (int) floor(cur_sel.t);
	fpoint* p = cur_sel.fp->p;
	double dx, dy;
	if (t==cur_sel.t)
		return 1;
	dx = fabs(p[t+1].x - p[t].x);
	dy = fabs(p[t+1].y - p[t].y);
	xtol /= (xtol>dx) ? xtol : dx;
	ytol /= (ytol>dy) ? ytol : dy;
	return (xtol<ytol) ? xtol : ytol;
}

void say_where(const transform* tr)
{
	double xtol=dxuntransform(tr,1), ytol=dyuntransform(tr,-1);
	char buf[100];
	int n, nmax = (top_right - top_left)/smaxch.x;
	if (nmax >= 100)
		nmax = 100-1;
	n = sprintf(buf,"(%.14g,%.14g) at t=%.14g",
			rnd(cur_sel.p.x,xtol), rnd(cur_sel.p.y,ytol),
			rnd(cur_sel.t, t_tol(xtol,ytol)));
	if (cur_sel.fp->nam[0] != 0)
		sprintf(buf+n," %.*s", nmax-n-1, cur_sel.fp->nam);
	show_mytext(buf);
}


void reselect(const transform* tr)	/* uselect(); set cur_sel; call this */
{
	Point pt2, pt3;
	fpoint p2;
	transform tra;
	if (cur_sel.t < 0)
		return;
	if (tr==0)
		{tra=cur_trans(); tr=&tra;}
	do_transform(&p2, tr, &cur_sel.p);
	if (fabs(p2.x)+fabs(p2.y)>1e8 || (pt2.x=p2.x, pt2.y=p2.y, is_off_screen(pt2)))
		{cur_sel.t= -1; return;}
	pt3.x=pt2.x-Dotrad;  pt3.y=pt2.y-Dotrad;
	draw(sel_bkg, sel_dot_box(0), screen, display->opaque, pt3);
	fillellipse(screen, pt2, Dotrad, Dotrad, clr_im(DRed), pt2);
	say_where(tr);
}


void do_select(Point pt)
{
	transform tr = cur_trans();
	fpoint pt1, pt2, ctr;
	frectangle r;
	double slant;
	pt_on_fpoly* psel;
	unselect(&tr);
	do_untransform(&ctr, &tr, &pt);
	pt1.x=pt.x-fuzz;  pt1.y=pt.y+fuzz;
	pt2.x=pt.x+fuzz;  pt2.y=pt.y-fuzz;
	do_untransform(&r.min, &tr, &pt1);
	do_untransform(&r.max, &tr, &pt2);
	slant = u_slant_amt(&univ);
	slant_frect(&r, -slant);
	psel = select_in_univ(&r, slant);
	if (psel==0)
		return;
	if (logfil!=0) {
		fprintf(logfil,"%.14g\t%.14g\n", psel->p.x, psel->p.y);
		fflush(logfil);
	}
	cur_sel = *psel;
	reselect(&tr);
}


/***************************** Prompting for text *****************************/

void unshow_mytext(char* msg)
{
	Rectangle r;
	Point siz = stringsize(display->defaultfont, msg);
	top_left -= siz.x;
	r.min.y = screen->r.min.y + outersep;
	r.min.x = top_left;
	r.max.y = r.min.y + siz.y;
	r.max.x = r.min.x + siz.x;
	draw(screen, r, display->white, display->opaque, r.min);
}


/* Show the given prompt and read a line of user input.  The text appears at the
   top left.  If it runs into the top right text, we stop echoing but let the user
   continue typing blind if he wants to.
*/
char* prompt_text(char* prompt)
{
	static char buf[200];
	int n0, n=0, nshown=0;
	Rune c;
	unselect(0);
	show_mytext(prompt);
	while (n<200-1-UTFmax && (c=ekbd())!='\n') {
		if (c=='\b') {
			buf[n] = 0;
			if (n > 0)
				do n--;
				while (n>0 && (buf[n-1]&0xc0)==0x80);
			if (n < nshown)
				{unshow_mytext(buf+n); nshown=n;}
		} else {
			n0 = n;
			n += runetochar(buf+n, &c);
			buf[n] = 0;
			if (nshown==n0 && top_right-top_left >= smaxch.x)
				{show_mytext(buf+n0); nshown=n;}
		}
	}
	buf[n] = 0;
	while (ecanmouse())
		emouse();
	return buf;
}


/**************************** Redrawing the screen ****************************/

/* Let p0 and its successors define a piecewise-linear function of a paramter t,
   and draw the 0<=t<=n1 portion using transform *tr.
*/
void draw_fpts(const fpoint* p0, double n1, const transform* tr, int thick,
		Image* clr)
{
	int n = (int) n1;
	const fpoint* p = p0 + n;
	fpoint pp;
	Point qq, q;
	if (n1 > n) {
		pp.x = p[0].x + (n1-n)*(p[1].x - p[0].x);
		pp.y = p[0].y + (n1-n)*(p[1].y - p[0].y);
	} else	pp = *p--;
	do_transform(&qq, tr, &pp);
	if (n1==0)
		fillellipse(screen, qq, 1+thick, 1+thick, clr, qq);
	for (; p>=p0; p--) {
		do_transform(&q, tr, p);
		line(screen, qq, q, Enddisc, Enddisc, thick, clr, qq);
		qq = q;
	}
}

void draw_1fpoly(const fpolygon* fp, const transform* tr, Image* clr,
		const frectangle *udisp, double slant)
{
	fpoint *p0=fp->p, *pn=fp->p+fp->n;
	double l1, l2;
	if (p0==pn && fcontains(udisp,*p0))
		{draw_fpts(p0, 0, tr, fp->thick, clr); return;}
	while ((l1=out_length(p0,pn,*udisp,slant)) < pn-p0) {
		fpoint p0sav;
		int i1 = (int) l1;
		p0+=i1; l1-=i1;
		p0sav = *p0;
		p0[0].x += l1*(p0[1].x - p0[0].x);
		p0[0].y += l1*(p0[1].y - p0[0].y);
		l2 = in_length(p0, pn, *udisp, slant);
		draw_fpts(p0, l2, tr, fp->thick, clr);
		*p0 = p0sav;
		p0 += (l2>0) ? ((int) ceil(l2)) : 1;
	}
}


double get_clip_data(const fpolygons *u, frectangle *r)
{
	double slant = set_unslanted_y((fpolygons*)u, &r->min.y, &r->max.y);
	r->min.x = u->disp.min.x;
	r->max.x = u->disp.max.x;
	return slant;
}


void draw_fpoly(const fpolygon* fp, const transform* tr, Image* clr)
{
	frectangle r;
	double slant = get_clip_data(&univ, &r);
	draw_1fpoly(fp, tr, clr, &r, slant);
}


void eresized(int new)
{
	transform tr;
	fpolygon* fp;
	frectangle clipr;
	double slant;
	if(new && getwindow(display, Refmesg) < 0) {
		fprintf(stderr,"can't reattach to window\n");
		exits("reshap");
	}
	draw(screen, screen->r, display->white, display->opaque, screen->r.min);
	tr = draw_frame();
	slant = get_clip_data(&univ, &clipr);
	for (fp=univ.p; fp!=0; fp=fp->link)
		if (fintersects(&clipr, &fp->bb, slant))
			draw_1fpoly(fp, &tr, fp->clr, &clipr, slant);
	reselect(0);
	if (mv_bkgd!=0 && mv_bkgd->repl==0) {
		freeimage(mv_bkgd);
		mv_bkgd = display->white;
	}
	flushimage(display, 1);
}




/********************************* Recoloring *********************************/

int draw_palette(int n)		/* n is number of colors; returns patch dy */
{
	int y0 = screen->r.min.y + top_border;
	int dy = (screen->r.max.y - bot_border - y0)/n;
	Rectangle r;
	int i;
	r.min.y = y0;
	r.min.x = screen->r.max.x - rt_border + framewd;
	r.max.y = y0 + dy;
	r.max.x = screen->r.max.x;
	for (i=0; i<n; i++) {
		draw(screen, r, clrtab[i].im, display->opaque, r.min);
		r.min.y = r.max.y;
		r.max.y += dy;
	}
	return dy;
}


Image* palette_color(Point pt, int dy, int n)
{				/* mouse at pt, patch size dy, n colors */
	int yy;
	if (screen->r.max.x - pt.x > rt_border - framewd)
		return 0;
	yy = pt.y - (screen->r.min.y + top_border);
	if (yy<0 || yy>=n*dy)
		return 0;
	return clrtab[yy/dy].im;
}


void all_set_clr(fpolygons* fps, Image* clr)
{
	fpolygon* p;
	for (p=fps->p; p!=0; p=p->link)
		p->clr = clr;
}


void do_recolor(int but, Mouse* m, int alluniv)
{
	int nclr = clr_id(DWhite);
	int dy = draw_palette(nclr);
	Image* clr;
	if (!get_1click(but, m, 0)) {
		eresized(0);
		return;
	}
	clr = palette_color(m->xy, dy, nclr);
	if (clr != 0) {
		if (alluniv)
			all_set_clr(&univ, clr);
		else	cur_sel.fp->clr = clr;
	}
	eresized(0);
	lift_button(but, m, Never);
}


/****************************** Move and rotate  ******************************/

void prepare_mv(const fpolygon* fp)
{
	Rectangle r = screen->r;
	Image* scr0;
	int dt = 1 + fp->thick;
	r.min.x+=lft_border-dt;  r.min.y+=top_border-dt;
	r.max.x-=rt_border-dt;   r.max.y-=bot_border-dt;
	if (mv_bkgd!=0 && mv_bkgd->repl==0)
		freeimage(mv_bkgd);
	mv_bkgd = allocimage(display, r, CMAP8, 0, DNofill);
	if (mv_bkgd==0)
		mv_bkgd = display->white;
	else {	transform tr = cur_trans();
		draw(mv_bkgd, r, screen, display->opaque, r.min);
		draw(mv_bkgd, sel_dot_box(&tr), sel_bkg, display->opaque, ZP);
		scr0 = screen;
		screen = mv_bkgd;
		draw_fpoly(fp, &tr, display->white);
		screen = scr0;
	}
}


void move_fp(fpolygon* fp, double dx, double dy)
{
	fpoint *p, *pn=fp->p+fp->n;
	for (p=fp->p; p<=pn; p++) {
		(p->x) += dx;
		(p->y) += dy;
	}
	(fp->bb.min.x)+=dx;  (fp->bb.min.y)+=dy;
	(fp->bb.max.x)+=dx;  (fp->bb.max.y)+=dy;
}


void rotate_fp(fpolygon* fp, fpoint o, double theta)
{
	double s=sin(theta), c=cos(theta);
	fpoint *p, *pn=fp->p+fp->n;
	for (p=fp->p; p<=pn; p++) {
		double x=p->x-o.x, y=p->y-o.y;
		(p->x) = o.x + c*x - s*y;
		(p->y) = o.y + s*x + c*y;
	}
	set_fbb(fp);
}


/* Move the selected fpolygon so the selected point tracks the mouse, and return
   the total amount of movement.  Button but has already been held down for at
   least Mv_delay milliseconds and the mouse might have moved some distance.
*/
fpoint do_move(int but, Mouse* m)
{
	transform tr = cur_trans();
	int bbit = Button_bit(but);
	fpolygon* fp = cur_sel.fp;
	fpoint loc, loc0=cur_sel.p;
	double tsav = cur_sel.t;
	unselect(&tr);
	do {	latest_mouse(but, m);
		(fp->thick)++;		/* line() DISAGREES WITH ITSELF */
		draw_fpoly(fp, &tr, mv_bkgd);
		(fp->thick)--;
		do_untransform(&loc, &tr, &m->xy);
		move_fp(fp, loc.x-cur_sel.p.x, loc.y-cur_sel.p.y);
		cur_sel.p = loc;
		draw_fpoly(fp, &tr, fp->clr);
	} while (m->buttons & bbit);
	cur_sel.t = tsav;
	reselect(&tr);
	loc.x -= loc0.x;
	loc.y -= loc0.y;
	return loc;
}


double dir_angle(const Point* pt, const transform* tr)
{
	fpoint p;
	double dy, dx;
	do_untransform(&p, tr, pt);
	dy=p.y-cur_sel.p.y;  dx=p.x-cur_sel.p.x;
	return (dx==0 && dy==0) ? 0.0 : atan2(dy, dx);
}


/* Rotate the selected fpolygon around the selection point so as to track the
   direction angle from the selected point to m->xy.  Stop when button but goes
   up and return the total amount of rotation in radians.
*/
double do_rotate(int but, Mouse* m)
{
	transform tr = cur_trans();
	int bbit = Button_bit(but);
	fpolygon* fp = cur_sel.fp;
	double theta0 = dir_angle(&m->xy, &tr);
	double th, theta = theta0;
	do {	latest_mouse(but, m);
		(fp->thick)++;		/* line() DISAGREES WITH ITSELF */
		draw_fpoly(fp, &tr, mv_bkgd);
		(fp->thick)--;
		th = dir_angle(&m->xy, &tr);
		rotate_fp(fp, cur_sel.p, th-theta);
		theta = th;
		draw_fpoly(fp, &tr, fp->clr);
	} while (m->buttons & bbit);
	unselect(&tr);
	cur_sel = prev_sel;
	reselect(&tr);
	return theta - theta0;
}



/********************************* Edit menu  *********************************/

typedef enum e_index {
		Erecolor, Ethick, Edelete, Eundo, Erotate, Eoptions,
		Emove
} e_index;

char* e_items[Eoptions+1];

Menu e_menu = {e_items, 0, 0};


typedef struct e_action {
	e_index typ;			/* What type of action */
	fpolygon* fp;			/* fpolygon the action applies to */
	Image* clr;			/* color to use if typ==Erecolor */
	double amt;			/* rotation angle or line thickness */
	fpoint pt;			/* movement vector or rotation center */
	struct e_action* link;		/* next in a stack */
} e_action;

e_action* unact = 0;			/* heads a linked list of actions */
e_action* do_undo(e_action*);		/* pop off an e_action and (un)do it */
e_action* save_act(e_action*,e_index);	/* append new e_action for status quo */


void save_mv(fpoint movement)
{
	unact = save_act(unact, Emove);
	unact->pt = movement;
}


void init_e_menu(void)
{
	char* u = "can't undo";
	e_items[Erecolor] = "recolor";
	e_items[Edelete] = "delete";
	e_items[Erotate] = "rotate";
	e_items[Eoptions-cantmv] = 0;
	e_items[Ethick] = (cur_sel.fp->thick >0) ? "thin" : "thick";
	if (unact!=0)
		switch (unact->typ) {
		case Erecolor: u="uncolor"; break;
		case Ethick: u=(unact->fp->thick==0) ? "unthin" : "unthicken";
			break;
		case Edelete: u="undelete"; break;
		case Emove: u="unmove"; break;
		case Erotate: u="unrotate"; break;
		}
	e_items[Eundo] = u;
}


void do_emenu(int but, Mouse* m)
{
	int h;
	if (cur_sel.t < 0)
		return;
	init_e_menu();
	h = emenuhit(but, m, &e_menu);
	switch(h) {
	case Ethick: unact = save_act(unact, h);
		cur_sel.fp->thick ^= 1;
		eresized(0);
		break;
	case Edelete: unact = save_act(unact, h);
		fp_remove(&univ, cur_sel.fp);
		unselect(0);
		eresized(0);
		break;
	case Erecolor: unact = save_act(unact, h);
		do_recolor(but, m, 0);
		break;
	case Erotate: unact = save_act(unact, h);
		prepare_mv(cur_sel.fp);
		if (get_1click(but, m, 0)) {
			unact->pt = cur_sel.p;
			unact->amt = do_rotate(but, m);
		}
		break;
	case Eundo: unact = do_undo(unact);
		break;
	}
}



/******************************* Undoing edits  *******************************/

e_action* save_act(e_action* a0, e_index typ)
{					/* append new e_action for status quo */
	e_action* a = malloc(sizeof(e_action));
	a->link = a0;
	a->pt.x = a->pt.y = 0.0;
	a->amt = cur_sel.fp->thick;
	a->clr = cur_sel.fp->clr;
	a->fp = cur_sel.fp;
	a->typ = typ;
	return a;
}


/* This would be trivial except it's nice to preserve the selection in order to make
   it easy to undo a series of moves.  (There's no do_unrotate() because it's harder
   and less important to preserve the selection in that case.)
*/
void do_unmove(e_action* a)
{
	double tsav = cur_sel.t;
	unselect(0);
	move_fp(a->fp, -a->pt.x, -a->pt.y);
	if (a->fp == cur_sel.fp) {
		cur_sel.p.x -= a->pt.x;
		cur_sel.p.y -= a->pt.y;
	}
	cur_sel.t = tsav;
	reselect(0);
}


e_action* do_undo(e_action* a0)		/* pop off an e_action and (un)do it */
{
	e_action* a = a0;
	if (a==0)
		return 0;
	switch(a->typ) {
	case Ethick: a->fp->thick = a->amt;
		eresized(0);
		break;
	case Erecolor: a->fp->clr = a->clr;
		eresized(0);
		break;
	case Edelete:
		a->fp->link = univ.p;
		univ.p = a->fp;
		grow_bb(&univ.bb, &a->fp->bb);
		eresized(0);
		break;
	case Emove:
		do_unmove(a);
		eresized(0);
		break;
	case Erotate:
		unselect(0);
		rotate_fp(a->fp, a->pt, -a->amt);
		eresized(0);
		break;
	}
	a0 = a->link;
	free(a);
	return a0;
}



/********************************* Main menu  *********************************/

enum m_index {     Mzoom_in,  Mzoom_out,  Munzoom,  Mslant,    Munslant,
		Msquare_up,  Mrecenter,  Mrecolor,  Mrestack,  Mread,
		Mwrite,      Mexit};
char* m_items[] = {"zoom in", "zoom out", "unzoom", "slant",   "unslant",
		"square up", "recenter", "recolor", "restack", "read",
		"write",     "exit", 0};

Menu m_menu = {m_items, 0, 0};


void do_mmenu(int but, Mouse* m)
{
	int e, h = emenuhit(but, m, &m_menu);
	switch (h) {
	case Mzoom_in:
		disp_zoomin(egetrect(but,m));
		eresized(0);
		break;
	case Mzoom_out:
		disp_zoomout(egetrect(but,m));
		eresized(0);
		break;
	case Msquare_up:
		disp_squareup();
		eresized(0);
		break;
	case Munzoom:
		init_disp();
		eresized(0);
		break;
	case Mrecenter:
		if (get_1click(but, m, &bullseye)) {
			recenter_disp(m->xy);
			eresized(0);
			lift_button(but, m, Never);
		}
		break;
	case Mslant:
		if (cur_sel.t>=0 && prev_sel.t>=0) {
			slant_disp(prev_sel.p, cur_sel.p);
			eresized(0);
		}
		break;
	case Munslant:
		univ.slant_ht = univ.disp.max.y - univ.disp.min.y;
		eresized(0);
		break;
	case Mrecolor:
		do_recolor(but, m, 1);
		break;
	case Mrestack:
		fps_invert(&univ);
		eresized(0);
		break;
	case Mread:
		e = doinput(prompt_text("File:"));
		if (e==0)
			eresized(0);
		else if (e<0)
			show_mytext(" - can't read");
		else {
			char ebuf[80];
			snprintf(ebuf, 80, " - error line %d", e);
			show_mytext(ebuf);
		}
		break;
	case Mwrite:
		if (!dooutput(prompt_text("File:")))
			show_mytext(" - can't write");
		break;
	case Mexit:
		exits("");
	}
}



/****************************** Handling events  ******************************/

void doevent(void)
{
	ulong etype;
	int mobile;
	ulong mvtime;
	Event	ev;

	etype = eread(Emouse|Ekeyboard, &ev);
	if(etype & Emouse) {
		if (ev.mouse.buttons & But1) {
			do_select(ev.mouse.xy);
			mvtime = Never;
			mobile = !cantmv && cur_sel.t>=0;
			if (mobile) {
				mvtime = ev.mouse.msec + Mv_delay;
				prepare_mv(cur_sel.fp);
				if (!lift_button(1, &ev.mouse, mvtime))
					save_mv(do_move(1, &ev.mouse));
			}
		} else if (ev.mouse.buttons & But2)
			do_emenu(2, &ev.mouse);
		else if (ev.mouse.buttons & But3)
			do_mmenu(3, &ev.mouse);
	}
	/* no need to check (etype & Ekeyboard)--there are no keyboard commands */
}



/******************************** Main program ********************************/

extern char* argv0;

void usage(void)
{
	int i;
	fprintf(stderr,"Usage %s [options] [infile]\n", argv0);
	fprintf(stderr,
"option ::= -W winsize | -l logfile | -m\n"
"\n"
"Read a polygonal line graph in an ASCII format (one x y pair per line, delimited\n"
"by spaces with a label after each polyline), and view it interactively.  Use\n"
"standard input if no infile is specified.\n"
	);
	fprintf(stderr,
"Option -l specifies a file in which to log the coordinates of each point selected.\n"
"(Clicking a point with button one selects it and displays its coordinates and\n"
"the label of its polylone.)  Option -m allows polylines to be moved and rotated.\n"
"The polyline labels can use the following color names:"
	);
	for (i=0; clrtab[i].c!=DNofill; i++)
		fprintf(stderr,"%s%8s", (i%8==0 ? "\n" : "  "), clrtab[i].nam);
	fputc('\n', stderr);
	exits("usage");
}

void main(int argc, char *argv[])
{
	int e;

	ARGBEGIN {
	case 'm': cantmv=0;
		break;
	case 'l': logfil = fopen(ARGF(),"w");
		break;
	case 'W':
		winsize = EARGF(usage());
		break;
	default: usage();
	} ARGEND

	if(initdraw(0, 0, "gview") < 0)
		sysfatal("initdraw");
	einit(Emouse|Ekeyboard);

	e = doinput(*argv ? *argv : "-");
	if (e < 0) {
		fprintf(stderr,"Cannot read input file %s\n", *argv);
		exits("no valid input file");
	} else if (e > 0) {
		fprintf(stderr,"Bad syntax at line %d in input file\n", e);
		exits("bad syntax in input");
	}
	init_disp();
	init_clrtab();
	set_default_clrs(&univ, 0);
	adjust_border(display->defaultfont);
	cur_sel.t = prev_sel.t = -1;
	eresized(0);
	for(;;)
		doevent();
}
