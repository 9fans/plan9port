#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include <event.h>
#include "imagefile.h"

int		cflag = 0;
int		dflag = 0;
int		eflag = 0;
int		jflag = 0;
int		fflag = 0;
int		Fflag = 0;
int		nineflag = 0;
int		threeflag = 0;
int		colorspace = CYCbCr;	/* default for 8-bit displays: combine color rotation with dither */
int		output = 0;
ulong	outchan = CMAP8;
Image	*image;
int		defaultcolor = 1;

enum{
	Border	= 2,
	Edge		= 5
};

char	*show(int, char*, int);

void
eresized(int new)
{
	Rectangle r;

	if(new && getwindow(display, Refnone) < 0){
		fprint(2, "jpg: can't reattach to window\n");
		exits("resize");
	}
	if(image == nil)
		return;
	r = rectaddpt(image->clipr, subpt(screen->r.min, image->clipr.min));
	if(!new && !winsize)
		drawresizewindow(r);
	draw(screen, r, image, nil, image->r.min);
	flushimage(display, 1);
}

void
usage(void)
{
	fprint(2, "usage: jpg -39cdefFkJrtv -W winsize [file.jpg ...]\n");
	exits("usage");
}

void
main(int argc, char *argv[])
{
	int fd, i, yflag;
	char *err;
	char buf[12+1];

	yflag = 0;
	ARGBEGIN{
	case 'W':
		winsize = EARGF(usage());
		break;
	case 'c':		/* produce encoded, compressed, bitmap file; no display by default */
		cflag++;
		dflag++;
		output++;
		if(defaultcolor)
			outchan = CMAP8;
		break;
	case 'd':		/* suppress display of image */
		dflag++;
		break;
	case 'e':		/* disable floyd-steinberg error diffusion */
		eflag++;
		break;
	case 'F':
		Fflag++;	/* make a movie */
		fflag++;	/* merge two fields per image */
		break;
	case 'f':
		fflag++;	/* merge two fields per image */
		break;
	case 'J':		/* decode jpeg only; no display or remap (for debugging, etc.) */
		jflag++;
		break;
	case 'k':		/* force black and white */
		defaultcolor = 0;
		outchan = GREY8;
		break;
	case 'r':
		colorspace = CRGB;
		break;
	case '3':		/* produce encoded, compressed, three-color bitmap file; no display by default */
		threeflag++;
		/* fall through */
	case 't':		/* produce encoded, compressed, true-color bitmap file; no display by default */
		cflag++;
		dflag++;
		output++;
		defaultcolor = 0;
		outchan = RGB24;
		break;
	case 'v':		/* force RGBV */
		defaultcolor = 0;
		outchan = CMAP8;
		break;
	case 'y':	/* leave it in CYCbCr; for debugging only */
		yflag = 1;
		colorspace = CYCbCr;
		break;
	case '9':		/* produce plan 9, uncompressed, bitmap file; no display by default */
		nineflag++;
		dflag++;
		output++;
		if(defaultcolor)
			outchan = CMAP8;
		break;
	default:
		usage();
	}ARGEND;

	if(yflag==0 && dflag==0 && colorspace==CYCbCr){	/* see if we should convert right to RGB */
		fd = open("/dev/screen", OREAD);
		if(fd > 0){
			buf[12] = '\0';
			if(read(fd, buf, 12)==12 && chantodepth(strtochan(buf))>8)
				colorspace = CRGB;
			close(fd);
		}
	}

	err = nil;
	if(argc == 0)
		err = show(0, "<stdin>", outchan);
	else{
		for(i=0; i<argc; i++){
			fd = open(argv[i], OREAD);
			if(fd < 0){
				fprint(2, "jpg: can't open %s: %r\n", argv[i]);
				err = "open";
			}else{
				err = show(fd, argv[i], outchan);
				close(fd);
			}
			if((nineflag || cflag) && argc>1 && err==nil){
				fprint(2, "jpg: exiting after one file\n");
				break;
			}
		}
	}
	exits(err);
}

Rawimage**
vidmerge(Rawimage **aa1, Rawimage **aa2)
{
	Rawimage **aao, *ao, *a1, *a2;
	int i, c, row, col;

	aao = nil;
	for (i = 0; aa1[i]; i++) {

		a1 = aa1[i];
		a2 = aa2[i];
		if (a2 == nil){
			fprint(2, "jpg: vidmerge: unequal lengths\n");
			return nil;
		}
		aao = realloc(aao, (i+2)*sizeof(Rawimage *));
		if (aao == nil){
			fprint(2, "jpg: vidmerge: realloc\n");
			return nil;
		}
		aao[i+1] = nil;
		ao = aao[i] = malloc(sizeof(Rawimage));
		if (ao == nil){
			fprint(2, "jpg: vidmerge: realloc\n");
			return nil;
		}
		memcpy(ao, a1, sizeof(Rawimage));
		if (!eqrect(a1->r , a2->r)){
			fprint(2, "jpg: vidmerge: rects different in img %d\n", i);
			return nil;
		}
		if (a1->cmaplen != a2->cmaplen){
			fprint(2, "jpg: vidmerge: cmaplen different in img %d\n", i);
			return nil;
		}
		if (a1->nchans != a2->nchans){
			fprint(2, "jpg: vidmerge: nchans different in img %d\n", i);
			return nil;
		}
		if (a1->fields != a2->fields){
			fprint(2, "jpg: vidmerge: fields different in img %d\n", i);
			return nil;
		}
		ao->r.max.y += Dy(ao->r);
		ao->chanlen += ao->chanlen;
		if (ao->chanlen != Dx(ao->r)*Dy(ao->r)){
			fprint(2, "jpg: vidmerge: chanlen wrong %d != %d*%d\n",
				ao->chanlen, Dx(ao->r), Dy(ao->r));
			return nil;
		}
		row = Dx(a1->r);
		for (c = 0; c < ao->nchans; c++) {
			uchar *po, *p1, *p2;

			ao->chans[c] = malloc(ao->chanlen);
			po = ao->chans[c];
			p1 = a1->chans[c];
			p2 = a2->chans[c];
			for (col = 0; col < Dy(a1->r); col++) {
				memcpy(po, p1, row);
				po += row, p1 += row;
				memcpy(po, p2, row);
				po += row, p2 += row;
			}
			free(a1->chans[c]);
			free(a2->chans[c]);
		}
		if(a2->cmap != nil)
			free(a2->cmap);
		free(a1);
		free(a2);
	}
	if (aa2[i] != nil)
		fprint(2, "jpg: vidmerge: unequal lengths\n");
	free(aa1);
	free(aa2);
	return aao;
}

char*
show(int fd, char *name, int outc)
{
	Rawimage **array, *r, *c;
	static int inited;
	Image *i;
	int j, ch, outchan;
	Biobuf b;
	char buf[32];

	if(Binit(&b, fd, OREAD) < 0)
		return nil;
	outchan = outc;
rpt:	array = Breadjpg(&b, colorspace);
	if(array == nil || array[0]==nil){
		fprint(2, "jpg: decode %s failed: %r\n", name);
		return "decode";
	}
	if (fflag) {
		Rawimage **a;

		a = Breadjpg(&b, colorspace);
		if(a == nil || a[0]==nil){
			fprint(2, "jpg: decode %s-2 failed: %r\n", name);
			return "decode";
		}
		array = vidmerge(a, array);
	} else
		Bterm(&b);

	r = array[0];
	c = nil;
	if(jflag)
		goto Return;
	if(!dflag){
		if(!inited){
			if(initdraw(0, 0, 0) < 0){
				fprint(2, "jpg: initdraw failed: %r\n");
				return "initdraw";
			}
			if(Fflag == 0)
				einit(Ekeyboard|Emouse);
		inited++;
		}
		if(defaultcolor && screen->depth>8 && outchan==CMAP8)
			outchan = RGB24;
	}
	if(outchan == CMAP8)
		c = torgbv(r, !eflag);
	else{
		if(outchan==GREY8 || (r->chandesc==CY && threeflag==0)){
			c = totruecolor(r, CY);
			outchan = GREY8;
		}else
			c = totruecolor(r, CRGB24);
	}
	if(c == nil){
		fprint(2, "jpg: conversion of %s failed: %r\n", name);
		return "torgbv";
	}
	if(!dflag){
		if(c->chandesc == CY)
			i = allocimage(display, c->r, GREY8, 0, 0);
		else
			i = allocimage(display, c->r, outchan, 0, 0);
		if(i == nil){
			fprint(2, "jpg: allocimage %s failed: %r\n", name);
			return "allocimage";
		}
		if(loadimage(i, i->r, c->chans[0], c->chanlen) < 0){
			fprint(2, "jpg: loadimage %s failed: %r\n", name);
			return "loadimage";
		}
		image = i;
		eresized(0);
		if (Fflag) {
			freeimage(i);
			for(j=0; j<r->nchans; j++)
				free(r->chans[j]);
			free(r->cmap);
			free(r);
			free(array);
			goto rpt;
		}
		if((ch=ekbd())=='q' || ch==0x7F || ch==0x04)
			exits(nil);
		draw(screen, screen->clipr, display->white, nil, ZP);
		image = nil;
		freeimage(i);
	}
	if(nineflag){
		chantostr(buf, outchan);
		print("%11s %11d %11d %11d %11d ", buf,
			c->r.min.x, c->r.min.y, c->r.max.x, c->r.max.y);
		if(write(1, c->chans[0], c->chanlen) != c->chanlen){
			fprint(2, "jpg: %s: write error %r\n", name);
			return "write";
		}
	}else if(cflag){
		if(writerawimage(1, c) < 0){
			fprint(2, "jpg: %s: write error: %r\n", name);
			return "write";
		}
	}
    Return:
	for(j=0; j<r->nchans; j++)
		free(r->chans[j]);
	free(r->cmap);
	free(r);
	free(array);
	if(c){
		free(c->chans[0]);
		free(c);
	}
	if (Fflag) goto rpt;
	return nil;
}
