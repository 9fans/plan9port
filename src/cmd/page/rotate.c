/*
 * rotate an image 180° in O(log Dx + log Dy) /dev/draw writes,
 * using an extra buffer same size as the image.
 *
 * the basic concept is that you can invert an array by inverting
 * the top half, inverting the bottom half, and then swapping them.
 * the code does this slightly backwards to ensure O(log n) runtime.
 * (If you do it wrong, you can get O(log² n) runtime.)
 *
 * This is usually overkill, but it speeds up slow remote
 * connections quite a bit.
 */

#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include "page.h"

int ndraw = 0;
enum {
	Xaxis = 0,
	Yaxis = 1,
};

Image *mtmp;

void
writefile(char *name, Image *im, int gran)
{
	static int c = 100;
	int fd;
	char buf[200];

	snprint(buf, sizeof buf, "%d%s%d", c++, name, gran);
	fd = create(buf, OWRITE, 0666);
	if(fd < 0)
		return;
	writeimage(fd, im, 0);
	close(fd);
}

void
moveup(Image *im, Image *tmp, int a, int b, int c, int axis)
{
	Rectangle range;
	Rectangle dr0, dr1;
	Point p0, p1;

	if(a == b || b == c)
		return;

	drawop(tmp, tmp->r, im, nil, im->r.min, S);

	switch(axis){
	case Xaxis:
		range = Rect(a, im->r.min.y,  c, im->r.max.y);
		dr0 = range;
		dr0.max.x = dr0.min.x+(c-b);
		p0 = Pt(b, im->r.min.y);

		dr1 = range;
		dr1.min.x = dr1.max.x-(b-a);
		p1 = Pt(a, im->r.min.y);
		break;
	case Yaxis:
	default:
		range = Rect(im->r.min.x, a,  im->r.max.x, c);
		dr0 = range;
		dr0.max.y = dr0.min.y+(c-b);
		p0 = Pt(im->r.min.x, b);

		dr1 = range;
		dr1.min.y = dr1.max.y-(b-a);
		p1 = Pt(im->r.min.x, a);
		break;
	}
	drawop(im, dr0, tmp, nil, p0, S);
	drawop(im, dr1, tmp, nil, p1, S);
}

void
interlace(Image *im, Image *tmp, int axis, int n, Image *mask, int gran)
{
	Point p0, p1;
	Rectangle r0;

	r0 = im->r;
	switch(axis) {
	case Xaxis:
		r0.max.x = n;
		p0 = (Point){gran, 0};
		p1 = (Point){-gran, 0};
		break;
	case Yaxis:
	default:
		r0.max.y = n;
		p0 = (Point){0, gran};
		p1 = (Point){0, -gran};
		break;
	}

	drawop(tmp, im->r, im, display->opaque, im->r.min, S);
	gendrawop(im, r0, tmp, p0, mask, mask->r.min, S);
	gendrawop(im, r0, tmp, p1, mask, p1, S);
}

/*
 * Halve the grating period in the mask.
 * The grating currently looks like
 * ####____####____####____####____
 * where #### is opacity.
 *
 * We want
 * ##__##__##__##__##__##__##__##__
 * which is achieved by shifting the mask
 * and drawing on itself through itself.
 * Draw doesn't actually allow this, so
 * we have to copy it first.
 *
 *     ####____####____####____####____ (dst)
 * +   ____####____####____####____#### (src)
 * in  __####____####____####____####__ (mask)
 * ===========================================
 *     ##__##__##__##__##__##__##__##__
 */
int
nextmask(Image *mask, int axis, int maskdim)
{
	Point o;

	o = axis==Xaxis ? Pt(maskdim,0) : Pt(0,maskdim);
	drawop(mtmp, mtmp->r, mask, nil, mask->r.min, S);
	gendrawop(mask, mask->r, mtmp, o, mtmp, divpt(o,-2), S);
//	writefile("mask", mask, maskdim/2);
	return maskdim/2;
}

void
shuffle(Image *im, Image *tmp, int axis, int n, Image *mask, int gran,
	int lastnn)
{
	int nn, left;

	if(gran == 0)
		return;
	left = n%(2*gran);
	nn = n - left;

	interlace(im, tmp, axis, nn, mask, gran);
//	writefile("interlace", im, gran);

	gran = nextmask(mask, axis, gran);
	shuffle(im, tmp, axis, n, mask, gran, nn);
//	writefile("shuffle", im, gran);
	moveup(im, tmp, lastnn, nn, n, axis);
//	writefile("move", im, gran);
}

void
rot180(Image *im)
{
	Image *tmp, *tmp0;
	Image *mask;
	Rectangle rmask;
	int gran;

	if(chantodepth(im->chan) < 8){
		/* this speeds things up dramatically; draw is too slow on sub-byte pixel sizes */
		tmp0 = xallocimage(display, im->r, CMAP8, 0, DNofill);
		drawop(tmp0, tmp0->r, im, nil, im->r.min, S);
	}else
		tmp0 = im;

	tmp = xallocimage(display, tmp0->r, tmp0->chan, 0, DNofill);
	if(tmp == nil){
		if(tmp0 != im)
			freeimage(tmp0);
		return;
	}
	for(gran=1; gran<Dx(im->r); gran *= 2)
		;
	gran /= 4;

	rmask.min = ZP;
	rmask.max = (Point){2*gran, 100};

	mask = xallocimage(display, rmask, GREY1, 1, DTransparent);
	mtmp = xallocimage(display, rmask, GREY1, 1, DTransparent);
	if(mask == nil || mtmp == nil) {
		fprint(2, "out of memory during rot180: %r\n");
		wexits("memory");
	}
	rmask.max.x = gran;
	drawop(mask, rmask, display->opaque, nil, ZP, S);
//	writefile("mask", mask, gran);
	shuffle(im, tmp, Xaxis, Dx(im->r), mask, gran, 0);
	freeimage(mask);
	freeimage(mtmp);

	for(gran=1; gran<Dy(im->r); gran *= 2)
		;
	gran /= 4;
	rmask.max = (Point){100, 2*gran};
	mask = xallocimage(display, rmask, GREY1, 1, DTransparent);
	mtmp = xallocimage(display, rmask, GREY1, 1, DTransparent);
	if(mask == nil || mtmp == nil) {
		fprint(2, "out of memory during rot180: %r\n");
		wexits("memory");
	}
	rmask.max.y = gran;
	drawop(mask, rmask, display->opaque, nil, ZP, S);
	shuffle(im, tmp, Yaxis, Dy(im->r), mask, gran, 0);
	freeimage(mask);
	freeimage(mtmp);
	freeimage(tmp);
	if(tmp0 != im)
		freeimage(tmp0);
}

/* rotates an image 90 degrees clockwise */
Image *
rot90(Image *im)
{
	Image *tmp;
	int i, j, dx, dy;

	dx = Dx(im->r);
	dy = Dy(im->r);
	tmp = xallocimage(display, Rect(0, 0, dy, dx), im->chan, 0, DCyan);
	if(tmp == nil) {
		fprint(2, "out of memory during rot90: %r\n");
		wexits("memory");
	}

	for(j = 0; j < dx; j++) {
		for(i = 0; i < dy; i++) {
			drawop(tmp, Rect(i, j, i+1, j+1), im, nil, Pt(j, dy-(i+1)), S);
		}
	}
	freeimage(im);

	return(tmp);
}

/* rotates an image 270 degrees clockwise */
Image *
rot270(Image *im)
{
	Image *tmp;
	int i, j, dx, dy;

	dx = Dx(im->r);
	dy = Dy(im->r);
	tmp = xallocimage(display, Rect(0, 0, dy, dx), im->chan, 0, DCyan);
	if(tmp == nil) {
		fprint(2, "out of memory during rot270: %r\n");
		wexits("memory");
	}

	for(i = 0; i < dy; i++) {
		for(j = 0; j < dx; j++) {
			drawop(tmp, Rect(i, j, i+1, j+1), im, nil, Pt(dx-(j+1), i), S);
		}
	}
	freeimage(im);

	return(tmp);
}

/* from resample.c -- resize from → to using interpolation */


#define K2 7	/* from -.7 to +.7 inclusive, meaning .2 into each adjacent pixel */
#define NK (2*K2+1)
double K[NK];

double
fac(int L)
{
	int i, f;

	f = 1;
	for(i=L; i>1; --i)
		f *= i;
	return f;
}

/*
 * i0(x) is the modified Bessel function, Σ (x/2)^2L / (L!)²
 * There are faster ways to calculate this, but we precompute
 * into a table so let's keep it simple.
 */
double
i0(double x)
{
	double v;
	int L;

	v = 1.0;
	for(L=1; L<10; L++)
		v += pow(x/2., 2*L)/pow(fac(L), 2);
	return v;
}

double
kaiser(double x, double t, double a)
{
	if(fabs(x) > t)
		return 0.;
	return i0(a*sqrt(1-(x*x/(t*t))))/i0(a);
}


void
resamplex(uchar *in, int off, int d, int inx, uchar *out, int outx)
{
	int i, x, k;
	double X, xx, v, rat;


	rat = (double)inx/(double)outx;
	for(x=0; x<outx; x++){
		if(inx == outx){
			/* don't resample if size unchanged */
			out[off+x*d] = in[off+x*d];
			continue;
		}
		v = 0.0;
		X = x*rat;
		for(k=-K2; k<=K2; k++){
			xx = X + rat*k/10.;
			i = xx;
			if(i < 0)
				i = 0;
			if(i >= inx)
				i = inx-1;
			v += in[off+i*d] * K[K2+k];
		}
		out[off+x*d] = v;
	}
}

void
resampley(uchar **in, int off, int iny, uchar **out, int outy)
{
	int y, i, k;
	double Y, yy, v, rat;

	rat = (double)iny/(double)outy;
	for(y=0; y<outy; y++){
		if(iny == outy){
			/* don't resample if size unchanged */
			out[y][off] = in[y][off];
			continue;
		}
		v = 0.0;
		Y = y*rat;
		for(k=-K2; k<=K2; k++){
			yy = Y + rat*k/10.;
			i = yy;
			if(i < 0)
				i = 0;
			if(i >= iny)
				i = iny-1;
			v += in[i][off] * K[K2+k];
		}
		out[y][off] = v;
	}

}

Image*
resample(Image *from, Image *to)
{
	int i, j, bpl, nchan;
	uchar **oscan, **nscan;
	char tmp[20];
	int xsize, ysize;
	double v;
	Image *t1, *t2;
	ulong tchan;

	for(i=-K2; i<=K2; i++){
		K[K2+i] = kaiser(i/10., K2/10., 4.);
	}

	/* normalize */
	v = 0.0;
	for(i=0; i<NK; i++)
		v += K[i];
	for(i=0; i<NK; i++)
		K[i] /= v;

	switch(from->chan){
	case GREY8:
	case RGB24:
	case RGBA32:
	case ARGB32:
	case XRGB32:
		break;

	case CMAP8:
	case RGB15:
	case RGB16:
		tchan = RGB24;
		goto Convert;

	case GREY1:
	case GREY2:
	case GREY4:
		tchan = GREY8;
	Convert:
		/* use library to convert to byte-per-chan form, then convert back */
		t1 = xallocimage(display, Rect(0, 0, Dx(from->r), Dy(from->r)), tchan, 0, DNofill);
		if(t1 == nil) {
			fprint(2, "out of memory for temp image 1 in resample: %r\n");
			wexits("memory");
		}
		drawop(t1, t1->r, from, nil, ZP, S);
		t2 = xallocimage(display, to->r, tchan, 0, DNofill);
		if(t2 == nil) {
			fprint(2, "out of memory temp image 2 in resample: %r\n");
			wexits("memory");
		}
		resample(t1, t2);
		drawop(to, to->r, t2, nil, ZP, S);
		freeimage(t1);
		freeimage(t2);
		return to;

	default:
		sysfatal("can't handle channel type %s", chantostr(tmp, from->chan));
	}

	xsize = Dx(to->r);
	ysize = Dy(to->r);
	oscan = malloc(Dy(from->r)*sizeof(uchar*));
	nscan = malloc(max(ysize, Dy(from->r))*sizeof(uchar*));
	if(oscan == nil || nscan == nil)
		sysfatal("can't allocate: %r");

	/* unload original image into scan lines */
	bpl = bytesperline(from->r, from->depth);
	for(i=0; i<Dy(from->r); i++){
		oscan[i] = malloc(bpl);
		if(oscan[i] == nil)
			sysfatal("can't allocate: %r");
		j = unloadimage(from, Rect(from->r.min.x, from->r.min.y+i, from->r.max.x, from->r.min.y+i+1), oscan[i], bpl);
		if(j != bpl)
			sysfatal("unloadimage");
	}

	/* allocate scan lines for destination. we do y first, so need at least Dy(from->r) lines */
	bpl = bytesperline(Rect(0, 0, xsize, Dy(from->r)), from->depth);
	for(i=0; i<max(ysize, Dy(from->r)); i++){
		nscan[i] = malloc(bpl);
		if(nscan[i] == nil)
			sysfatal("can't allocate: %r");
	}

	/* resample in X */
	nchan = from->depth/8;
	for(i=0; i<Dy(from->r); i++){
		for(j=0; j<nchan; j++){
			if(j==0 && from->chan==XRGB32)
				continue;
			resamplex(oscan[i], j, nchan, Dx(from->r), nscan[i], xsize);
		}
		free(oscan[i]);
		oscan[i] = nscan[i];
		nscan[i] = malloc(bpl);
		if(nscan[i] == nil)
			sysfatal("can't allocate: %r");
	}

	/* resample in Y */
	for(i=0; i<xsize; i++)
		for(j=0; j<nchan; j++)
			resampley(oscan, nchan*i+j, Dy(from->r), nscan, ysize);

	/* pack data into destination */
	bpl = bytesperline(to->r, from->depth);
	for(i=0; i<ysize; i++){
		j = loadimage(to, Rect(0, i, xsize, i+1), nscan[i], bpl);
		if(j != bpl)
			sysfatal("loadimage: %r");
	}

	for(i=0; i<Dy(from->r); i++){
		free(oscan[i]);
		free(nscan[i]);
	}
	free(oscan);
	free(nscan);

	return to;
}
