/*
 * Rotate an image 180° in O(log Dx + log Dy)
 * draw calls, using an extra buffer the same size
 * as the image.
 *
 * The basic concept is that you can invert an array by
 * inverting the top half, inverting the bottom half, and
 * then swapping them.
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
	Xaxis,
	Yaxis,
};

static void reverse(Image*, Image*, int);
static void shuffle(Image*, Image*, int, int, Image*, int, int);
static void writefile(char *name, Image *im, int gran);
static void halvemaskdim(Image*);
static void swapranges(Image*, Image*, int, int, int, int);

/*
 * Rotate the image 180° by reflecting first
 * along the X axis, and then along the Y axis.
 */
void
rot180(Image *img)
{
	Image *tmp;

	tmp = xallocimage(display, img->r, img->chan, 0, DNofill);
	if(tmp == nil)
		return;

	reverse(img, tmp, Xaxis);
	reverse(img, tmp, Yaxis);

	freeimage(tmp);
}

Image *mtmp;

static void
reverse(Image *img, Image *tmp, int axis)
{
	Image *mask;
	Rectangle r;
	int i, d;

	/*
	 * We start by swapping large chunks at a time.
	 * The chunk size should be the largest power of
	 * two that fits in the dimension.
	 */
	d = axis==Xaxis ? Dx(img) : Dy(img);
	for(i = 1; i*2 <= d; i *= 2)
		;

	r = axis==Xaxis ? Rect(0,0, i,100) : Rect(0,0, 100,i);
	mask = xallocimage(display, r, GREY1, 1, DTransparent);
	mtmp = xallocimage(display, r, GREY1, 1, DTransparent);

	/*
	 * Now color the bottom (or left) half of the mask opaque.
	 */
	if(axis==Xaxis)
		r.max.x /= 2;
	else
		r.max.y /= 2;

	draw(mask, r, display->opaque, nil, ZP);
	writefile("mask", mask, i);

	/*
	 * Shuffle will recur, shuffling the pieces as necessary
	 * and making the mask a finer and finer grating.
	 */
	shuffle(img, tmp, axis, d, mask, i, 0);

	freeimage(mask);
}

/*
 * Shuffle the image by swapping pieces of size maskdim.
 */
static void
shuffle(Image *img, Image *tmp, int axis, int imgdim, Image *mask, int maskdim)
{
	int slop;

	if(maskdim == 0)
		return;

	/*
	 * Figure out how much will be left over that needs to be
	 * shifted specially to the bottom.
	 */
	slop = imgdim % maskdim;

	/*
	 * Swap adjacent grating lines as per mask.
	 */
	swapadjacent(img, tmp, axis, imgdim - slop, mask, maskdim);

	/*
	 * Calculate the mask with gratings half as wide and recur.
	 */
	halvemaskdim(mask, maskdim, axis);
	writefile("mask", mask, maskdim/2);

	shuffle(img, tmp, axis, imgdim, mask, maskdim/2);

	/*
	 * Move the slop down to the bottom of the image.
	 */
	swapranges(img, tmp, 0, imgdim-slop, imgdim, axis);
	moveup(im, tmp, lastnn, nn, n, axis);
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
static void
halvemaskdim(Image *m, int maskdim, int axis)
{
	Point δ;

	δ = axis==Xaxis ? Pt(maskdim,0) : Pt(0,maskdim);
	draw(mtmp, mtmp->r, mask, nil, mask->r.min);
	gendraw(mask, mask->r, mtmp, δ, mtmp, divpt(δ,2));
	writefile("mask", mask, maskdim/2);
}

/*
 * Swap the regions [a,b] and [b,c]
 */
static void
swapranges(Image *img, Image *tmp, int a, int b, int c, int axis)
{
	Rectangle r;
	Point δ;

	if(a == b || b == c)
		return;

	writefile("swap", img, 0);
	draw(tmp, tmp->r, im, nil, im->r.min);

	/* [a,a+(c-b)] gets [b,c] */
	r = img->r;
	if(axis==Xaxis){
		δ = Pt(1,0);
		r.min.x = img->r.min.x + a;
		r.max.x = img->r.min.x + a + (c-b);
	}else{
		δ = Pt(0,1);
		r.min.y = img->r.min.y + a;
		r.max.y = img->r.min.y + a + (c-b);
	}
	draw(img, r, tmp, nil, addpt(tmp->r.min, mulpt(δ, b)));

	/* [a+(c-b), c] gets [a,b] */
	r = img->r;
	if(axis==Xaxis){
		r.min.x = img->r.min.x + a + (c-b);
		r.max.x = img->r.min.x + c;
	}else{
		r.min.y = img->r.min.y + a + (c-b);
		r.max.y = img->r.min.y + c;
	}
	draw(img, r, tmp, nil, addpt(tmp->r.min, mulpt(δ, a)));
	writefile("swap", img, 1);
}

/*
 * Swap adjacent regions as specified by the grating.
 * We do this by copying the image through the mask twice,
 * once aligned with the grading and once 180° out of phase.
 */
static void
swapadjacent(Image *img, Image *tmp, int axis, int imgdim, Image *mask, int maskdim)
{
	Point δ;
	Rectangle r0, r1;

	δ = axis==Xaxis ? Pt(1,0) : Pt(0,1);

	r0 = img->r;
	r1 = img->r;
	switch(axis){
	case Xaxis:
		r0.max.x = imgdim;
		r1.min.x = imgdim;
		break;
	case Yaxis:
		r0.max.y = imgdim;
		r1.min.y = imgdim;
	}

	/*
	 * r0 is the lower rectangle, while r1 is the upper one.
	 */
	draw(tmp, tmp->r, img, nil,
}

void
interlace(Image *im, Image *tmp, int axis, int n, Image *mask, int gran)
{
	Point p0, p1;
	Rectangle r0, r1;

	r0 = im->r;
	r1 = im->r;
	switch(axis) {
	case Xaxis:
		r0.max.x = n;
		r1.min.x = n;
		p0 = (Point){gran, 0};
		p1 = (Point){-gran, 0};
		break;
	case Yaxis:
		r0.max.y = n;
		r1.min.y = n;
		p0 = (Point){0, gran};
		p1 = (Point){0, -gran};
		break;
	}

	draw(tmp, im->r, im, display->black, im->r.min);
	gendraw(im, r0, tmp, p0, mask, mask->r.min);
	gendraw(im, r0, tmp, p1, mask, p1);
}


static void
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
