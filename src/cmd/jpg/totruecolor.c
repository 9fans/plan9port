#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include "imagefile.h"

enum {
	c1 = 2871,	/* 1.402 * 2048 */
	c2 = 705,		/* 0.34414 * 2048 */
	c3 = 1463,	/* 0.71414 * 2048 */
	c4 = 3629,	/* 1.772 * 2048 */
};

Rawimage*
totruecolor(Rawimage *i, int chandesc)
{
	int j, k;
	Rawimage *im;
	char err[ERRMAX];
	uchar *rp, *gp, *bp, *cmap, *inp, *outp, cmap1[3*256];
	int r, g, b, Y, Cr, Cb;

	if(chandesc!=CY && chandesc!=CRGB24)
		return _remaperror("remap: can't convert to chandesc %d", chandesc);

	err[0] = '\0';
	errstr(err, sizeof err);	/* throw it away */
	im = malloc(sizeof(Rawimage));
	if(im == nil)
		return nil;
	memset(im, 0, sizeof(Rawimage));
	if(chandesc == CY)
		im->chanlen = i->chanlen;
	else
		im->chanlen = 3*i->chanlen;
	im->chandesc = chandesc;
	im->chans[0] = malloc(im->chanlen);
	if(im->chans[0] == nil){
		free(im);
		return nil;
	}
	im->r = i->r;
	im->nchans = 1;

	cmap = i->cmap;

	outp = im->chans[0];

	switch(i->chandesc){
	default:
		return _remaperror("remap: can't recognize channel type %d", i->chandesc);
	case CY:
		if(i->nchans != 1)
			return _remaperror("remap: Y image has %d chans", i->nchans);
		if(chandesc == CY){
			memmove(im->chans[0], i->chans[0], i->chanlen);
			break;
		}
		/* convert to three color */
		inp = i->chans[0];
		for(j=0; j<i->chanlen; j++){
			k = *inp++;
			*outp++ = k;
			*outp++ = k;
			*outp++ = k;
		}
		break;

	case CRGB1:
		if(cmap == nil)
			return _remaperror("remap: image has no color map");
		if(i->nchans != 1)
			return _remaperror("remap: can't handle nchans %d", i->nchans);
		for(j=1; j<=8; j++)
			if(i->cmaplen == 3*(1<<j))
				break;
		if(j > 8)
			return _remaperror("remap: can't do colormap size 3*%d", i->cmaplen/3);
		if(i->cmaplen != 3*256){
			/* to avoid a range check in loop below, make a full-size cmap */
			memmove(cmap1, cmap, i->cmaplen);
			cmap = cmap1;
		}
		inp = i->chans[0];
		if(chandesc == CY){
			for(j=0; j<i->chanlen; j++){
				k = *inp++;
				r = cmap[3*k+2];
				g = cmap[3*k+1];
				b = cmap[3*k+0];
				r = (2125*r + 7154*g + 721*b)/10000;	/* Poynton page 84 */
				*outp++ = r;
			}
		}else{
			for(j=0; j<i->chanlen; j++){
				k = *inp++;
				*outp++ = cmap[3*k+2];
				*outp++ = cmap[3*k+1];
				*outp++ = cmap[3*k+0];
			}
		}
		break;

	case CRGB:
		if(i->nchans != 3)
			return _remaperror("remap: can't handle nchans %d", i->nchans);
		rp = i->chans[0];
		gp = i->chans[1];
		bp = i->chans[2];
		if(chandesc == CY){
			for(j=0; j<i->chanlen; j++){
				r = *bp++;
				g = *gp++;
				b = *rp++;
				r = (2125*r + 7154*g + 721*b)/10000;	/* Poynton page 84 */
				*outp++ = r;
			}
		}else
			for(j=0; j<i->chanlen; j++){
				*outp++ = *bp++;
				*outp++ = *gp++;
				*outp++ = *rp++;
			}
		break;

	case CYCbCr:
		if(i->nchans != 3)
			return _remaperror("remap: can't handle nchans %d", i->nchans);
		rp = i->chans[0];
		gp = i->chans[1];
		bp = i->chans[2];
		for(j=0; j<i->chanlen; j++){
			Y = *rp++ << 11;
			Cb = *gp++ - 128;
			Cr = *bp++ - 128;
			r = (Y+c1*Cr) >> 11;
			g = (Y-c2*Cb-c3*Cr) >> 11;
			b = (Y+c4*Cb) >> 11;
			if(r < 0)
				r = 0;
			if(r > 255)
				r = 255;
			if(g < 0)
				g = 0;
			if(g > 255)
				g = 255;
			if(b < 0)
				b = 0;
			if(b > 255)
				b = 255;
			if(chandesc == CY){
				r = (2125*r + 7154*g + 721*b)/10000;
				*outp++ = r;
			}else{
				*outp++ = b;
				*outp++ = g;
				*outp++ = r;
			}
		}
		break;
	}
	return im;
}
