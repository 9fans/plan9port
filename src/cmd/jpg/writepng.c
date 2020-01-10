/* based on PNG 1.2 specification, July 1999  (see also rfc2083) */
/* Alpha is not supported yet because of lack of industry acceptance and */
/* because Plan9 Image uses premultiplied alpha, so png can't be lossless. */
/* Only 24bit color supported, because 8bit may as well use GIF. */

#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <ctype.h>
#include <bio.h>
#include <flate.h>
#include "imagefile.h"

enum{	IDATSIZE = 	20000,
	FilterNone =	0
};

typedef struct ZlibR{
	uchar *data;
	int width;
	int nrow, ncol;
	int row, col;	/* next pixel to send */
	int pixwid;
} ZlibR;

typedef struct ZlibW{
	Biobuf *bo;
	uchar *buf;
	uchar *b;	/* next place to write */
	uchar *e;	/* past end of buf */
} ZlibW;

static uint32 *crctab;
static uchar PNGmagic[] = {137,80,78,71,13,10,26,10};

static void
put4(uchar *a, uint32 v)
{
	a[0] = v>>24;
	a[1] = v>>16;
	a[2] = v>>8;
	a[3] = v;
}

static void
chunk(Biobuf *bo, char *type, uchar *d, int n)
{
	uchar buf[4];
	uint32 crc = 0;

	if(strlen(type) != 4)
		return;
	put4(buf, n);
	Bwrite(bo, buf, 4);
	Bwrite(bo, type, 4);
	Bwrite(bo, d, n);
	crc = blockcrc(crctab, crc, type, 4);
	crc = blockcrc(crctab, crc, d, n);
	put4(buf, crc);
	Bwrite(bo, buf, 4);
}

static int
zread(void *va, void *buf, int n)
{
	ZlibR *z = va;
	int nrow = z->nrow;
	int ncol = z->ncol;
	uchar *b = buf, *e = b+n, *img;
	int pixels;  /* number of pixels in row that can be sent now */
	int i, a, pixwid;

	pixwid = z->pixwid;
	while(b+pixwid <= e){ /* loop over image rows */
		if(z->row >= nrow)
			break;
		if(z->col==0)
			*b++ = FilterNone;
		pixels = (e-b)/pixwid;
		if(pixels > ncol - z->col)
			pixels = ncol - z->col;
		img = z->data + z->width * z->row + pixwid * z->col;

		memmove(b, img, pixwid*pixels);
		if(pixwid == 4){
			/*
			 * Convert to non-premultiplied alpha.
			 */
			for(i=0; i<pixels; i++, b+=4){
				a = b[3];
				if(a == 255 || a == 0)
					;
				else{
					if(b[0] >= a)
						b[0] = a;
					b[0] = (b[0]*255)/a;
					if(b[1] >= a)
						b[1] = a;
					b[1] = (b[1]*255)/a;
					if(b[2] >= a)
						b[2] = a;
					b[2] = (b[2]*255)/a;
				}
			}
		}else
			b += pixwid*pixels;

		z->col += pixels;
		if(z->col >= ncol){
			z->col = 0;
			z->row++;
		}
	}
	return b - (uchar*)buf;
}

static void
IDAT(ZlibW *z)
{
	chunk(z->bo, "IDAT", z->buf, z->b - z->buf);
	z->b = z->buf;
}

static int
zwrite(void *va, void *buf, int n)
{
	ZlibW *z = va;
	uchar *b = buf, *e = b+n;
	int m;

	while(b < e){ /* loop over IDAT chunks */
		m = z->e - z->b;
		if(m > e - b)
			m = e - b;
		memmove(z->b, b, m);
		z->b += m;
		b += m;
		if(z->b >= z->e)
			IDAT(z);
	}
	return n;
}

static Memimage*
memRGBA(Memimage *i)
{
	Memimage *ni;
	char buf[32];
	ulong dst;

	/*
	 * [A]BGR because we want R,G,B,[A] in big-endian order.  Sigh.
	 */
	chantostr(buf, i->chan);
	if(strchr(buf, 'a'))
		dst = ABGR32;
	else
		dst = BGR24;

	if(i->chan == dst)
		return i;

	ni = allocmemimage(i->r, dst);
	if(ni == nil)
		return ni;
	memimagedraw(ni, ni->r, i, i->r.min, nil, i->r.min, S);
	return ni;
}

char*
memwritepng(Biobuf *bo, Memimage *r, ImageInfo *II)
{
	uchar buf[200], *h;
	ulong vgamma;
	int err, n;
	ZlibR zr;
	ZlibW zw;
	int nrow = r->r.max.y - r->r.min.y;
	int ncol = r->r.max.x - r->r.min.x;
	Tm *tm;
	Memimage *rgb;

	rgb = memRGBA(r);
	if(rgb == nil)
		return "allocmemimage nil";
	crctab = mkcrctab(0xedb88320);
	if(crctab == nil)
		sysfatal("mkcrctab error");
	deflateinit();

	Bwrite(bo, PNGmagic, sizeof PNGmagic);
	/* IHDR chunk */
	h = buf;
	put4(h, ncol); h += 4;
	put4(h, nrow); h += 4;
	*h++ = 8; /* bit depth = 24 bit per pixel */
	*h++ = rgb->chan==BGR24 ? 2 : 6; /* color type = rgb */
	*h++ = 0; /* compression method = deflate */
	*h++ = 0; /* filter method */
	*h++ = 0; /* interlace method = no interlace */
	chunk(bo, "IHDR", buf, h-buf);

	tm = gmtime(time(0));
	h = buf;
	*h++ = (tm->year + 1900)>>8;
	*h++ = (tm->year + 1900)&0xff;
	*h++ = tm->mon + 1;
	*h++ = tm->mday;
	*h++ = tm->hour;
	*h++ = tm->min;
	*h++ = tm->sec;
	chunk(bo, "tIME", buf, h-buf);

	if(II->fields_set & II_GAMMA){
		vgamma = II->gamma*100000;
		put4(buf, vgamma);
		chunk(bo, "gAMA", buf, 4);
	}

	if(II->fields_set & II_COMMENT){
		strncpy((char*)buf, "Comment", sizeof buf);
		n = strlen((char*)buf)+1; /* leave null between Comment and text */
		strncpy((char*)(buf+n), II->comment, sizeof buf - n);
		chunk(bo, "tEXt", buf, n+strlen((char*)buf+n));
	}

	/* image chunks */
	zr.nrow = nrow;
	zr.ncol = ncol;
	zr.width = rgb->width * sizeof(uint32);
	zr.data = rgb->data->bdata;
	zr.row = zr.col = 0;
	zr.pixwid = chantodepth(rgb->chan)/8;
	zw.bo = bo;
	zw.buf = malloc(IDATSIZE);
	zw.b = zw.buf;
	zw.e = zw.b + IDATSIZE;
	err = deflatezlib(&zw, zwrite, &zr, zread, 6, 0);
	if(zw.b > zw.buf)
		IDAT(&zw);
	free(zw.buf);
	if(err)
		sysfatal("deflatezlib %s\n", flateerr(err));
	chunk(bo, "IEND", nil, 0);

	if(r != rgb)
		freememimage(rgb);
	return nil;
}
