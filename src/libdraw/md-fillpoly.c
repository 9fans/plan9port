#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

typedef struct Seg	Seg;

struct Seg
{
	Point	p0;
	Point	p1;
	long	num;
	long	den;
	long	dz;
	long	dzrem;
	long	z;
	long	zerr;
	long	d;
};

static	void	zsort(Seg **seg, Seg **ep);
static	int	ycompare(const void*, const void*);
static	int	xcompare(const void*, const void*);
static	int	zcompare(const void*, const void*);
static	void	xscan(Memimage *dst, Seg **seg, Seg *segtab, int nseg, int wind, Memimage *src, Point sp, int, int, int, int);
static	void	yscan(Memimage *dst, Seg **seg, Seg *segtab, int nseg, int wind, Memimage *src, Point sp, int, int);

#if 0
static void
fillcolor(Memimage *dst, int left, int right, int y, Memimage *src, Point p)
{
	int srcval;

	USED(src);
	srcval = p.x;
	p.x = left;
	p.y = y;
	memset(byteaddr(dst, p), srcval, right-left);
}
#endif

static void
fillline(Memimage *dst, int left, int right, int y, Memimage *src, Point p, int op)
{
	Rectangle r;

	r.min.x = left;
	r.min.y = y;
	r.max.x = right;
	r.max.y = y+1;
	p.x += left;
	p.y += y;
	memdraw(dst, r, src, p, memopaque, p, op);
}

static void
fillpoint(Memimage *dst, int x, int y, Memimage *src, Point p, int op)
{
	Rectangle r;

	r.min.x = x;
	r.min.y = y;
	r.max.x = x+1;
	r.max.y = y+1;
	p.x += x;
	p.y += y;
	memdraw(dst, r, src, p, memopaque, p, op);
}

void
memfillpoly(Memimage *dst, Point *vert, int nvert, int w, Memimage *src, Point sp, int op)
{
	_memfillpolysc(dst, vert, nvert, w, src, sp, 0, 0, 0, op);
}

void
_memfillpolysc(Memimage *dst, Point *vert, int nvert, int w, Memimage *src, Point sp, int detail, int fixshift, int clipped, int op)
{
	Seg **seg, *segtab;
	Point p0;
	int i;

	if(nvert == 0)
		return;

	seg = malloc((nvert+2)*sizeof(Seg*));
	if(seg == nil)
		return;
	segtab = malloc((nvert+1)*sizeof(Seg));
	if(segtab == nil) {
		free(seg);
		return;
	}

	sp.x = (sp.x - vert[0].x) >> fixshift;
	sp.y = (sp.y - vert[0].y) >> fixshift;
	p0 = vert[nvert-1];
	if(!fixshift) {
		p0.x <<= 1;
		p0.y <<= 1;
	}
	for(i = 0; i < nvert; i++) {
		segtab[i].p0 = p0;
		p0 = vert[i];
		if(!fixshift) {
			p0.x <<= 1;
			p0.y <<= 1;
		}
		segtab[i].p1 = p0;
		segtab[i].d = 1;
	}
	if(!fixshift)
		fixshift = 1;

	xscan(dst, seg, segtab, nvert, w, src, sp, detail, fixshift, clipped, op);
	if(detail)
		yscan(dst, seg, segtab, nvert, w, src, sp, fixshift, op);

	free(seg);
	free(segtab);
}

static long
mod(long x, long y)
{
	long z;

	z = x%y;
	if((long)(((ulong)z)^((ulong)y)) > 0 || z == 0)
		return z;
	return z + y;
}

static long
sdiv(long x, long y)
{
	if((long)(((ulong)x)^((ulong)y)) >= 0 || x == 0)
		return x/y;

	return (x+((y>>30)|1))/y-1;
}

static long
smuldivmod(long x, long y, long z, long *mod)
{
	vlong vx;

	if(x == 0 || y == 0){
		*mod = 0;
		return 0;
	}
	vx = x;
	vx *= y;
	*mod = vx % z;
	if(*mod < 0)
		*mod += z;	/* z is always >0 */
	if((vx < 0) == (z < 0))
		return vx/z;
	return -((-vx)/z);
}

static void
xscan(Memimage *dst, Seg **seg, Seg *segtab, int nseg, int wind, Memimage *src, Point sp, int detail, int fixshift, int clipped, int op)
{
	long y, maxy, x, x2, xerr, xden, onehalf;
	Seg **ep, **next, **p, **q, *s;
	long n, i, iy, cnt, ix, ix2, minx, maxx;
	Point pt;
	void	(*fill)(Memimage*, int, int, int, Memimage*, Point, int);

	fill = fillline;
/*
 * This can only work on 8-bit destinations, since fillcolor is
 * just using memset on sp.x.
 *
 * I'd rather not even enable it then, since then if the general
 * code is too slow, someone will come up with a better improvement
 * than this sleazy hack.	-rsc
 *
	if(clipped && (src->flags&Frepl) && src->depth==8 && Dx(src->r)==1 && Dy(src->r)==1) {
		fill = fillcolor;
		sp.x = membyteval(src);
	}
 *
 */
	USED(clipped);


	for(i=0, s=segtab, p=seg; i<nseg; i++, s++) {
		*p = s;
		if(s->p0.y == s->p1.y)
			continue;
		if(s->p0.y > s->p1.y) {
			pt = s->p0;
			s->p0 = s->p1;
			s->p1 = pt;
			s->d = -s->d;
		}
		s->num = s->p1.x - s->p0.x;
		s->den = s->p1.y - s->p0.y;
		s->dz = sdiv(s->num, s->den) << fixshift;
		s->dzrem = mod(s->num, s->den) << fixshift;
		s->dz += sdiv(s->dzrem, s->den);
		s->dzrem = mod(s->dzrem, s->den);
		p++;
	}
	n = p-seg;
	if(n == 0)
		return;
	*p = 0;
	qsort(seg, p-seg , sizeof(Seg*), ycompare);

	onehalf = 0;
	if(fixshift)
		onehalf = 1 << (fixshift-1);

	minx = dst->clipr.min.x;
	maxx = dst->clipr.max.x;

	y = seg[0]->p0.y;
	if(y < (dst->clipr.min.y << fixshift))
		y = dst->clipr.min.y << fixshift;
	iy = (y + onehalf) >> fixshift;
	y = (iy << fixshift) + onehalf;
	maxy = dst->clipr.max.y << fixshift;

	ep = next = seg;

	while(y<maxy) {
		for(q = p = seg; p < ep; p++) {
			s = *p;
			if(s->p1.y < y)
				continue;
			s->z += s->dz;
			s->zerr += s->dzrem;
			if(s->zerr >= s->den) {
				s->z++;
				s->zerr -= s->den;
				if(s->zerr < 0 || s->zerr >= s->den)
					print("bad ratzerr1: %ld den %ld dzrem %ld\n", s->zerr, s->den, s->dzrem);
			}
			*q++ = s;
		}

		for(p = next; *p; p++) {
			s = *p;
			if(s->p0.y >= y)
				break;
			if(s->p1.y < y)
				continue;
			s->z = s->p0.x;
			s->z += smuldivmod(y - s->p0.y, s->num, s->den, &s->zerr);
			if(s->zerr < 0 || s->zerr >= s->den)
				print("bad ratzerr2: %ld den %ld ratdzrem %ld\n", s->zerr, s->den, s->dzrem);
			*q++ = s;
		}
		ep = q;
		next = p;

		if(ep == seg) {
			if(*next == 0)
				break;
			iy = (next[0]->p0.y + onehalf) >> fixshift;
			y = (iy << fixshift) + onehalf;
			continue;
		}

		zsort(seg, ep);

		for(p = seg; p < ep; p++) {
			cnt = 0;
			x = p[0]->z;
			xerr = p[0]->zerr;
			xden = p[0]->den;
			ix = (x + onehalf) >> fixshift;
			if(ix >= maxx)
				break;
			if(ix < minx)
				ix = minx;
			cnt += p[0]->d;
			p++;
			for(;;) {
				if(p == ep) {
					print("xscan: fill to infinity");
					return;
				}
				cnt += p[0]->d;
				if((cnt&wind) == 0)
					break;
				p++;
			}
			x2 = p[0]->z;
			ix2 = (x2 + onehalf) >> fixshift;
			if(ix2 <= minx)
				continue;
			if(ix2 > maxx)
				ix2 = maxx;
			if(ix == ix2 && detail) {
				if(xerr*p[0]->den + p[0]->zerr*xden > p[0]->den*xden)
					x++;
				ix = (x + x2) >> (fixshift+1);
				ix2 = ix+1;
			}
			(*fill)(dst, ix, ix2, iy, src, sp, op);
		}
		y += (1<<fixshift);
		iy++;
	}
}

static void
yscan(Memimage *dst, Seg **seg, Seg *segtab, int nseg, int wind, Memimage *src, Point sp, int fixshift, int op)
{
	long x, maxx, y, y2, yerr, yden, onehalf;
	Seg **ep, **next, **p, **q, *s;
	int n, i, ix, cnt, iy, iy2, miny, maxy;
	Point pt;

	for(i=0, s=segtab, p=seg; i<nseg; i++, s++) {
		*p = s;
		if(s->p0.x == s->p1.x)
			continue;
		if(s->p0.x > s->p1.x) {
			pt = s->p0;
			s->p0 = s->p1;
			s->p1 = pt;
			s->d = -s->d;
		}
		s->num = s->p1.y - s->p0.y;
		s->den = s->p1.x - s->p0.x;
		s->dz = sdiv(s->num, s->den) << fixshift;
		s->dzrem = mod(s->num, s->den) << fixshift;
		s->dz += sdiv(s->dzrem, s->den);
		s->dzrem = mod(s->dzrem, s->den);
		p++;
	}
	n = p-seg;
	if(n == 0)
		return;
	*p = 0;
	qsort(seg, n , sizeof(Seg*), xcompare);

	onehalf = 0;
	if(fixshift)
		onehalf = 1 << (fixshift-1);

	miny = dst->clipr.min.y;
	maxy = dst->clipr.max.y;

	x = seg[0]->p0.x;
	if(x < (dst->clipr.min.x << fixshift))
		x = dst->clipr.min.x << fixshift;
	ix = (x + onehalf) >> fixshift;
	x = (ix << fixshift) + onehalf;
	maxx = dst->clipr.max.x << fixshift;

	ep = next = seg;

	while(x<maxx) {
		for(q = p = seg; p < ep; p++) {
			s = *p;
			if(s->p1.x < x)
				continue;
			s->z += s->dz;
			s->zerr += s->dzrem;
			if(s->zerr >= s->den) {
				s->z++;
				s->zerr -= s->den;
				if(s->zerr < 0 || s->zerr >= s->den)
					print("bad ratzerr1: %ld den %ld ratdzrem %ld\n", s->zerr, s->den, s->dzrem);
			}
			*q++ = s;
		}

		for(p = next; *p; p++) {
			s = *p;
			if(s->p0.x >= x)
				break;
			if(s->p1.x < x)
				continue;
			s->z = s->p0.y;
			s->z += smuldivmod(x - s->p0.x, s->num, s->den, &s->zerr);
			if(s->zerr < 0 || s->zerr >= s->den)
				print("bad ratzerr2: %ld den %ld ratdzrem %ld\n", s->zerr, s->den, s->dzrem);
			*q++ = s;
		}
		ep = q;
		next = p;

		if(ep == seg) {
			if(*next == 0)
				break;
			ix = (next[0]->p0.x + onehalf) >> fixshift;
			x = (ix << fixshift) + onehalf;
			continue;
		}

		zsort(seg, ep);

		for(p = seg; p < ep; p++) {
			cnt = 0;
			y = p[0]->z;
			yerr = p[0]->zerr;
			yden = p[0]->den;
			iy = (y + onehalf) >> fixshift;
			if(iy >= maxy)
				break;
			if(iy < miny)
				iy = miny;
			cnt += p[0]->d;
			p++;
			for(;;) {
				if(p == ep) {
					print("yscan: fill to infinity");
					return;
				}
				cnt += p[0]->d;
				if((cnt&wind) == 0)
					break;
				p++;
			}
			y2 = p[0]->z;
			iy2 = (y2 + onehalf) >> fixshift;
			if(iy2 <= miny)
				continue;
			if(iy2 > maxy)
				iy2 = maxy;
			if(iy == iy2) {
				if(yerr*p[0]->den + p[0]->zerr*yden > p[0]->den*yden)
					y++;
				iy = (y + y2) >> (fixshift+1);
				fillpoint(dst, ix, iy, src, sp, op);
			}
		}
		x += (1<<fixshift);
		ix++;
	}
}

static void
zsort(Seg **seg, Seg **ep)
{
	int done;
	Seg **q, **p, *s;

	if(ep-seg < 20) {
		/* bubble sort by z - they should be almost sorted already */
		q = ep;
		do {
			done = 1;
			q--;
			for(p = seg; p < q; p++) {
				if(p[0]->z > p[1]->z) {
					s = p[0];
					p[0] = p[1];
					p[1] = s;
					done = 0;
				}
			}
		} while(!done);
	} else {
		q = ep-1;
		for(p = seg; p < q; p++) {
			if(p[0]->z > p[1]->z) {
				qsort(seg, ep-seg, sizeof(Seg*), zcompare);
				break;
			}
		}
	}
}

static int
ycompare(const void *a, const void *b)
{
	Seg **s0, **s1;
	long y0, y1;

	s0 = (Seg**)a;
	s1 = (Seg**)b;
	y0 = (*s0)->p0.y;
	y1 = (*s1)->p0.y;

	if(y0 < y1)
		return -1;
	if(y0 == y1)
		return 0;
	return 1;
}

static int
xcompare(const void *a, const void *b)
{
	Seg **s0, **s1;
	long x0, x1;

	s0 = (Seg**)a;
	s1 = (Seg**)b;
	x0 = (*s0)->p0.x;
	x1 = (*s1)->p0.x;

	if(x0 < x1)
		return -1;
	if(x0 == x1)
		return 0;
	return 1;
}

static int
zcompare(const void *a, const void *b)
{
	Seg **s0, **s1;
	long z0, z1;

	s0 = (Seg**)a;
	s1 = (Seg**)b;
	z0 = (*s0)->z;
	z1 = (*s1)->z;

	if(z0 < z1)
		return -1;
	if(z0 == z1)
		return 0;
	return 1;
}
