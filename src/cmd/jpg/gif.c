#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include <event.h>
#include "imagefile.h"

int		cflag = 0;
int		dflag = 0;
int		eflag = 0;
int		nineflag = 0;
int		threeflag = 0;
int		output = 0;
ulong	outchan = CMAP8;
Image	**allims;
Image	**allmasks;
int		which;
int		defaultcolor = 1;

enum{
	Border	= 2,
	Edge		= 5
};

char	*show(int, char*);

Rectangle
imager(void)
{
	Rectangle r;

	if(allims==nil || allims[0]==nil)
		return screen->r;
	r = insetrect(screen->clipr, Edge+Border);
	r.max.x = r.min.x+Dx(allims[0]->r);
	r.max.y = r.min.y+Dy(allims[0]->r);
	return r;
}

void
eresized(int new)
{
	Rectangle r;

	if(new && getwindow(display, Refnone) < 0){
		fprint(2, "gif: can't reattach to window\n");
		exits("resize");
	}
	if(allims==nil || allims[which]==nil)
		return;
	r = imager();
	border(screen, r, -Border, nil, ZP);
	r.min.x += allims[which]->r.min.x - allims[0]->r.min.x;
	r.min.y += allims[which]->r.min.y - allims[0]->r.min.y;
	drawop(screen, r, allims[which], allmasks[which], allims[which]->r.min, S);
	flushimage(display, 1);
}

void
main(int argc, char *argv[])
{
	int fd, i;
	char *err;

	ARGBEGIN{
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
	case 'k':		/* force black and white */
		defaultcolor = 0;
		outchan = GREY8;
		break;
	case 'v':		/* force RGBV */
		defaultcolor = 0;
		outchan = CMAP8;
		break;
	case '9':		/* produce plan 9, uncompressed, bitmap file; no display by default */
		nineflag++;
		dflag++;
		output++;
		if(defaultcolor)
			outchan = CMAP8;
		break;
	default:
		fprint(2, "usage: gif -39cdektv  [file.gif ...]\n");
		exits("usage");
	}ARGEND;

	err = nil;
	if(argc == 0)
		err = show(0, "<stdin>");
	else{
		for(i=0; i<argc; i++){
			fd = open(argv[i], OREAD);
			if(fd < 0){
				fprint(2, "gif: can't open %s: %r\n", argv[i]);
				err = "open";
			}else{
				err = show(fd, argv[i]);
				close(fd);
			}
			if(output && argc>1 && err==nil){
				fprint(2, "gif: exiting after one file\n");
				break;
			}
		}
	}
	exits(err);
}

Image*
transparency(Rawimage *r, char *name)
{
	Image *i;
	int j, index;
	uchar *pic, *mpic, *mask;

	if((r->gifflags&TRANSP) == 0)
		return nil;
	i = allocimage(display, r->r, GREY8, 0, 0);
	if(i == nil){
		fprint(2, "gif: allocimage for mask of %s failed: %r\n", name);
		return nil;
	}
	pic = r->chans[0];
	mask = malloc(r->chanlen);
	if(mask == nil){
		fprint(2, "gif: malloc for mask of %s failed: %r\n", name);
		freeimage(i);
		return nil;
	}
	index = r->giftrindex;
	mpic = mask;
	for(j=0; j<r->chanlen; j++)
		if(*pic++ == index)
			*mpic++ = 0;
		else
			*mpic++ = 0xFF;
	if(loadimage(i, i->r, mask, r->chanlen) < 0){
		fprint(2, "gif: loadimage for mask of %s failed: %r\n", name);
		free(mask);
		freeimage(i);
		return nil;
	}
	free(mask);
	return i;
}

/* interleave alpha values of 0xFF in data stream. alpha value comes first, then b g r */
uchar*
expand(uchar *u, int chanlen, int nchan)
{
	int j, k;
	uchar *v, *up, *vp;

	v = malloc(chanlen*(nchan+1));
	if(v == nil){
		fprint(2, "gif: malloc fails: %r\n");
		exits("malloc");
	}
	up = u;
	vp = v;
	for(j=0; j<chanlen; j++){
		*vp++ = 0xFF;
		for(k=0; k<nchan; k++)
			*vp++ = *up++;
	}
	return v;
}

void
addalpha(Rawimage *i)
{
	char buf[32];

	switch(outchan){
	case CMAP8:
		i->chans[0] = expand(i->chans[0], i->chanlen/1, 1);
		i->chanlen = 2*(i->chanlen/1);
		i->chandesc = CRGBVA16;
		outchan = CHAN2(CMap, 8, CAlpha, 8);
		break;

	case GREY8:
		i->chans[0] = expand(i->chans[0], i->chanlen/1, 1);
		i->chanlen = 2*(i->chanlen/1);
		i->chandesc = CYA16;
		outchan = CHAN2(CGrey, 8, CAlpha, 8);
		break;

	case RGB24:
		i->chans[0] = expand(i->chans[0], i->chanlen/3, 3);
		i->chanlen = 4*(i->chanlen/3);
		i->chandesc = CRGBA32;
		outchan = RGBA32;
		break;

	default:
		chantostr(buf, outchan);
		fprint(2, "gif: can't add alpha to type %s\n", buf);
		exits("err");
	}
}

/*
 * Called only when writing output.  If the output is RGBA32,
 * we must write four bytes per pixel instead of two.
 * There's always at least two: data plus alpha.
 * r is used only for reference; the image is already in c.
 */
void
whiteout(Rawimage *r, Rawimage *c)
{
	int i, trindex;
	uchar *rp, *cp;

	rp = r->chans[0];
	cp = c->chans[0];
	trindex = r->giftrindex;
	if(outchan == RGBA32)
		for(i=0; i<r->chanlen; i++){
			if(*rp == trindex){
				*cp++ = 0x00;
				*cp++ = 0xFF;
				*cp++ = 0xFF;
				*cp++ = 0xFF;
			}else{
				*cp++ = 0xFF;
				cp += 3;
			}
			rp++;
		}
	else
		for(i=0; i<r->chanlen; i++){
			if(*rp == trindex){
				*cp++ = 0x00;
				*cp++ = 0xFF;
			}else{
				*cp++ = 0xFF;
				cp++;
			}
			rp++;
		}
}

int
init(void)
{
	static int inited;

	if(inited == 0){
		if(initdraw(0, 0, 0) < 0){
			fprint(2, "gif: initdraw failed: %r\n");
			return -1;
		}
		einit(Ekeyboard|Emouse);
		inited++;
	}
	return 1;
}

char*
show(int fd, char *name)
{
	Rawimage **images, **rgbv;
	Image **ims, **masks;
	int j, k, n, ch, nloop, loopcount, dt;
	char *err;
	char buf[32];

	err = nil;
	images = readgif(fd, CRGB);
	if(images == nil){
		fprint(2, "gif: decode %s failed: %r\n", name);
		return "decode";
	}
	for(n=0; images[n]; n++)
		;
	ims = malloc((n+1)*sizeof(Image*));
	masks = malloc((n+1)*sizeof(Image*));
	rgbv = malloc((n+1)*sizeof(Rawimage*));
	if(masks==nil || rgbv==nil || ims==nil){
		fprint(2, "gif: malloc of masks for %s failed: %r\n", name);
		err = "malloc";
		goto Return;
	}
	memset(masks, 0, (n+1)*sizeof(Image*));
	memset(ims, 0, (n+1)*sizeof(Image*));
	memset(rgbv, 0, (n+1)*sizeof(Rawimage*));
	if(!dflag){
		if(init() < 0){
			err = "initdraw";
			goto Return;
		}
		if(defaultcolor && screen->depth>8)
			outchan = RGB24;
	}

	for(k=0; k<n; k++){
		if(outchan == CMAP8)
			rgbv[k] = torgbv(images[k], !eflag);
		else{
			if(outchan==GREY8 || (images[k]->chandesc==CY && threeflag==0))
				rgbv[k] = totruecolor(images[k], CY);
			else
				rgbv[k] = totruecolor(images[k], CRGB24);
		}
		if(rgbv[k] == nil){
			fprint(2, "gif: converting %s to local format failed: %r\n", name);
			err = "torgbv";
			goto Return;
		}
		if(!dflag){
			masks[k] = transparency(images[k], name);
			if(rgbv[k]->chandesc == CY)
				ims[k] = allocimage(display, rgbv[k]->r, GREY8, 0, 0);
			else
				ims[k] = allocimage(display, rgbv[k]->r, outchan, 0, 0);
			if(ims[k] == nil){
				fprint(2, "gif: allocimage %s failed: %r\n", name);
				err = "allocimage";
				goto Return;
			}
			if(loadimage(ims[k], ims[k]->r, rgbv[k]->chans[0], rgbv[k]->chanlen) < 0){
				fprint(2, "gif: loadimage %s failed: %r\n", name);
				err = "loadimage";
				goto Return;
			}
		}
	}

	allims = ims;
	allmasks = masks;
	loopcount = images[0]->gifloopcount;
	if(!dflag){
		nloop = 0;
		do{
			for(k=0; k<n; k++){
				which = k;
				eresized(0);
				dt = images[k]->gifdelay*10;
				if(dt < 50)
					dt = 50;
				while(n==1 || ecankbd()){
					if((ch=ekbd())=='q' || ch==0x7F || ch==0x04)	/* an odd, democratic list */
						exits(nil);
					if(ch == '\n')
						goto Out;
				}sleep(dt);
			}
			/* loopcount -1 means no loop (this code's rule), loopcount 0 means loop forever (netscape's rule)*/
		}while(loopcount==0 || ++nloop<loopcount);
		/* loop count has run out */
		ekbd();
    Out:
		drawop(screen, screen->clipr, display->white, nil, ZP, S);
	}
	if(n>1 && output)
		fprint(2, "gif: warning: only writing first image in %d-image GIF %s\n", n, name);
	if(nineflag){
		if(images[0]->gifflags&TRANSP){
			addalpha(rgbv[0]);
			whiteout(images[0], rgbv[0]);
		}
		chantostr(buf, outchan);
		print("%11s %11d %11d %11d %11d ", buf,
			rgbv[0]->r.min.x, rgbv[0]->r.min.y, rgbv[0]->r.max.x, rgbv[0]->r.max.y);
		if(write(1, rgbv[0]->chans[0], rgbv[0]->chanlen) != rgbv[0]->chanlen){
			fprint(2, "gif: %s: write error %r\n", name);
			return "write";
		}
	}else if(cflag){
		if(images[0]->gifflags&TRANSP){
			addalpha(rgbv[0]);
			whiteout(images[0], rgbv[0]);
		}
		if(writerawimage(1, rgbv[0]) < 0){
			fprint(2, "gif: %s: write error: %r\n", name);
			return "write";
		}
	}

    Return:
	allims = nil;
	allmasks = nil;
	for(k=0; images[k]; k++){
		for(j=0; j<images[k]->nchans; j++)
			free(images[k]->chans[j]);
		free(images[k]->cmap);
		if(rgbv[k])
			free(rgbv[k]->chans[0]);
		freeimage(ims[k]);
		freeimage(masks[k]);
		free(images[k]);
		free(rgbv[k]);
	}
	free(images);
	free(masks);
	free(ims);
	return err;
}
