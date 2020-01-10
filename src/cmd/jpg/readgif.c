#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include "imagefile.h"

typedef struct Entry Entry;
typedef struct Header Header;

struct Entry{
	int		prefix;
	int		exten;
};


struct Header{
	Biobuf	*fd;
	char		err[256];
	jmp_buf	errlab;
	uchar 	buf[3*256];
	char 		vers[8];
	uchar 	*globalcmap;
	int		screenw;
	int		screenh;
	int		fields;
	int		bgrnd;
	int		aspect;
	int		flags;
	int		delay;
	int		trindex;
	int		loopcount;
	Entry	tbl[4096];
	Rawimage	**array;
	Rawimage	*new;

	uchar	*pic;
};

static char		readerr[] = "ReadGIF: read error: %r";
static char		extreaderr[] = "ReadGIF: can't read extension: %r";
static char		memerr[] = "ReadGIF: malloc failed: %r";

static Rawimage**	readarray(Header*);
static Rawimage*	readone(Header*);
static void			readheader(Header*);
static void			skipextension(Header*);
static uchar*		readcmap(Header*, int);
static uchar*		decode(Header*, Rawimage*, Entry*);
static void			interlace(Header*, Rawimage*);

static
void
clear(void *pp)
{
	void **p = (void**)pp;

	if(*p){
		free(*p);
		*p = nil;
	}
}

static
void
giffreeall(Header *h, int freeimage)
{
	int i;

	if(h->fd){
		Bterm(h->fd);
		h->fd = nil;
	}
	clear(&h->pic);
	if(h->new){
		clear(&h->new->cmap);
		clear(&h->new->chans[0]);
		clear(&h->new);
	}
	clear(&h->globalcmap);
	if(freeimage && h->array!=nil){
		for(i=0; h->array[i]; i++){
			clear(&h->array[i]->cmap);
			clear(&h->array[i]->chans[0]);
		}
		clear(&h->array);
	}
}

static
void
giferror(Header *h, char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	vseprint(h->err, h->err+sizeof h->err, fmt, arg);
	va_end(arg);

	werrstr(h->err);
	giffreeall(h, 1);
	longjmp(h->errlab, 1);
}


Rawimage**
readgif(int fd, int colorspace)
{
	Rawimage **a;
	Biobuf b;
	Header *h;
	char buf[ERRMAX];

	buf[0] = '\0';
	USED(colorspace);
	if(Binit(&b, fd, OREAD) < 0)
		return nil;
	h = malloc(sizeof(Header));
	if(h == nil){
		Bterm(&b);
		return nil;
	}
	memset(h, 0, sizeof(Header));
	h->fd = &b;
	errstr(buf, sizeof buf);	/* throw it away */
	if(setjmp(h->errlab))
		a = nil;
	else
		a = readarray(h);
	giffreeall(h, 0);
	free(h);
	return a;
}

static
void
inittbl(Header *h)
{
	int i;
	Entry *tbl;

	tbl = h->tbl;
	for(i=0; i<258; i++) {
		tbl[i].prefix = -1;
		tbl[i].exten = i;
	}
}

static
Rawimage**
readarray(Header *h)
{
	Entry *tbl;
	Rawimage *new, **array;
	int c, nimages;

	tbl = h->tbl;

	readheader(h);

	if(h->fields & 0x80)
		h->globalcmap = readcmap(h, (h->fields&7)+1);

	array = malloc(sizeof(Rawimage**));
	if(array == nil)
		giferror(h, memerr);
	nimages = 0;
	array[0] = nil;
	h->array = array;

	for(;;){
		switch(c = Bgetc(h->fd)){
		case Beof:
			goto Return;

		case 0x21:	/* Extension (ignored) */
			skipextension(h);
			break;

		case 0x2C:	/* Image Descriptor */
			inittbl(h);
			new = readone(h);
			if(new->fields & 0x80){
				new->cmaplen = 3*(1<<((new->fields&7)+1));
				new->cmap = readcmap(h, (new->fields&7)+1);
			}else{
				new->cmaplen = 3*(1<<((h->fields&7)+1));
				new->cmap = malloc(new->cmaplen);
				memmove(new->cmap, h->globalcmap, new->cmaplen);
			}
			h->new = new;
			new->chans[0] = decode(h, new, tbl);
			if(new->fields & 0x40)
				interlace(h, new);
			new->gifflags = h->flags;
			new->gifdelay = h->delay;
			new->giftrindex = h->trindex;
			new->gifloopcount = h->loopcount;
			array = realloc(h->array, (nimages+2)*sizeof(Rawimage*));
			if(array == nil)
				giferror(h, memerr);
			array[nimages++] = new;
			array[nimages] = nil;
			h->array = array;
			h->new = nil;
			break;

		case 0x3B:	/* Trailer */
			goto Return;

		default:
			fprint(2, "ReadGIF: unknown block type: 0x%.2x\n", c);
			goto Return;
		}
	}

   Return:
	if(array[0]==nil || array[0]->chans[0] == nil)
		giferror(h, "ReadGIF: no picture in file");

	return array;
}

static
void
readheader(Header *h)
{
	if(Bread(h->fd, h->buf, 13) != 13)
		giferror(h, "ReadGIF: can't read header: %r");
	memmove(h->vers, h->buf, 6);
	if(strcmp(h->vers, "GIF87a")!=0 &&  strcmp(h->vers, "GIF89a")!=0)
		giferror(h, "ReadGIF: can't recognize format %s", h->vers);
	h->screenw = h->buf[6]+(h->buf[7]<<8);
	h->screenh = h->buf[8]+(h->buf[9]<<8);
	h->fields = h->buf[10];
	h->bgrnd = h->buf[11];
	h->aspect = h->buf[12];
	h->flags = 0;
	h->delay = 0;
	h->trindex = 0;
	h->loopcount = -1;
}

static
uchar*
readcmap(Header *h, int size)
{
	uchar *map;

	if(size > 8)
		giferror(h, "ReadGIF: can't handles %d bits per pixel", size);
	size = 3*(1<<size);
	if(Bread(h->fd, h->buf, size) != size)
		giferror(h, "ReadGIF: short read on color map");
	map = malloc(size);
	if(map == nil)
		giferror(h, memerr);
	memmove(map, h->buf, size);
	return map;
}

static
Rawimage*
readone(Header *h)
{
	Rawimage *i;
	int left, top, width, height;

	if(Bread(h->fd, h->buf, 9) != 9)
		giferror(h, "ReadGIF: can't read image descriptor: %r");
	i = malloc(sizeof(Rawimage));
	if(i == nil)
		giferror(h, memerr);
	left = h->buf[0]+(h->buf[1]<<8);
	top = h->buf[2]+(h->buf[3]<<8);
	width = h->buf[4]+(h->buf[5]<<8);
	height = h->buf[6]+(h->buf[7]<<8);
	i->fields = h->buf[8];
	i->r.min.x = left;
	i->r.min.y = top;
	i->r.max.x = left+width;
	i->r.max.y = top+height;
	i->nchans = 1;
	i->chandesc = CRGB1;
	return i;
}


static
int
readdata(Header *h, uchar *data)
{
	int nbytes, n;

	nbytes = Bgetc(h->fd);
	if(nbytes < 0)
		giferror(h, "ReadGIF: can't read data: %r");
	if(nbytes == 0)
		return 0;
	n = Bread(h->fd, data, nbytes);
	if(n < 0)
		giferror(h, "ReadGIF: can't read data: %r");
	if(n != nbytes)
		fprint(2, "ReadGIF: short data subblock\n");
	return n;
}

static
void
graphiccontrol(Header *h)
{
	if(Bread(h->fd, h->buf, 5+1) != 5+1)
		giferror(h, readerr);
	h->flags = h->buf[1];
	h->delay = h->buf[2]+(h->buf[3]<<8);
	h->trindex = h->buf[4];
}

static
void
skipextension(Header *h)
{
	int type, hsize, hasdata, n;
	uchar data[256];

	hsize = 0;
	hasdata = 0;

	type = Bgetc(h->fd);
	switch(type){
	case Beof:
		giferror(h, extreaderr);
		break;
	case 0x01:	/* Plain Text Extension */
		hsize = 13;
		hasdata = 1;
		break;
	case 0xF9:	/* Graphic Control Extension */
		graphiccontrol(h);
		return;
	case 0xFE:	/* Comment Extension */
		hasdata = 1;
		break;
	case 0xFF:	/* Application Extension */
		hsize = Bgetc(h->fd);
		/* standard says this must be 11, but Adobe likes to put out 10-byte ones,
		 * so we pay attention to the field. */
		hasdata = 1;
		break;
	default:
		giferror(h, "ReadGIF: unknown extension");
	}
	if(hsize>0 && Bread(h->fd, h->buf, hsize) != hsize)
		giferror(h, extreaderr);
	if(!hasdata)
		return;

	/* loop counter: Application Extension with NETSCAPE2.0 as string and 1 <loop.count> in data */
	if(type == 0xFF && hsize==11 && memcmp(h->buf, "NETSCAPE2.0", 11)==0){
		n = readdata(h, data);
		if(n == 0)
			return;
		if(n==3 && data[0]==1)
			h->loopcount = data[1] | (data[2]<<8);
	}
	while(readdata(h, data) != 0)
		;
}

static
uchar*
decode(Header *h, Rawimage *i, Entry *tbl)
{
	int c, incode, codesize, CTM, EOD, pici, datai, stacki, nbits, sreg, fc, code, piclen;
	int csize, nentry, maxentry, first, ocode, ndata, nb;
	uchar *pic;
	uchar stack[4096], data[256];

	if(Bread(h->fd, h->buf, 1) != 1)
		giferror(h, "ReadGIF: can't read data: %r");
	codesize = h->buf[0];
	if(codesize>8 || 0>codesize)
		giferror(h, "ReadGIF: can't handle codesize %d", codesize);
	if(i->cmap!=nil && i->cmaplen!=3*(1<<codesize)
	  && (codesize!=2 || i->cmaplen!=3*2)) /* peculiar GIF bitmap files... */
		giferror(h, "ReadGIF: codesize %d doesn't match color map 3*%d", codesize, i->cmaplen/3);

	CTM =1<<codesize;
	EOD = CTM+1;

	piclen = (i->r.max.x-i->r.min.x)*(i->r.max.y-i->r.min.y);
	i->chanlen = piclen;
	pic = malloc(piclen);
	if(pic == nil)
		giferror(h, memerr);
	h->pic = pic;
	pici = 0;
	ndata = 0;
	datai = 0;
	nbits = 0;
	sreg = 0;
	fc = 0;

    Loop:
	for(;;){
		csize = codesize+1;
		nentry = EOD+1;
		maxentry = (1<<csize)-1;
		first = 1;
		ocode = -1;

		for(;; ocode = incode) {
			while(nbits < csize) {
				if(datai == ndata){
					ndata = readdata(h, data);
					if(ndata == 0)
						goto Return;
					datai = 0;
				}
				c = data[datai++];
				sreg |= c<<nbits;
				nbits += 8;
			}
			code = sreg & ((1<<csize) - 1);
			sreg >>= csize;
			nbits -= csize;

			if(code == EOD){
				ndata = readdata(h, data);
				if(ndata != 0)
					fprint(2, "ReadGIF: unexpected data past EOD");
				goto Return;
			}

			if(code == CTM)
				goto Loop;

			stacki = (sizeof stack)-1;

			incode = code;

			/* special case for KwKwK */
			if(code == nentry) {
				stack[stacki--] = fc;
				code = ocode;
			}

			if(code > nentry)
				giferror(h, "ReadGIF: bad code %x %x", code, nentry);

			for(c=code; c>=0; c=tbl[c].prefix)
				stack[stacki--] = tbl[c].exten;

			nb = (sizeof stack)-(stacki+1);
			if(pici+nb > piclen){
				/* this common error is harmless
				 * we have to keep reading to keep the blocks in sync */
				;
			}else{
				memmove(pic+pici, stack+stacki+1, sizeof stack - (stacki+1));
				pici += nb;
			}

			fc = stack[stacki+1];

			if(first){
				first = 0;
				continue;
			}
			#define early 0 /* peculiar tiff feature here for reference */
			if(nentry == maxentry-early) {
				if(csize >= 12)
					continue;
				csize++;
				maxentry = (1<<csize);
				if(csize < 12)
					maxentry--;
			}
			tbl[nentry].prefix = ocode;
			tbl[nentry].exten = fc;
			nentry++;
		}
	}

Return:
	h->pic = nil;
	return pic;
}

static
void
interlace(Header *h, Rawimage *image)
{
	uchar *pic;
	Rectangle r;
	int dx, yy, y;
	uchar *ipic;

	pic = image->chans[0];
	r = image->r;
	dx = r.max.x-r.min.x;
	ipic = malloc(dx*(r.max.y-r.min.y));
	if(ipic == nil)
		giferror(h, nil);

	/* Group 1: every 8th row, starting with row 0 */
	yy = 0;
	for(y=r.min.y; y<r.max.y; y+=8){
		memmove(&ipic[(y-r.min.y)*dx], &pic[yy*dx], dx);
		yy++;
	}

	/* Group 2: every 8th row, starting with row 4 */
	for(y=r.min.y+4; y<r.max.y; y+=8){
		memmove(&ipic[(y-r.min.y)*dx], &pic[yy*dx], dx);
		yy++;
	}

	/* Group 3: every 4th row, starting with row 2 */
	for(y=r.min.y+2; y<r.max.y; y+=4){
		memmove(&ipic[(y-r.min.y)*dx], &pic[yy*dx], dx);
		yy++;
	}

	/* Group 4: every 2nd row, starting with row 1 */
	for(y=r.min.y+1; y<r.max.y; y+=2){
		memmove(&ipic[(y-r.min.y)*dx], &pic[yy*dx], dx);
		yy++;
	}

	free(image->chans[0]);
	image->chans[0] = ipic;
}
