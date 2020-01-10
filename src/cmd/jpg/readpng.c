#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <bio.h>
#include <flate.h>
#include <draw.h>
#include "imagefile.h"

int debug;

enum{  IDATSIZE=1000000,
	/* filtering algorithms, supposedly increase compression */
	FilterNone =	0,	/* new[x][y] = buf[x][y] */
	FilterSub	=	1,	/* new[x][y] = buf[x][y] + new[x-1][y] */
	FilterUp	=	2,	/* new[x][y] = buf[x][y] + new[x][y-1] */
	FilterAvg	=	3,	/* new[x][y] = buf[x][y] + (new[x-1][y]+new[x][y-1])/2 */
	FilterPaeth=	4,	/* new[x][y] = buf[x][y] + paeth(new[x-1][y],new[x][y-1],new[x-1][y-1]) */
	FilterLast	=	5,
	PropertyBit =	1<<5
};


typedef struct ZlibW{
	uchar *chan[4]; /* Rawimage channels */
	uchar *scan;	/* new scanline */
	uchar *pscan;	/* previous scanline */
	int scanl;		/* scan len */
	int scanp;		/* scan pos */
	int nchan;		/* number of input chans */
	int npix;		/* pixels read so far */
	int	chanl;		/* number of bytes allocated to chan[x] */
	int scanpix;
	int bpp;		/* bits per sample */
	int palsize;
	int row;		/* current scanline number */
	uchar palette[3*256];
} ZlibW;

typedef struct ZlibR{
	Biobuf *bi;
	uchar *buf;
	uchar *b;	/* next byte to decompress */
	uchar *e;	/* past end of buf */
	ZlibW *w;
} ZlibR;

static uint32 *crctab;
static uchar PNGmagic[] = {137,80,78,71,13,10,26,10};
static char memerr[] = "ReadPNG: malloc failed: %r";

static uint32
get4(uchar *a)
{
	return (a[0]<<24) | (a[1]<<16) | (a[2]<<8) | a[3];
}

static
void
pnginit(void)
{
	static int inited;

	if(inited)
		return;
	inited = 1;
	crctab = mkcrctab(0xedb88320);
	if(crctab == nil)
		sysfatal("mkcrctab error");
	inflateinit();
}

static
void*
pngmalloc(ulong n, int clear)
{
	void *p;

	p = malloc(n);
	if(p == nil)
		sysfatal(memerr);
	if(clear)
		memset(p, 0, n);
	return p;
}

static int
getchunk(Biobuf *b, char *type, uchar *d, int m)
{
	uchar buf[8];
	uint32 crc = 0, crc2;
	int n, nr;

	if(Bread(b, buf, 8) != 8)
		return -1;
	n = get4(buf);
	memmove(type, buf+4, 4);
	type[4] = 0;
	if(n > m)
		sysfatal("getchunk needed %d, had %d", n, m);
	nr = Bread(b, d, n);
	if(nr != n)
		sysfatal("getchunk read %d, expected %d", nr, n);
	crc = blockcrc(crctab, crc, type, 4);
	crc = blockcrc(crctab, crc, d, n);
	if(Bread(b, buf, 4) != 4)
		sysfatal("getchunk tlr failed");
	crc2 = get4(buf);
	if(crc != crc2)
		sysfatal("getchunk crc failed");
	return n;
}

static int
zread(void *va)
{
	ZlibR *z = va;
	char type[5];
	int n;

	if(z->b >= z->e){
refill_buffer:
		z->b = z->buf;
		n = getchunk(z->bi, type, z->b, IDATSIZE);
		if(n < 0 || strcmp(type, "IEND") == 0)
			return -1;
		z->e = z->b + n;
		if(!strcmp(type,"PLTE")) {
			if (n < 3 || n > 3*256 || n%3)
				sysfatal("invalid PLTE chunk len %d", n);
			memcpy(z->w->palette, z->b, n);
			z->w->palsize = n/3;
			goto refill_buffer;
		}
		if(type[0] & PropertyBit)
			goto refill_buffer;  /* skip auxiliary chunks for now */
		if(strcmp(type,"IDAT")) {
			sysfatal("unrecognized mandatory chunk %s", type);
			goto refill_buffer;
		}
	}
	return *z->b++;
}

static uchar
paeth(uchar a, uchar b, uchar c)
{
	int p, pa, pb, pc;

	p = (int)a + (int)b - (int)c;
	pa = abs(p - (int)a);
	pb = abs(p - (int)b);
	pc = abs(p - (int)c);

	if(pa <= pb && pa <= pc)
		return a;
	else if(pb <= pc)
		return b;
	return c;
}

static void
unfilter(int alg, uchar *buf, uchar *up, int len, int bypp)
{
	int i;
	switch(alg){
	case FilterNone:
		break;

	case FilterSub:
		for (i = bypp; i < len; ++i)
			buf[i] += buf[i-bypp];
		break;

	case FilterUp:
		for (i = 0; i < len; ++i)
			buf[i] += up[i];
		break;

	case FilterAvg:
		for (i = 0; i < bypp; ++i)
			buf[i] += (0+up[i])/2;
		for (; i < len; ++i)
			buf[i] += (buf[i-bypp]+up[i])/2;
		break;

	case FilterPaeth:
		for (i = 0; i < bypp; ++i)
			buf[i] += paeth(0, up[i], 0);
		for (; i < len; ++i)
			buf[i] += paeth(buf[i-bypp], up[i], up[i-bypp]);
		break;
	default:
		sysfatal("unknown filtering scheme %d\n", alg);
	}
}

static void
convertpix(ZlibW *z, uchar *pixel, uchar *r, uchar *g, uchar *b)
{
	int off;
	switch (z->nchan) {
	case 1:	/* gray or indexed */
	case 2:	/* gray+alpha */
		if (z->bpp < 8)
			pixel[0] >>= 8-z->bpp;
		if (pixel[0] > z->palsize)
			sysfatal("index %d out of bounds %d", pixel[0], z->palsize);
		off = 3*pixel[0];
		*r = z->palette[off];
		*g = z->palette[off+1];
		*b = z->palette[off+2];
		break;
	case 3:	/* rgb */
	case 4:	/* rgb+alpha */
		*r = pixel[0];
		*g = pixel[1];
		*b = pixel[2];
		break;
	default:
		sysfatal("bad number of channels: %d", z->nchan);
	}
}

static void
scan(ZlibW *z)
{
	uchar *p;
	int i, bit, n, ch, nch, pd;
	uchar cb;
	uchar pixel[4];

	p = z->scan;
	nch = z->nchan;

	unfilter(p[0], p+1, z->pscan+1, z->scanl-1, (nch*z->bpp+7)/8);
/*
 *	Adam7 interlace order.
 *	1 6 4 6 2 6 4 6
 *	7 7 7 7 7 7 7 7
 *	5 6 5 6 5 6 5 6
 *	7 7 7 7 7 7 7 7
 *	3 6 4 6 3 6 4 6
 *	7 7 7 7 7 7 7 7
 *	5 6 5 6 5 6 5 6
 *	7 7 7 7 7 7 7 7
 */
	ch = 0;
	n = 0;
	cb = 128;
	pd = z->row * z->scanpix;
	for (i = 1; i < z->scanl; ++i)
		for (bit = 128; bit > 0; bit /= 2) {

			pixel[ch] &= ~cb;
			if (p[i] & bit)
				pixel[ch] |= cb;

			cb >>= 1;

			if (++n == z->bpp) {
				cb = 128;
				n = 0;
				ch++;
			}
			if (ch == nch) {
				if (z->npix++ < z->chanl)
					convertpix(z,pixel,z->chan[0]+pd,z->chan[1]+pd,z->chan[2]+pd);
				pd++;
				if (pd % z->scanpix == 0)
					goto out;
				ch = 0;
			}
		}
out: ;
}

static int
zwrite(void *va, void *vb, int n)
{
	ZlibW *z = va;
	uchar *buf = vb;
	int i, j;

	j = z->scanp;
	for (i = 0; i < n; ++i) {
		z->scan[j++] = buf[i];
		if (j == z->scanl) {
			uchar *tp;
			scan(z);

			tp = z->scan;
			z->scan = z->pscan;
			z->pscan = tp;
			z->row++;
			j = 0;
		}
	}
	z->scanp = j;

	return n;
}

static Rawimage*
readslave(Biobuf *b)
{
	ZlibR zr;
	ZlibW zw;
	Rawimage *image;
	char type[5];
	uchar *buf, *h;
	int k, n, nrow, ncol, err, bpp, nch;

	zr.w = &zw;

	buf = pngmalloc(IDATSIZE, 0);
	Bread(b, buf, sizeof PNGmagic);
	if(memcmp(PNGmagic, buf, sizeof PNGmagic) != 0)
		sysfatal("bad PNGmagic");

	n = getchunk(b, type, buf, IDATSIZE);
	if(n < 13 || strcmp(type,"IHDR") != 0)
		sysfatal("missing IHDR chunk");
	h = buf;
	ncol = get4(h);  h += 4;
	nrow = get4(h);  h += 4;
	if(ncol <= 0 || nrow <= 0)
		sysfatal("impossible image size nrow=%d ncol=%d", nrow, ncol);
	if(debug)
		fprint(2, "readpng nrow=%d ncol=%d\n", nrow, ncol);

	bpp = *h++;
	nch = 0;
	switch (*h++) {
	case 0:	/* grey */
		nch = 1;
		break;
	case 2:	/* rgb */
		nch = 3;
		break;
	case 3: /* indexed rgb with PLTE */
		nch = 1;
		break;
	case 4:	/* grey+alpha */
		nch = 2;
		break;
	case 6:	/* rgb+alpha */
		nch = 4;
		break;
	default:
		sysfatal("unsupported color scheme %d", h[-1]);
	}

	/* generate default palette for grayscale */
	zw.palsize = 256;
	if (nch < 3 && bpp < 9)
		zw.palsize = 1<<bpp;
	for (k = 0; k < zw.palsize; ++k) {
		zw.palette[3*k] = (k*255)/(zw.palsize-1);
		zw.palette[3*k+1] = (k*255)/(zw.palsize-1);
		zw.palette[3*k+2] = (k*255)/(zw.palsize-1);
	}

	if(*h++ != 0)
		sysfatal("only deflate supported for now [%d]", h[-1]);
	if(*h++ != FilterNone)
		sysfatal("only FilterNone supported for now [%d]", h[-1]);
	if(*h != 0)
		sysfatal("only non-interlaced supported for now [%d]", h[-1]);

	image = pngmalloc(sizeof(Rawimage), 1);
	image->r = Rect(0, 0, ncol, nrow);
	image->cmap = nil;
	image->cmaplen = 0;
	image->chanlen = ncol*nrow;
	image->fields = 0;
	image->gifflags = 0;
	image->gifdelay = 0;
	image->giftrindex = 0;
	image->chandesc = CRGB;
	image->nchans = 3;

	zw.chanl = ncol*nrow;
	zw.npix = 0;
	for(k=0; k<4; k++)
		image->chans[k] = zw.chan[k] = pngmalloc(ncol*nrow, 1);

	zr.bi = b;
	zr.buf = buf;
	zr.b = zr.e = buf + IDATSIZE;

	zw.scanp = 0;
	zw.row = 0;
	zw.scanpix = ncol;
	zw.scanl = (nch*ncol*bpp+7)/8+1;
	zw.scan = pngmalloc(zw.scanl, 1);
	zw.pscan = pngmalloc(zw.scanl, 1);
	zw.nchan = nch;
	zw.bpp = bpp;

	err = inflatezlib(&zw, zwrite, &zr, zread);

	if (zw.npix > zw.chanl)
		fprint(2, "tried to overflow by %d pix\n", zw.npix - zw.chanl);


	if(err)
		sysfatal("inflatezlib %s\n", flateerr(err));

	free(image->chans[3]);
	image->chans[3] = nil;
	free(buf);
	free(zw.scan);
	free(zw.pscan);
	return image;
}

Rawimage**
Breadpng(Biobuf *b, int colorspace)
{
	Rawimage *r, **array;
	char buf[ERRMAX];

	buf[0] = '\0';
	if(colorspace != CRGB){
		errstr(buf, sizeof buf);	/* throw it away */
		werrstr("ReadPNG: unknown color space %d", colorspace);
		return nil;
	}
	pnginit();
	array = malloc(2*sizeof(*array));
	if(array==nil)
		return nil;
	errstr(buf, sizeof buf);	/* throw it away */
	r = readslave(b);
	array[0] = r;
	array[1] = nil;
	return array;
}

Rawimage**
readpng(int fd, int colorspace)
{
	Rawimage** a;
	Biobuf b;

	if(Binit(&b, fd, OREAD) < 0)
		return nil;
	a = Breadpng(&b, colorspace);
	Bterm(&b);
	return a;
}
