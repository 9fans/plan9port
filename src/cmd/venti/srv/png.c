#include "stdinc.h"
#include "dat.h"
#include "fns.h"

enum
{
	IDATSIZE	= 20000,
	FilterNone = 0
};

typedef struct ZlibR ZlibR;
typedef struct ZlibW ZlibW;

struct ZlibR
{
	uchar *data;
	int width;
	int dx;
	int dy;
	int x;
	int y;
	int pixwid;
};

struct ZlibW
{
	Hio *io;
	uchar *buf;
	uchar *b;
	uchar *e;
};

static u32int *crctab;
static uchar PNGmagic[] = { 137, 'P', 'N', 'G', '\r', '\n', 26, '\n'};

static void
put4(uchar *a, ulong v)
{
	a[0] = v>>24;
	a[1] = v>>16;
	a[2] = v>>8;
	a[3] = v;
}

static void
chunk(Hio *io, char *type, uchar *d, int n)
{
	uchar buf[4];
	ulong crc = 0;

	if(strlen(type) != 4)
		return;
	put4(buf, n);
	hwrite(io, buf, 4);
	hwrite(io, type, 4);
	hwrite(io, d, n);
	crc = blockcrc(crctab, crc, type, 4);
	crc = blockcrc(crctab, crc, d, n);
	put4(buf, crc);
	hwrite(io, buf, 4);
}

static int
zread(void *va, void *buf, int n)
{
	int a, i, pixels, pixwid;
	uchar *b, *e, *img;
	ZlibR *z;

	z = va;
	pixwid = z->pixwid;
	b = buf;
	e = b+n;
	while(b+pixwid <= e){
		if(z->y >= z->dy)
			break;
		if(z->x == 0)
			*b++ = FilterNone;
		pixels = (e-b)/pixwid;
		if(pixels > z->dx - z->x)
			pixels = z->dx - z->x;
		img = z->data + z->width*z->y + pixwid*z->x;
		memmove(b, img, pixwid*pixels);
		if(pixwid == 4){
			/*
			 * Convert to non-premultiplied alpha.
			 */
			for(i=0; i<pixels; i++, b+=4){
				a = b[3];
				if(a != 0 && a != 255){
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

		z->x += pixels;
		if(z->x >= z->dx){
			z->x = 0;
			z->y++;
		}
	}
	return b - (uchar*)buf;
}

static void
IDAT(ZlibW *z)
{
	chunk(z->io, "IDAT", z->buf, z->b - z->buf);
	z->b = z->buf;
}

static int
zwrite(void *va, void *buf, int n)
{
	int m;
	uchar *b, *e;
	ZlibW *z;

	z = va;
	b = buf;
	e = b+n;

	while(b < e){
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

	qlock(&memdrawlock);
	ni = allocmemimage(i->r, dst);
	if(ni)
		memimagedraw(ni, ni->r, i, i->r.min, nil, i->r.min, S);
	qunlock(&memdrawlock);
	return ni;
}

int
writepng(Hio *io, Memimage *m)
{
	static int first = 1;
	static QLock lk;
	uchar buf[200], *h;
	Memimage *rgb;
	ZlibR zr;
	ZlibW zw;

	if(first){
		qlock(&lk);
		if(first){
			deflateinit();
			crctab = mkcrctab(0xedb88320);
			first = 0;
		}
		qunlock(&lk);
	}

	rgb = memRGBA(m);
	if(rgb == nil)
		return -1;

	hwrite(io, PNGmagic, sizeof PNGmagic);

	/* IHDR chunk */
	h = buf;
	put4(h, Dx(m->r)); h += 4;
	put4(h, Dy(m->r)); h += 4;
	*h++ = 8;	/* 8 bits per channel */
	if(rgb->chan == BGR24)
		*h++ = 2;		/* RGB */
	else
		*h++ = 6;		/* RGBA */
	*h++ = 0;	/* compression - deflate */
	*h++ = 0;	/* filter - none */
	*h++ = 0;	/* interlace - none */
	chunk(io, "IHDR", buf, h-buf);

	/* image data */
	zr.dx = Dx(m->r);
	zr.dy = Dy(m->r);
	zr.width = rgb->width * sizeof(u32int);
	zr.data = rgb->data->bdata;
	zr.x = 0;
	zr.y = 0;
	zr.pixwid = chantodepth(rgb->chan)/8;
	zw.io = io;
	zw.buf = vtmalloc(IDATSIZE);
	zw.b = zw.buf;
	zw.e = zw.b + IDATSIZE;
	if(deflatezlib(&zw, zwrite, &zr, zread, 6, 0) < 0){
		free(zw.buf);
		return -1;
	}
	if(zw.b > zw.buf)
		IDAT(&zw);
	free(zw.buf);
	chunk(io, "IEND", nil, 0);

	if(m != rgb){
		qlock(&memdrawlock);
		freememimage(rgb);
		qunlock(&memdrawlock);
	}
	return 0;
}
