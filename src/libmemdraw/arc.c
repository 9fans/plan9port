#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

/*
 * elarc(dst,c,a,b,t,src,sp,alpha,phi)
 *   draws the part of an ellipse between rays at angles alpha and alpha+phi
 *   measured counterclockwise from the positive x axis. other
 *   arguments are as for ellipse(dst,c,a,b,t,src,sp)
 */

enum
{
	R, T, L, B	/* right, top, left, bottom */
};

static
Point corners[] = {
	{1,1},
	{-1,1},
	{-1,-1},
	{1,-1}
};

static
Point p00;

/*
 * make a "wedge" mask covering the desired angle and contained in
 * a surrounding square; draw a full ellipse; intersect that with the
 * wedge to make a mask through which to copy src to dst.
 */
void
memarc(Memimage *dst, Point c, int a, int b, int t, Memimage *src, Point sp, int alpha, int phi, int op)
{
	int i, w, beta, tmp, c1, c2, m, m1;
	Rectangle rect;
	Point p,	bnd[8];
	Memimage *wedge, *figure, *mask;

	if(a < 0)
		a = -a;
	if(b < 0)
		b = -b;
	w = t;
	if(w < 0)
		w = 0;
	alpha = -alpha;		/* compensate for upside-down coords */
	phi = -phi;
	beta = alpha + phi;
	if(phi < 0){
		tmp = alpha;
		alpha = beta;
		beta = tmp;
		phi = -phi;
	}
	if(phi >= 360){
		memellipse(dst, c, a, b, t, src, sp, op);
		return;
	}
	while(alpha < 0)
		alpha += 360;
	while(beta < 0)
		beta += 360;
	c1 = alpha/90 & 3;	/* number of nearest corner */
	c2 = beta/90 & 3;
		/*
		 * icossin returns point at radius ICOSSCALE.
		 * multiplying by m1 moves it outside the ellipse
		*/
	rect = Rect(-a-w, -b-w, a+w+1, b+w+1);
	m = rect.max.x;	/* inradius of bounding square */
	if(m < rect.max.y)
		m = rect.max.y;
	m1 = (m+ICOSSCALE-1) >> 10;
	m = m1 << 10;		/* assure m1*cossin is inside */
	i = 0;
	bnd[i++] = Pt(0,0);
	icossin(alpha, &p.x, &p.y);
	bnd[i++] = mulpt(p, m1);
	for(;;) {
		bnd[i++] = mulpt(corners[c1], m);
		if(c1==c2 && phi<180)
			break;
		c1 = (c1+1) & 3;
		phi -= 90;
	}
	icossin(beta, &p.x, &p.y);
	bnd[i++] = mulpt(p, m1);

	figure = nil;
	mask = nil;
	wedge = allocmemimage(rect, GREY1);
	if(wedge == nil)
		goto Return;
	memfillcolor(wedge, DTransparent);
	memfillpoly(wedge, bnd, i, ~0, memopaque, p00, S);
	figure = allocmemimage(rect, GREY1);
	if(figure == nil)
		goto Return;
	memfillcolor(figure, DTransparent);
	memellipse(figure, p00, a, b, t, memopaque, p00, S);
	mask = allocmemimage(rect, GREY1);
	if(mask == nil)
		goto Return;
	memfillcolor(mask, DTransparent);
	memimagedraw(mask, rect, figure, rect.min, wedge, rect.min, S);
	c = subpt(c, dst->r.min);
	memdraw(dst, dst->r, src, subpt(sp, c), mask, subpt(p00, c), op);

    Return:
	freememimage(wedge);
	freememimage(figure);
	freememimage(mask);
}
