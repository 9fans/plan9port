/* readyuv.c - read an Abekas A66 style image file.   Steve Simon, 2003 */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include <ctype.h>
#include "imagefile.h"

/*
 * ITU/CCIR Rec601 states:
 *
 * R = y + 1.402 * Cr
 * B = Y + 1.77305 * Cb
 * G = Y - 0.72414 * Cr - 0.34414 * Cb
 *
 *	using 8 bit traffic
 * Y = 16 + 219 * Y
 * Cr = 128 + 224 * Cr
 * Cb = 128 + 224 * Cb
 * 	or, if 10bit is used
 * Y = 64 + 876 * Y
 * Cr = 512 + 896 * Cr
 * Cb = 512 + 896 * Cb
 */

enum {
	PAL = 576, NTSC = 486 };


static int lsbtab[] = { 6, 4, 2, 0};

static int
clip(int x)
{
	x >>= 18;

	if (x > 255)
		return 0xff;
	if (x <= 0)
		return 0;
	return x;
}


Rawimage**
Breadyuv(Biobuf *bp, int colourspace)
{
	Dir * d;
	Rawimage * a, **array;
	char	*e, ebuf[128];
	ushort * mux, *end, *frm;
	uchar buf[720 * 2], *r, *g, *b;
	int	y1, y2, cb, cr, sz, c, l, w, base, bits, lines;

	frm = 0;
	if (colourspace != CYCbCr) {
		errstr(ebuf, sizeof ebuf);	/* throw it away */
		werrstr("ReadYUV: unknown colour space %d", colourspace);
		return nil;
	}

	if ((a = calloc(sizeof(Rawimage), 1)) == nil)
		sysfatal("no memory");

	if ((array = calloc(sizeof(Rawimage * ), 2)) == nil)
		sysfatal("no memory");
	array[0] = a;
	array[1] = nil;

	if ((d = dirfstat(Bfildes(bp))) != nil) {
		sz = d->length;
		free(d);
	} else {
		fprint(2, "cannot stat input, assuming 720x576x10bit\n");
		sz = 720 * PAL * 2L + (720 * PAL / 2L);
	}

	switch (sz) {
	case 720 * PAL * 2:				/* 625 x 8bit */
		bits = 8;
		lines = PAL;
		break;
	case 720 * NTSC * 2:				/* 525 x 8bit */
		bits = 8;
		lines = NTSC;
		break;
	case 720 * PAL * 2 + (720 * PAL / 2) :		/* 625 x 10bit */
			bits = 10;
		lines = PAL;
		break;
	case 720 * NTSC * 2 + (720 * NTSC / 2) :	/* 525 x 10bit */
			bits = 10;
		lines = NTSC;
		break;
	default:
		e = "unknown file size";
		goto Error;
	}

	/*	print("bits=%d pixels=%d lines=%d\n", bits, 720, lines); */
	/* */
	a->nchans = 3;
	a->chandesc = CRGB;
	a->chanlen = 720 * lines;
	a->r = Rect(0, 0, 720, lines);

	e = "no memory";
	if ((frm = malloc(720 * 2 * lines * sizeof(ushort))) == nil)
		goto Error;

	for (c = 0; c  < 3; c++)
		if ((a->chans[c] = malloc(720 * lines)) == nil)
			goto Error;

	e = "read file";
	for (l = 0; l < lines; l++) {
		if (Bread(bp, buf, 720 * 2) == -1)
			goto Error;

		base = l * 720 * 2;
		for (w = 0; w < 720 * 2; w++)
			frm[base + w] = ((ushort)buf[w]) << 2;
	}


	if (bits == 10)
		for (l = 0; l < lines; l++) {
			if (Bread(bp, buf, 720 / 2) == -1)
				goto Error;


			base = l * 720 * 2;
			for (w = 0; w < 720 * 2; w++)
				frm[base + w] |= buf[w / 4] >> lsbtab[w % 4];
		}

	mux = frm;
	end = frm + 720 * lines * 2;
	r = a->chans[0];
	g = a->chans[1];
	b = a->chans[2];

	while (mux < end) {
		cb = *mux++ - 512;
		y1 = (*mux++ - 64) * 76310;
		cr = *mux++ - 512;
		y2 = (*mux++ - 64) * 76310;

		*r++ = clip((104635 * cr) + y1);
		*g++ = clip((-25690 * cb + -53294 * cr) + y1);
		*b++ = clip((132278 * cb) + y1);

		*r++ = clip((104635 * cr) + y2);
		*g++ = clip((-25690 * cb + -53294 * cr) + y2);
		*b++ = clip((132278 * cb) + y2);
	}
	free(frm);
	return array;

Error:

	errstr(ebuf, sizeof ebuf);
	if (ebuf[0] == 0)
		strcpy(ebuf, e);
	errstr(ebuf, sizeof ebuf);

	for (c = 0; c < 3; c++)
		free(a->chans[c]);
	free(a->cmap);
	free(array[0]);
	free(array);
	free(frm);
	return nil;
}


Rawimage**
readyuv(int fd, int colorspace)
{
	Rawimage * *a;
	Biobuf b;

	if (Binit(&b, fd, OREAD) < 0)
		return nil;
	a = Breadyuv(&b, colorspace);
	Bterm(&b);
	return a;
}
