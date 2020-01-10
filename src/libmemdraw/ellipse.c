#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

/*
 * ellipse(dst, c, a, b, t, src, sp)
 *   draws an ellipse centered at c with semiaxes a,b>=0
 *   and semithickness t>=0, or filled if t<0.  point sp
 *   in src maps to c in dst
 *
 *   very thick skinny ellipses are brushed with circles (slow)
 *   others are approximated by filling between 2 ellipses
 *   criterion for very thick when b<a: t/b > 0.5*x/(1-x)
 *   where x = b/a
 */

typedef struct Param	Param;
typedef struct State	State;

static	void	bellipse(int, State*, Param*);
static	void	erect(int, int, int, int, Param*);
static	void	eline(int, int, int, int, Param*);

struct Param {
	Memimage	*dst;
	Memimage	*src;
	Point			c;
	int			t;
	Point			sp;
	Memimage	*disc;
	int			op;
};

/*
 * denote residual error by e(x,y) = b^2*x^2 + a^2*y^2 - a^2*b^2
 * e(x,y) = 0 on ellipse, e(x,y) < 0 inside, e(x,y) > 0 outside
 */

struct State {
	int	a;
	int	x;
	vlong	a2;	/* a^2 */
	vlong	b2;	/* b^2 */
	vlong	b2x;	/* b^2 * x */
	vlong	a2y;	/* a^2 * y */
	vlong	c1;
	vlong	c2;	/* test criteria */
	vlong	ee;	/* ee = e(x+1/2,y-1/2) - (a^2+b^2)/4 */
	vlong	dxe;
	vlong	dye;
	vlong	d2xe;
	vlong	d2ye;
};

static
State*
newstate(State *s, int a, int b)
{
	s->x = 0;
	s->a = a;
	s->a2 = (vlong)(a*a);
	s->b2 = (vlong)(b*b);
	s->b2x = (vlong)0;
	s->a2y = s->a2*(vlong)b;
	s->c1 = -((s->a2>>2) + (vlong)(a&1) + s->b2);
	s->c2 = -((s->b2>>2) + (vlong)(b&1));
	s->ee = -s->a2y;
	s->dxe = (vlong)0;
	s->dye = s->ee<<1;
	s->d2xe = s->b2<<1;
	s->d2ye = s->a2<<1;
	return s;
}

/*
 * return x coord of rightmost pixel on next scan line
 */
static
int
step(State *s)
{
	while(s->x < s->a) {
		if(s->ee+s->b2x <= s->c1 ||	/* e(x+1,y-1/2) <= 0 */
		   s->ee+s->a2y <= s->c2) {	/* e(x+1/2,y) <= 0 (rare) */
			s->dxe += s->d2xe;
			s->ee += s->dxe;
			s->b2x += s->b2;
			s->x++;
			continue;
		}
		s->dye += s->d2ye;
		s->ee += s->dye;
		s->a2y -= s->a2;
		if(s->ee-s->a2y <= s->c2) {	/* e(x+1/2,y-1) <= 0 */
			s->dxe += s->d2xe;
			s->ee += s->dxe;
			s->b2x += s->b2;
			return s->x++;
		}
		break;
	}
	return s->x;
}

void
memellipse(Memimage *dst, Point c, int a, int b, int t, Memimage *src, Point sp, int op)
{
	State in, out;
	int y, inb, inx, outx, u;
	Param p;

	if(a < 0)
		a = -a;
	if(b < 0)
		b = -b;
	p.dst = dst;
	p.src = src;
	p.c = c;
	p.t = t;
	p.sp = subpt(sp, c);
	p.disc = nil;
	p.op = op;

	u = (t<<1)*(a-b);
	if(b<a && u>b*b || a<b && -u>a*a) {
/*	if(b<a&&(t<<1)>b*b/a || a<b&&(t<<1)>a*a/b)	# very thick */
		bellipse(b, newstate(&in, a, b), &p);
		return;
	}

	if(t < 0) {
		inb = -1;
		newstate(&out, a, y = b);
	} else {
		inb = b - t;
		newstate(&out, a+t, y = b+t);
	}
	if(t > 0)
		newstate(&in, a-t, inb);
	inx = 0;
	for( ; y>=0; y--) {
		outx = step(&out);
		if(y > inb) {
			erect(-outx, y, outx, y, &p);
			if(y != 0)
				erect(-outx, -y, outx, -y, &p);
			continue;
		}
		if(t > 0) {
			inx = step(&in);
			if(y == inb)
				inx = 0;
		} else if(inx > outx)
			inx = outx;
		erect(inx, y, outx, y, &p);
		if(y != 0)
			erect(inx, -y, outx, -y, &p);
		erect(-outx, y, -inx, y, &p);
		if(y != 0)
			erect(-outx, -y, -inx, -y, &p);
		inx = outx + 1;
	}
}

static Point p00 = {0, 0};

/*
 * a brushed ellipse
 */
static
void
bellipse(int y, State *s, Param *p)
{
	int t, ox, oy, x, nx;

	t = p->t;
	p->disc = allocmemimage(Rect(-t,-t,t+1,t+1), GREY1);
	if(p->disc == nil)
		return;
	memfillcolor(p->disc, DTransparent);
	memellipse(p->disc, p00, t, t, -1, memopaque, p00, p->op);
	oy = y;
	ox = 0;
	nx = x = step(s);
	do {
		while(nx==x && y-->0)
			nx = step(s);
		y++;
		eline(-x,-oy,-ox, -y, p);
		eline(ox,-oy,  x, -y, p);
		eline(-x,  y,-ox, oy, p);
		eline(ox,  y,  x, oy, p);
		ox = x+1;
		x = nx;
		y--;
		oy = y;
	} while(oy > 0);
}

/*
 * a rectangle with closed (not half-open) coordinates expressed
 * relative to the center of the ellipse
 */
static
void
erect(int x0, int y0, int x1, int y1, Param *p)
{
	Rectangle r;

/*	print("R %d,%d %d,%d\n", x0, y0, x1, y1); */
	r = Rect(p->c.x+x0, p->c.y+y0, p->c.x+x1+1, p->c.y+y1+1);
	memdraw(p->dst, r, p->src, addpt(p->sp, r.min), memopaque, p00, p->op);
}

/*
 * a brushed point similarly specified
 */
static
void
epoint(int x, int y, Param *p)
{
	Point p0;
	Rectangle r;

/*	print("P%d %d,%d\n", p->t, x, y);	*/
	p0 = Pt(p->c.x+x, p->c.y+y);
	r = Rpt(addpt(p0, p->disc->r.min), addpt(p0, p->disc->r.max));
	memdraw(p->dst, r, p->src, addpt(p->sp, r.min), p->disc, p->disc->r.min, p->op);
}

/*
 * a brushed horizontal or vertical line similarly specified
 */
static
void
eline(int x0, int y0, int x1, int y1, Param *p)
{
/*	print("L%d %d,%d %d,%d\n", p->t, x0, y0, x1, y1); */
	if(x1 > x0+1)
		erect(x0+1, y0-p->t, x1-1, y1+p->t, p);
	else if(y1 > y0+1)
		erect(x0-p->t, y0+1, x1+p->t, y1-1, p);
	epoint(x0, y0, p);
	if(x1-x0 || y1-y0)
		epoint(x1, y1, p);
}
