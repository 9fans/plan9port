#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include "imagefile.h"

enum {
	/* Constants, all preceded by byte 0xFF */
	SOF	=0xC0,	/* Start of Frame */
	SOF2=0xC2,	/* Start of Frame; progressive Huffman */
	JPG	=0xC8,	/* Reserved for JPEG extensions */
	DHT	=0xC4,	/* Define Huffman Tables */
	DAC	=0xCC,	/* Arithmetic coding conditioning */
	RST	=0xD0,	/* Restart interval termination */
	RST7	=0xD7,	/* Restart interval termination (highest value) */
	SOI	=0xD8,	/* Start of Image */
	EOI	=0xD9,	/* End of Image */
	SOS	=0xDA,	/* Start of Scan */
	DQT	=0xDB,	/* Define quantization tables */
	DNL	=0xDC,	/* Define number of lines */
	DRI	=0xDD,	/* Define restart interval */
	DHP	=0xDE,	/* Define hierarchical progression */
	EXP	=0xDF,	/* Expand reference components */
	APPn	=0xE0,	/* Reserved for application segments */
	JPGn	=0xF0,	/* Reserved for JPEG extensions */
	COM	=0xFE,	/* Comment */

	CLAMPOFF	= 300,
	NCLAMP		= CLAMPOFF+700
};

typedef struct Framecomp Framecomp;
typedef struct Header Header;
typedef struct Huffman Huffman;

struct Framecomp	/* Frame component specifier from SOF marker */
{
	int	C;
	int	H;
	int	V;
	int	Tq;
};

struct Huffman
{
	int	*size;	/* malloc'ed */
	int	*code;	/* malloc'ed */
	int	*val;		/* malloc'ed */
	int	mincode[17];
	int	maxcode[17];
	int	valptr[17];
	/* fast lookup */
	int	value[256];
	int	shift[256];
};


struct Header
{
	Biobuf	*fd;
	char		err[256];
	jmp_buf	errlab;
	/* variables in i/o routines */
	int		sr;	/* shift register, right aligned */
	int		cnt;	/* # bits in right part of sr */
	uchar	*buf;
	int		nbuf;
	int		peek;

	int		Nf;

	Framecomp	comp[3];
	uchar	mode;
	int		X;
	int		Y;
	int		qt[4][64];		/* quantization tables */
	Huffman	dcht[4];
	Huffman	acht[4];
	int		**data[3];
	int		ndata[3];

	uchar	*sf;	/* start of frame; do better later */
	uchar	*ss;	/* start of scan; do better later */
	int		ri;	/* restart interval */

	/* progressive scan */
	Rawimage *image;
	Rawimage **array;
	int		*dccoeff[3];
	int		**accoeff[3];	/* only need 8 bits plus quantization */
	int		naccoeff[3];
	int		nblock[3];
	int		nacross;
	int		ndown;
	int		Hmax;
	int		Vmax;
};

static	uchar	clamp[NCLAMP];

static	Rawimage	*readslave(Header*, int);
static	int			readsegment(Header*, int*);
static	void			quanttables(Header*, uchar*, int);
static	void			huffmantables(Header*, uchar*, int);
static	void			soiheader(Header*);
static	int			nextbyte(Header*, int);
static	int			int2(uchar*, int);
static	void			nibbles(int, int*, int*);
static	int			receive(Header*, int);
static	int			receiveEOB(Header*, int);
static	int			receivebit(Header*);
static	void			restart(Header*, int);
static	int			decode(Header*, Huffman*);
static	Rawimage*	baselinescan(Header*, int);
static	void			progressivescan(Header*, int);
static	Rawimage*	progressiveIDCT(Header*, int);
static	void			idct(int*);
static	void			colormap1(Header*, int, Rawimage*, int*, int, int);
static	void			colormapall1(Header*, int, Rawimage*, int*, int*, int*, int, int);
static	void			colormap(Header*, int, Rawimage*, int**, int**, int**, int, int, int, int, int*, int*);
static	void			jpgerror(Header*, char*, ...);

static	char		readerr[] = "ReadJPG: read error: %r";
static	char		memerr[] = "ReadJPG: malloc failed: %r";

static	int zig[64] = {
	0, 1, 8, 16, 9, 2, 3, 10, 17, /* 0-7 */
	24, 32, 25, 18, 11, 4, 5, /* 8-15 */
	12, 19, 26, 33, 40, 48, 41, 34, /* 16-23 */
	27, 20, 13, 6, 7, 14, 21, 28, /* 24-31 */
	35, 42, 49, 56, 57, 50, 43, 36, /* 32-39 */
	29, 22, 15, 23, 30, 37, 44, 51, /* 40-47 */
	58, 59, 52, 45, 38, 31, 39, 46, /* 48-55 */
	53, 60, 61, 54, 47, 55, 62, 63 /* 56-63 */
};

static
void
jpginit(void)
{
	int k;
	static int inited;

	if(inited)
		return;
	inited = 1;
	for(k=0; k<CLAMPOFF; k++)
		clamp[k] = 0;
	for(; k<CLAMPOFF+256; k++)
		clamp[k] = k-CLAMPOFF;
	for(; k<NCLAMP; k++)
		clamp[k] = 255;
}

static
void*
jpgmalloc(Header *h, int n, int clear)
{
	void *p;

	p = malloc(n);
	if(p == nil)
		jpgerror(h, memerr);
	if(clear)
		memset(p, 0, n);
	return p;
}

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
jpgfreeall(Header *h, int freeimage)
{
	int i, j;

	clear(&h->buf);
	if(h->dccoeff[0])
		for(i=0; i<3; i++)
			clear(&h->dccoeff[i]);
	if(h->accoeff[0])
		for(i=0; i<3; i++){
			if(h->accoeff[i])
				for(j=0; j<h->naccoeff[i]; j++)
					clear(&h->accoeff[i][j]);
			clear(&h->accoeff[i]);
		}
	for(i=0; i<4; i++){
		clear(&h->dcht[i].size);
		clear(&h->acht[i].size);
		clear(&h->dcht[i].code);
		clear(&h->acht[i].code);
		clear(&h->dcht[i].val);
		clear(&h->acht[i].val);
	}
	if(h->data[0])
		for(i=0; i<3; i++){
			if(h->data[i])
				for(j=0; j<h->ndata[i]; j++)
					clear(&h->data[i][j]);
			clear(&h->data[i]);
		}
	if(freeimage && h->image!=nil){
		clear(&h->array);
		clear(&h->image->cmap);
		for(i=0; i<3; i++)
			clear(&h->image->chans[i]);
		clear(&h->image);
	}
}

static
void
jpgerror(Header *h, char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	vseprint(h->err, h->err+sizeof h->err, fmt, arg);
	va_end(arg);

	werrstr(h->err);
	jpgfreeall(h, 1);
	longjmp(h->errlab, 1);
}

Rawimage**
Breadjpg(Biobuf *b, int colorspace)
{
	Rawimage *r, **array;
	Header *h;
	char buf[ERRMAX];

	buf[0] = '\0';
	if(colorspace!=CYCbCr && colorspace!=CRGB){
		errstr(buf, sizeof buf);	/* throw it away */
		werrstr("ReadJPG: unknown color space");
		return nil;
	}
	jpginit();
	h = malloc(sizeof(Header));
	array = malloc(sizeof(Header));
	if(h==nil || array==nil){
		free(h);
		free(array);
		return nil;
	}
	h->array = array;
	memset(h, 0, sizeof(Header));
	h->fd = b;
	errstr(buf, sizeof buf);	/* throw it away */
	if(setjmp(h->errlab))
		r = nil;
	else
		r = readslave(h, colorspace);
	jpgfreeall(h, 0);
	free(h);
	array[0] = r;
	array[1] = nil;
	return array;
}

Rawimage**
readjpg(int fd, int colorspace)
{
	Rawimage** a;
	Biobuf b;

	if(Binit(&b, fd, OREAD) < 0)
		return nil;
	a = Breadjpg(&b, colorspace);
	Bterm(&b);
	return a;
}

static
Rawimage*
readslave(Header *header, int colorspace)
{
	Rawimage *image;
	int nseg, i, H, V, m, n;
	uchar *b;

	soiheader(header);
	nseg = 0;
	image = nil;

	header->buf = jpgmalloc(header, 4096, 0);
	header->nbuf = 4096;
	while(header->err[0] == '\0'){
		nseg++;
		n = readsegment(header, &m);
		b = header->buf;
		switch(m){
		case -1:
			return image;

		case APPn+0:
			if(nseg==1 && strncmp((char*)b, "JFIF", 4)==0)  /* JFIF header; check version */
				if(b[5]>1 || b[6]>2)
					sprint(header->err, "ReadJPG: can't handle JFIF version %d.%2d", b[5], b[6]);
			break;

		case APPn+1: case APPn+2: case APPn+3: case APPn+4: case APPn+5:
		case APPn+6: case APPn+7: case APPn+8: case APPn+9: case APPn+10:
		case APPn+11: case APPn+12: case APPn+13: case APPn+14: case APPn+15:
			break;

		case DQT:
			quanttables(header, b, n);
			break;

		case SOF:
		case SOF2:
			header->Y = int2(b, 1);
			header->X = int2(b, 3);
			header->Nf =b[5];
			for(i=0; i<header->Nf; i++){
				header->comp[i].C = b[6+3*i+0];
				nibbles(b[6+3*i+1], &H, &V);
				if(H<=0 || V<=0)
					jpgerror(header, "non-positive sampling factor (Hsamp or Vsamp)");
				header->comp[i].H = H;
				header->comp[i].V = V;
				header->comp[i].Tq = b[6+3*i+2];
			}
			header->mode = m;
			header->sf = b;
			break;

		case  SOS:
			header->ss = b;
			switch(header->mode){
			case SOF:
				image = baselinescan(header, colorspace);
				break;
			case SOF2:
				progressivescan(header, colorspace);
				break;
			default:
				sprint(header->err, "unrecognized or unspecified encoding %d", header->mode);
				break;
			}
			break;

		case  DHT:
			huffmantables(header, b, n);
			break;

		case  DRI:
			header->ri = int2(b, 0);
			break;

		case  COM:
			break;

		case EOI:
			if(header->mode == SOF2)
				image = progressiveIDCT(header, colorspace);
			return image;

		default:
			sprint(header->err, "ReadJPG: unknown marker %.2x", m);
			break;
		}
	}
	return image;
}

/* readsegment is called after reading scan, which can have */
/* read ahead a byte.  so we must check peek here */
static
int
readbyte(Header *h)
{
	uchar x;

	if(h->peek >= 0){
		x = h->peek;
		h->peek = -1;
	}else if(Bread(h->fd, &x, 1) != 1)
		jpgerror(h, readerr);
	return x;
}

static
int
marker(Header *h)
{
	int c;

	while((c=readbyte(h)) == 0)
		fprint(2, "ReadJPG: skipping zero byte at offset %lld\n", Boffset(h->fd));
	if(c != 0xFF)
		jpgerror(h, "ReadJPG: expecting marker; found 0x%x at offset %lld\n", c, Boffset(h->fd));
	while(c == 0xFF)
		c = readbyte(h);
	return c;
}

static
int
int2(uchar *buf, int n)
{
	return (buf[n]<<8) + buf[n+1];
}

static
void
nibbles(int b, int *p0, int *p1)
{
	*p0 = (b>>4) & 0xF;
	*p1 = b & 0xF;
}

static
void
soiheader(Header *h)
{
	h->peek = -1;
	if(marker(h) != SOI)
		jpgerror(h, "ReadJPG: unrecognized marker in header");
	h->err[0] = '\0';
	h->mode = 0;
	h->ri = 0;
}

static
int
readsegment(Header *h, int *markerp)
{
	int m, n;
	uchar tmp[2];

	m = marker(h);
	switch(m){
	case EOI:
		*markerp = m;
		return 0;
	case 0:
		jpgerror(h, "ReadJPG: expecting marker; saw %.2x at offset %lld", m, Boffset(h->fd));
	}
	if(Bread(h->fd, tmp, 2) != 2)
    Readerr:
		jpgerror(h, readerr);
	n = int2(tmp, 0);
	if(n < 2)
		goto Readerr;
	n -= 2;
	if(n > h->nbuf){
		free(h->buf);
		h->buf = jpgmalloc(h, n+1, 0); /* +1 for sentinel */
		h->nbuf = n;
	}
	if(Bread(h->fd, h->buf, n) != n)
		goto Readerr;
	*markerp = m;
	return n;
}

static
int
huffmantable(Header *h, uchar *b)
{
	Huffman *t;
	int Tc, th, n, nsize, i, j, k, v, cnt, code, si, sr, m;
	int *maxcode;

	nibbles(b[0], &Tc, &th);
	if(Tc > 1)
		jpgerror(h, "ReadJPG: unknown Huffman table class %d", Tc);
	if(th>3 || (h->mode==SOF && th>1))
		jpgerror(h, "ReadJPG: unknown Huffman table index %d", th);
	if(Tc == 0)
		t = &h->dcht[th];
	else
		t = &h->acht[th];

	/* flow chart C-2 */
	nsize = 0;
	for(i=0; i<16; i++)
		nsize += b[1+i];
	t->size = jpgmalloc(h, (nsize+1)*sizeof(int), 1);
	k = 0;
	for(i=1; i<=16; i++){
		n = b[i];
		for(j=0; j<n; j++)
			t->size[k++] = i;
	}
	t->size[k] = 0;

	/* initialize HUFFVAL */
	t->val = jpgmalloc(h, nsize*sizeof(int), 1);
	for(i=0; i<nsize; i++)
		t->val[i] = b[17+i];

	/* flow chart C-3 */
	t->code = jpgmalloc(h, (nsize+1)*sizeof(int), 1);
	k = 0;
	code = 0;
	si = t->size[0];
	for(;;){
		do
			t->code[k++] = code++;
		while(t->size[k] == si);
		if(t->size[k] == 0)
			break;
		do{
			code <<= 1;
			si++;
		}while(t->size[k] != si);
	}

	/* flow chart F-25 */
	i = 0;
	j = 0;
	for(;;){
		for(;;){
			i++;
			if(i > 16)
				goto outF25;
			if(b[i] != 0)
				break;
			t->maxcode[i] = -1;
		}
		t->valptr[i] = j;
		t->mincode[i] = t->code[j];
		j += b[i]-1;
		t->maxcode[i] = t->code[j];
		j++;
	}
outF25:

	/* create byte-indexed fast path tables */
	maxcode = t->maxcode;
	/* stupid startup algorithm: just run machine for each byte value */
	for(v=0; v<256; ){
		cnt = 7;
		m = 1<<7;
		code = 0;
		sr = v;
		i = 1;
		for(;;i++){
			if(sr & m)
				code |= 1;
			if(code <= maxcode[i])
				break;
			code <<= 1;
			m >>= 1;
			if(m == 0){
				t->shift[v] = 0;
				t->value[v] = -1;
				goto continueBytes;
			}
			cnt--;
		}
		t->shift[v] = 8-cnt;
		t->value[v] = t->val[t->valptr[i]+(code-t->mincode[i])];

    continueBytes:
		v++;
	}

	return nsize;
}

static
void
huffmantables(Header *h, uchar *b, int n)
{
	int l, mt;

	for(l=0; l<n; l+=17+mt)
		mt = huffmantable(h, &b[l]);
}

static
int
quanttable(Header *h, uchar *b)
{
	int i, pq, tq, *q;

	nibbles(b[0], &pq, &tq);
	if(pq > 1)
		jpgerror(h, "ReadJPG: unknown quantization table class %d", pq);
	if(tq > 3)
		jpgerror(h, "ReadJPG: unknown quantization table index %d", tq);
	q = h->qt[tq];
	for(i=0; i<64; i++){
		if(pq == 0)
			q[i] = b[1+i];
		else
			q[i] = int2(b, 1+2*i);
	}
	return 64*(1+pq);
}

static
void
quanttables(Header *h, uchar *b, int n)
{
	int l, m;

	for(l=0; l<n; l+=1+m)
		m = quanttable(h, &b[l]);
}

static
Rawimage*
baselinescan(Header *h, int colorspace)
{
	int Ns, z, k, m, Hmax, Vmax, comp;
	int allHV1, nblock, ri, mcu, nacross, nmcu;
	Huffman *dcht, *acht;
	int block, t, diff, *qt;
	uchar *ss;
	Rawimage *image;
	int Td[3], Ta[3], H[3], V[3], DC[3];
	int ***data, *zz;

	ss = h->ss;
	Ns = ss[0];
	if((Ns!=3 && Ns!=1) || Ns!=h->Nf)
		jpgerror(h, "ReadJPG: can't handle scan not 3 components");

	image = jpgmalloc(h, sizeof(Rawimage), 1);
	h->image = image;
	image->r = Rect(0, 0, h->X, h->Y);
	image->cmap = nil;
	image->cmaplen = 0;
	image->chanlen = h->X*h->Y;
	image->fields = 0;
	image->gifflags = 0;
	image->gifdelay = 0;
	image->giftrindex = 0;
	if(Ns == 3)
		image->chandesc = colorspace;
	else
		image->chandesc = CY;
	image->nchans = h->Nf;
	for(k=0; k<h->Nf; k++)
		image->chans[k] = jpgmalloc(h, h->X*h->Y, 0);

	/* compute maximum H and V */
	Hmax = 0;
	Vmax = 0;
	for(comp=0; comp<Ns; comp++){
		if(h->comp[comp].H > Hmax)
			Hmax = h->comp[comp].H;
		if(h->comp[comp].V > Vmax)
			Vmax = h->comp[comp].V;
	}

	/* initialize data structures */
	allHV1 = 1;
	data = h->data;
	for(comp=0; comp<Ns; comp++){
		/* JPEG requires scan components to be in same order as in frame, */
		/* so if both have 3 we know scan is Y Cb Cr and there's no need to */
		/* reorder */
		nibbles(ss[2+2*comp], &Td[comp], &Ta[comp]);
		H[comp] = h->comp[comp].H;
		V[comp] = h->comp[comp].V;
		nblock = H[comp]*V[comp];
		if(nblock != 1)
			allHV1 = 0;
		data[comp] = jpgmalloc(h, nblock*sizeof(int*), 0);
		h->ndata[comp] = nblock;
		DC[comp] = 0;
		for(m=0; m<nblock; m++)
			data[comp][m] = jpgmalloc(h, 8*8*sizeof(int), 0);
	}

	ri = h->ri;

	h->cnt = 0;
	h->sr = 0;
	h->peek = -1;
	nacross = ((h->X+(8*Hmax-1))/(8*Hmax));
	nmcu = ((h->Y+(8*Vmax-1))/(8*Vmax))*nacross;
	for(mcu=0; mcu<nmcu; ){
		for(comp=0; comp<Ns; comp++){
			dcht = &h->dcht[Td[comp]];
			acht = &h->acht[Ta[comp]];
			qt = h->qt[h->comp[comp].Tq];

			for(block=0; block<H[comp]*V[comp]; block++){
				/* F-22 */
				t = decode(h, dcht);
				diff = receive(h, t);
				DC[comp] += diff;

				/* F-23 */
				zz = data[comp][block];
				memset(zz, 0, 8*8*sizeof(int));
				zz[0] = qt[0]*DC[comp];
				k = 1;

				for(;;){
					t = decode(h, acht);
					if((t&0x0F) == 0){
						if((t&0xF0) != 0xF0)
							break;
						k += 16;
					}else{
						k += t>>4;
						z = receive(h, t&0xF);
						zz[zig[k]] = z*qt[k];
						if(k == 63)
							break;
						k++;
					}
				}

				idct(zz);
			}
		}

		/* rotate colors to RGB and assign to bytes */
		if(Ns == 1) /* very easy */
			colormap1(h, colorspace, image, data[0][0], mcu, nacross);
		else if(allHV1) /* fairly easy */
			colormapall1(h, colorspace, image, data[0][0], data[1][0], data[2][0], mcu, nacross);
		else /* miserable general case */
			colormap(h, colorspace, image, data[0], data[1], data[2], mcu, nacross, Hmax, Vmax, H, V);
		/* process restart marker, if present */
		mcu++;
		if(ri>0 && mcu<nmcu && mcu%ri==0){
			restart(h, mcu);
			for(comp=0; comp<Ns; comp++)
				DC[comp] = 0;
		}
	}
	return image;
}

static
void
restart(Header *h, int mcu)
{
	int rest, rst, nskip;

	rest = mcu/h->ri-1;
	nskip = 0;
	do{
		do{
			rst = nextbyte(h, 1);
			nskip++;
		}while(rst>=0 && rst!=0xFF);
		if(rst == 0xFF){
			rst = nextbyte(h, 1);
			nskip++;
		}
	}while(rst>=0 && (rst&~7)!=RST);
	if(nskip != 2)
		sprint(h->err, "ReadJPG: skipped %d bytes at restart %d\n", nskip-2, rest);
	if(rst < 0)
		jpgerror(h, readerr);
	if((rst&7) != (rest&7))
		jpgerror(h, "ReadJPG: expected RST%d got %d", rest&7, rst&7);
	h->cnt = 0;
	h->sr = 0;
}

static
Rawimage*
progressiveIDCT(Header *h, int colorspace)
{
	int k, m, comp, block, Nf, bn;
	int allHV1, nblock, mcu, nmcu;
	int H[3], V[3], blockno[3];
	int *dccoeff, **accoeff;
	int ***data, *zz;

	Nf = h->Nf;
	allHV1 = 1;
	data = h->data;

	for(comp=0; comp<Nf; comp++){
		H[comp] = h->comp[comp].H;
		V[comp] = h->comp[comp].V;
		nblock = h->nblock[comp];
		if(nblock != 1)
			allHV1 = 0;
		h->ndata[comp] = nblock;
		data[comp] = jpgmalloc(h, nblock*sizeof(int*), 0);
		for(m=0; m<nblock; m++)
			data[comp][m] = jpgmalloc(h, 8*8*sizeof(int), 0);
	}

	memset(blockno, 0, sizeof blockno);
	nmcu = h->nacross*h->ndown;
	for(mcu=0; mcu<nmcu; mcu++){
		for(comp=0; comp<Nf; comp++){
			dccoeff = h->dccoeff[comp];
			accoeff = h->accoeff[comp];
			bn = blockno[comp];
			for(block=0; block<h->nblock[comp]; block++){
				zz = data[comp][block];
				memset(zz, 0, 8*8*sizeof(int));
				zz[0] = dccoeff[bn];

				for(k=1; k<64; k++)
					zz[zig[k]] = accoeff[bn][k];

				idct(zz);
				bn++;
			}
			blockno[comp] = bn;
		}

		/* rotate colors to RGB and assign to bytes */
		if(Nf == 1) /* very easy */
			colormap1(h, colorspace, h->image, data[0][0], mcu, h->nacross);
		else if(allHV1) /* fairly easy */
			colormapall1(h, colorspace, h->image, data[0][0], data[1][0], data[2][0], mcu, h->nacross);
		else /* miserable general case */
			colormap(h, colorspace, h->image, data[0], data[1], data[2], mcu, h->nacross, h->Hmax, h->Vmax, H, V);
	}

	return h->image;
}

static
void
progressiveinit(Header *h, int colorspace)
{
	int Nf, Ns, j, k, nmcu, comp;
	uchar *ss;
	Rawimage *image;

	ss = h->ss;
	Ns = ss[0];
	Nf = h->Nf;
	if((Ns!=3 && Ns!=1) || Ns!=Nf)
		jpgerror(h, "ReadJPG: image must have 1 or 3 components");

	image = jpgmalloc(h, sizeof(Rawimage), 1);
	h->image = image;
	image->r = Rect(0, 0, h->X, h->Y);
	image->cmap = nil;
	image->cmaplen = 0;
	image->chanlen = h->X*h->Y;
	image->fields = 0;
	image->gifflags = 0;
	image->gifdelay = 0;
	image->giftrindex = 0;
	if(Nf == 3)
		image->chandesc = colorspace;
	else
		image->chandesc = CY;
	image->nchans = h->Nf;
	for(k=0; k<Nf; k++){
		image->chans[k] = jpgmalloc(h, h->X*h->Y, 0);
		h->nblock[k] = h->comp[k].H*h->comp[k].V;
	}

	/* compute maximum H and V */
	h->Hmax = 0;
	h->Vmax = 0;
	for(comp=0; comp<Nf; comp++){
		if(h->comp[comp].H > h->Hmax)
			h->Hmax = h->comp[comp].H;
		if(h->comp[comp].V > h->Vmax)
			h->Vmax = h->comp[comp].V;
	}
	h->nacross = ((h->X+(8*h->Hmax-1))/(8*h->Hmax));
	h->ndown = ((h->Y+(8*h->Vmax-1))/(8*h->Vmax));
	nmcu = h->nacross*h->ndown;

	for(k=0; k<Nf; k++){
		h->dccoeff[k] = jpgmalloc(h, h->nblock[k]*nmcu * sizeof(int), 1);
		h->accoeff[k] = jpgmalloc(h, h->nblock[k]*nmcu * sizeof(int*), 1);
		h->naccoeff[k] = h->nblock[k]*nmcu;
		for(j=0; j<h->nblock[k]*nmcu; j++)
			h->accoeff[k][j] = jpgmalloc(h, 64*sizeof(int), 1);
	}

}

static
void
progressivedc(Header *h, int comp, int Ah, int Al)
{
	int Ns, z, ri, mcu,  nmcu;
	int block, t, diff, qt, *dc, bn;
	Huffman *dcht;
	uchar *ss;
	int Td[3], DC[3], blockno[3];

	ss= h->ss;
	Ns = ss[0];
	if(Ns!=h->Nf)
		jpgerror(h, "ReadJPG: can't handle progressive with Nf!=Ns in DC scan");

	/* initialize data structures */
	h->cnt = 0;
	h->sr = 0;
	h->peek = -1;
	for(comp=0; comp<Ns; comp++){
		/*
		 * JPEG requires scan components to be in same order as in frame,
		 * so if both have 3 we know scan is Y Cb Cr and there's no need to
		 * reorder
		 */
		nibbles(ss[2+2*comp], &Td[comp], &z);	/* z is ignored */
		DC[comp] = 0;
	}

	ri = h->ri;

	nmcu = h->nacross*h->ndown;
	memset(blockno, 0, sizeof blockno);
	for(mcu=0; mcu<nmcu; ){
		for(comp=0; comp<Ns; comp++){
			dcht = &h->dcht[Td[comp]];
			qt = h->qt[h->comp[comp].Tq][0];
			dc = h->dccoeff[comp];
			bn = blockno[comp];

			for(block=0; block<h->nblock[comp]; block++){
				if(Ah == 0){
					t = decode(h, dcht);
					diff = receive(h, t);
					DC[comp] += diff;
					dc[bn] = qt*DC[comp]<<Al;
				}else
					dc[bn] |= qt*receivebit(h)<<Al;
				bn++;
			}
			blockno[comp] = bn;
		}

		/* process restart marker, if present */
		mcu++;
		if(ri>0 && mcu<nmcu && mcu%ri==0){
			restart(h, mcu);
			for(comp=0; comp<Ns; comp++)
				DC[comp] = 0;
		}
	}
}

static
void
progressiveac(Header *h, int comp, int Al)
{
	int Ns, Ss, Se, z, k, eobrun, x, y, nver, tmcu, blockno, *acc, rs;
	int ri, mcu, nacross, ndown, nmcu, nhor;
	Huffman *acht;
	int *qt, rrrr, ssss, q;
	uchar *ss;
	int Ta, H, V;

	ss = h->ss;
	Ns = ss[0];
	if(Ns != 1)
		jpgerror(h, "ReadJPG: illegal Ns>1 in progressive AC scan");
	Ss = ss[1+2];
	Se = ss[2+2];
	H = h->comp[comp].H;
	V = h->comp[comp].V;

	nacross = h->nacross*H;
	ndown = h->ndown*V;
	q = 8*h->Hmax/H;
	nhor = (h->X+q-1)/q;
	q = 8*h->Vmax/V;
	nver = (h->Y+q-1)/q;

	/* initialize data structures */
	h->cnt = 0;
	h->sr = 0;
	h->peek = -1;
	nibbles(ss[1+1], &z, &Ta);	/* z is thrown away */

	ri = h->ri;

	eobrun = 0;
	acht = &h->acht[Ta];
	qt = h->qt[h->comp[comp].Tq];
	nmcu = nacross*ndown;
	mcu = 0;
	for(y=0; y<nver; y++){
		for(x=0; x<nhor; x++){
			/* Figure G-3  */
			if(eobrun > 0){
				--eobrun;
				continue;
			}

			/* arrange blockno to be in same sequence as original scan calculation. */
			tmcu = x/H + (nacross/H)*(y/V);
			blockno = tmcu*H*V + H*(y%V) + x%H;
			acc = h->accoeff[comp][blockno];
			k = Ss;
			for(;;){
				rs = decode(h, acht);
				/* XXX remove rrrr ssss as in baselinescan */
				nibbles(rs, &rrrr, &ssss);
				if(ssss == 0){
					if(rrrr < 15){
						eobrun = 0;
						if(rrrr > 0)
							eobrun = receiveEOB(h, rrrr)-1;
						break;
					}
					k += 16;
				}else{
					k += rrrr;
					z = receive(h, ssss);
					acc[k] = z*qt[k]<<Al;
					if(k == Se)
						break;
					k++;
				}
			}
		}

		/* process restart marker, if present */
		mcu++;
		if(ri>0 && mcu<nmcu && mcu%ri==0){
			restart(h, mcu);
			eobrun = 0;
		}
	}
}

static
void
increment(Header *h, int acc[], int k, int Pt)
{
	if(acc[k] == 0)
		return;
	if(receivebit(h) != 0)
		if(acc[k] < 0)
			acc[k] -= Pt;
		else
			acc[k] += Pt;
}

static
void
progressiveacinc(Header *h, int comp, int Al)
{
	int Ns, i, z, k, Ss, Se, Ta, **ac, H, V;
	int ri, mcu, nacross, ndown, nhor, nver, eobrun, nzeros, pending, x, y, tmcu, blockno, q, nmcu;
	Huffman *acht;
	int *qt, rrrr, ssss, *acc, rs;
	uchar *ss;

	ss = h->ss;
	Ns = ss[0];
	if(Ns != 1)
		jpgerror(h, "ReadJPG: illegal Ns>1 in progressive AC scan");
	Ss = ss[1+2];
	Se = ss[2+2];
	H = h->comp[comp].H;
	V = h->comp[comp].V;

	nacross = h->nacross*H;
	ndown = h->ndown*V;
	q = 8*h->Hmax/H;
	nhor = (h->X+q-1)/q;
	q = 8*h->Vmax/V;
	nver = (h->Y+q-1)/q;

	/* initialize data structures */
	h->cnt = 0;
	h->sr = 0;
	h->peek = -1;
	nibbles(ss[1+1], &z, &Ta);	/* z is thrown away */
	ri = h->ri;

	eobrun = 0;
	ac = h->accoeff[comp];
	acht = &h->acht[Ta];
	qt = h->qt[h->comp[comp].Tq];
	nmcu = nacross*ndown;
	mcu = 0;
	pending = 0;
	nzeros = -1;
	for(y=0; y<nver; y++){
		for(x=0; x<nhor; x++){
			/* Figure G-7 */

			/*  arrange blockno to be in same sequence as original scan calculation. */
			tmcu = x/H + (nacross/H)*(y/V);
			blockno = tmcu*H*V + H*(y%V) + x%H;
			acc = ac[blockno];
			if(eobrun > 0){
				if(nzeros > 0)
					jpgerror(h, "ReadJPG: zeros pending at block start");
				for(k=Ss; k<=Se; k++)
					increment(h, acc, k, qt[k]<<Al);
				--eobrun;
				continue;
			}

			for(k=Ss; k<=Se; ){
				if(nzeros >= 0){
					if(acc[k] != 0)
						increment(h, acc, k, qt[k]<<Al);
					else if(nzeros-- == 0)
						acc[k] = pending;
					k++;
					continue;
				}
				rs = decode(h, acht);
				nibbles(rs, &rrrr, &ssss);
				if(ssss == 0){
					if(rrrr < 15){
						eobrun = 0;
						if(rrrr > 0)
							eobrun = receiveEOB(h, rrrr)-1;
						while(k <= Se){
							increment(h, acc, k, qt[k]<<Al);
							k++;
						}
						break;
					}
					for(i=0; i<16; k++){
						increment(h, acc, k, qt[k]<<Al);
						if(acc[k] == 0)
							i++;
					}
					continue;
				}else if(ssss != 1)
					jpgerror(h, "ReadJPG: ssss!=1 in progressive increment");
				nzeros = rrrr;
				pending = receivebit(h);
				if(pending == 0)
					pending = -1;
				pending *= qt[k]<<Al;
			}
		}

		/* process restart marker, if present */
		mcu++;
		if(ri>0 && mcu<nmcu && mcu%ri==0){
			restart(h, mcu);
			eobrun = 0;
			nzeros = -1;
		}
	}
}

static
void
progressivescan(Header *h, int colorspace)
{
	uchar *ss;
	int Ns, Ss, Ah, Al, c, comp, i;

	if(h->dccoeff[0] == nil)
		progressiveinit(h, colorspace);

	ss = h->ss;
	Ns = ss[0];
	Ss = ss[1+2*Ns];
	nibbles(ss[3+2*Ns], &Ah, &Al);
	c = ss[1];
	comp = -1;
	for(i=0; i<h->Nf; i++)
		if(h->comp[i].C == c)
			comp = i;
	if(comp == -1)
		jpgerror(h, "ReadJPG: bad component index in scan header");

	if(Ss == 0){
		progressivedc(h, comp, Ah, Al);
		return;
	}
	if(Ah == 0){
		progressiveac(h, comp, Al);
		return;
	}
	progressiveacinc(h, comp, Al);
}

enum {
	c1 = 2871,	/* 1.402 * 2048 */
	c2 = 705,		/* 0.34414 * 2048 */
	c3 = 1463,	/* 0.71414 * 2048 */
	c4 = 3629	/* 1.772 * 2048 */
};

static
void
colormap1(Header *h, int colorspace, Rawimage *image, int data[8*8], int mcu, int nacross)
{
	uchar *pic;
	int x, y, dx, dy, minx, miny;
	int r, k, pici;

	USED(colorspace);
	pic = image->chans[0];
	minx = 8*(mcu%nacross);
	dx = 8;
	if(minx+dx > h->X)
		dx = h->X-minx;
	miny = 8*(mcu/nacross);
	dy = 8;
	if(miny+dy > h->Y)
		dy = h->Y-miny;
	pici = miny*h->X+minx;
	k = 0;
	for(y=0; y<dy; y++){
		for(x=0; x<dx; x++){
			r = clamp[(data[k+x]+128)+CLAMPOFF];
			pic[pici+x] = r;
		}
		pici += h->X;
		k += 8;
	}
}

static
void
colormapall1(Header *h, int colorspace, Rawimage *image, int data0[8*8], int data1[8*8], int data2[8*8], int mcu, int nacross)
{
	uchar *rpic, *gpic, *bpic, *rp, *gp, *bp;
	int *p0, *p1, *p2;
	int x, y, dx, dy, minx, miny;
	int r, g, b, k, pici;
	int Y, Cr, Cb;

	rpic = image->chans[0];
	gpic = image->chans[1];
	bpic = image->chans[2];
	minx = 8*(mcu%nacross);
	dx = 8;
	if(minx+dx > h->X)
		dx = h->X-minx;
	miny = 8*(mcu/nacross);
	dy = 8;
	if(miny+dy > h->Y)
		dy = h->Y-miny;
	pici = miny*h->X+minx;
	k = 0;
	for(y=0; y<dy; y++){
		p0 = data0+k;
		p1 = data1+k;
		p2 = data2+k;
		rp = rpic+pici;
		gp = gpic+pici;
		bp = bpic+pici;
		if(colorspace == CYCbCr)
			for(x=0; x<dx; x++){
				*rp++ = clamp[*p0++ + 128 + CLAMPOFF];
				*gp++ = clamp[*p1++ + 128 + CLAMPOFF];
				*bp++ = clamp[*p2++ + 128 + CLAMPOFF];
			}
		else
			for(x=0; x<dx; x++){
				Y = (*p0++ + 128) << 11;
				Cb = *p1++;
				Cr = *p2++;
				r = Y+c1*Cr;
				g = Y-c2*Cb-c3*Cr;
				b = Y+c4*Cb;
				*rp++ = clamp[(r>>11)+CLAMPOFF];
				*gp++ = clamp[(g>>11)+CLAMPOFF];
				*bp++ = clamp[(b>>11)+CLAMPOFF];
			}
		pici += h->X;
		k += 8;
	}
}

static
void
colormap(Header *h, int colorspace, Rawimage *image, int *data0[8*8], int *data1[8*8], int *data2[8*8], int mcu, int nacross, int Hmax, int Vmax,  int *H, int *V)
{
	uchar *rpic, *gpic, *bpic;
	int x, y, dx, dy, minx, miny;
	int r, g, b, pici, H0, H1, H2;
	int t, b0, b1, b2, y0, y1, y2, x0, x1, x2;
	int Y, Cr, Cb;

	rpic = image->chans[0];
	gpic = image->chans[1];
	bpic = image->chans[2];
	minx = 8*Hmax*(mcu%nacross);
	dx = 8*Hmax;
	if(minx+dx > h->X)
		dx = h->X-minx;
	miny = 8*Vmax*(mcu/nacross);
	dy = 8*Vmax;
	if(miny+dy > h->Y)
		dy = h->Y-miny;
	pici = miny*h->X+minx;
	H0 = H[0];
	H1 = H[1];
	H2 = H[2];
	for(y=0; y<dy; y++){
		t = y*V[0];
		b0 = H0*(t/(8*Vmax));
		y0 = 8*((t/Vmax)&7);
		t = y*V[1];
		b1 = H1*(t/(8*Vmax));
		y1 = 8*((t/Vmax)&7);
		t = y*V[2];
		b2 = H2*(t/(8*Vmax));
		y2 = 8*((t/Vmax)&7);
		x0 = 0;
		x1 = 0;
		x2 = 0;
		for(x=0; x<dx; x++){
			if(colorspace == CYCbCr){
				rpic[pici+x] = clamp[data0[b0][y0+x0++*H0/Hmax] + 128 + CLAMPOFF];
				gpic[pici+x] = clamp[data1[b1][y1+x1++*H1/Hmax] + 128 + CLAMPOFF];
				bpic[pici+x] = clamp[data2[b2][y2+x2++*H2/Hmax] + 128 + CLAMPOFF];
			}else{
				Y = (data0[b0][y0+x0++*H0/Hmax]+128)<<11;
				Cb = data1[b1][y1+x1++*H1/Hmax];
				Cr = data2[b2][y2+x2++*H2/Hmax];
				r = Y+c1*Cr;
				g = Y-c2*Cb-c3*Cr;
				b = Y+c4*Cb;
				rpic[pici+x] = clamp[(r>>11)+CLAMPOFF];
				gpic[pici+x] = clamp[(g>>11)+CLAMPOFF];
				bpic[pici+x] = clamp[(b>>11)+CLAMPOFF];
			}
			if(x0*H0/Hmax >= 8){
				x0 = 0;
				b0++;
			}
			if(x1*H1/Hmax >= 8){
				x1 = 0;
				b1++;
			}
			if(x2*H2/Hmax >= 8){
				x2 = 0;
				b2++;
			}
		}
		pici += h->X;
	}
}

/*
 * decode next 8-bit value from entropy-coded input.  chart F-26
 */
static
int
decode(Header *h, Huffman *t)
{
	int code, v, cnt, m, sr, i;
	int *maxcode;
	static int badcode;

	maxcode = t->maxcode;
	if(h->cnt < 8)
		nextbyte(h, 0);
	/* fast lookup */
	code = (h->sr>>(h->cnt-8))&0xFF;
	v = t->value[code];
	if(v >= 0){
		h->cnt -= t->shift[code];
		return v;
	}

	h->cnt -= 8;
	if(h->cnt == 0)
		nextbyte(h, 0);
	h->cnt--;
	cnt = h->cnt;
	m = 1<<cnt;
	sr = h->sr;
	code <<= 1;
	i = 9;
	for(;;i++){
		if(sr & m)
			code |= 1;
		if(code <= maxcode[i])
			break;
		code <<= 1;
		m >>= 1;
		if(m == 0){
			sr = nextbyte(h, 0);
			m = 0x80;
			cnt = 8;
		}
		cnt--;
	}
	if(i >= 17){
		if(badcode == 0)
			fprint(2, "badly encoded %dx%d JPEG file; ignoring bad value\n", h->X, h->Y);
		badcode = 1;
		i = 0;
	}
	h->cnt = cnt;
	return t->val[t->valptr[i]+(code-t->mincode[i])];
}

/*
 * load next byte of input
 */
static
int
nextbyte(Header *h, int marker)
{
	int b, b2;

	if(h->peek >= 0){
		b = h->peek;
		h->peek = -1;
	}else{
		b = Bgetc(h->fd);
		if(b == Beof)
			jpgerror(h, "truncated file");
		b &= 0xFF;
	}

	if(b == 0xFF){
		if(marker)
			return b;
		b2 = Bgetc(h->fd);
		if(b2 != 0){
			if(b2 == Beof)
				jpgerror(h, "truncated file");
			b2 &= 0xFF;
			if(b2 == DNL)
				jpgerror(h, "ReadJPG: DNL marker unimplemented");
			/* decoder is reading into marker; satisfy it and restore state */
			Bungetc(h->fd);
			h->peek = b;
		}
	}
	h->cnt += 8;
	h->sr = (h->sr<<8) | b;
	return b;
}

/*
 * return next s bits of input, MSB first, and level shift it
 */
static
int
receive(Header *h, int s)
{
	int v, m;

	while(h->cnt < s)
		nextbyte(h, 0);
	h->cnt -= s;
	v = h->sr >> h->cnt;
	m = (1<<s);
	v &= m-1;
	/* level shift */
	if(v < (m>>1))
		v += ~(m-1)+1;
	return v;
}

/*
 * return next s bits of input, decode as EOB
 */
static
int
receiveEOB(Header *h, int s)
{
	int v, m;

	while(h->cnt < s)
		nextbyte(h, 0);
	h->cnt -= s;
	v = h->sr >> h->cnt;
	m = (1<<s);
	v &= m-1;
	/* level shift */
	v += m;
	return v;
}

/*
 * return next bit of input
 */
static
int
receivebit(Header *h)
{
	if(h->cnt < 1)
		nextbyte(h, 0);
	h->cnt--;
	return (h->sr >> h->cnt) & 1;
}

/*
 *  Scaled integer implementation.
 *  inverse two dimensional DCT, Chen-Wang algorithm
 * (IEEE ASSP-32, pp. 803-816, Aug. 1984)
 * 32-bit integer arithmetic (8 bit coefficients)
 * 11 mults, 29 adds per DCT
 *
 * coefficients extended to 12 bit for IEEE1180-1990 compliance
 */

enum {
	W1		= 2841,	/* 2048*sqrt(2)*cos(1*pi/16)*/
	W2		= 2676,	/* 2048*sqrt(2)*cos(2*pi/16)*/
	W3		= 2408,	/* 2048*sqrt(2)*cos(3*pi/16)*/
	W5		= 1609,	/* 2048*sqrt(2)*cos(5*pi/16)*/
	W6		= 1108,	/* 2048*sqrt(2)*cos(6*pi/16)*/
	W7		= 565,	/* 2048*sqrt(2)*cos(7*pi/16)*/

	W1pW7	= 3406,	/* W1+W7*/
	W1mW7	= 2276,	/* W1-W7*/
	W3pW5	= 4017,	/* W3+W5*/
	W3mW5	= 799,	/* W3-W5*/
	W2pW6	= 3784,	/* W2+W6*/
	W2mW6	= 1567,	/* W2-W6*/

	R2		= 181	/* 256/sqrt(2)*/
};

static
void
idct(int b[8*8])
{
	int x, y, eighty, v;
	int x0, x1, x2, x3, x4, x5, x6, x7, x8;
	int *p;

	/* transform horizontally*/
	for(y=0; y<8; y++){
		eighty = y<<3;
		/* if all non-DC components are zero, just propagate the DC term*/
		p = b+eighty;
		if(p[1]==0)
		if(p[2]==0 && p[3]==0)
		if(p[4]==0 && p[5]==0)
		if(p[6]==0 && p[7]==0){
			v = p[0]<<3;
			p[0] = v;
			p[1] = v;
			p[2] = v;
			p[3] = v;
			p[4] = v;
			p[5] = v;
			p[6] = v;
			p[7] = v;
			continue;
		}
		/* prescale*/
		x0 = (p[0]<<11)+128;
		x1 = p[4]<<11;
		x2 = p[6];
		x3 = p[2];
		x4 = p[1];
		x5 = p[7];
		x6 = p[5];
		x7 = p[3];
		/* first stage*/
		x8 = W7*(x4+x5);
		x4 = x8 + W1mW7*x4;
		x5 = x8 - W1pW7*x5;
		x8 = W3*(x6+x7);
		x6 = x8 - W3mW5*x6;
		x7 = x8 - W3pW5*x7;
		/* second stage*/
		x8 = x0 + x1;
		x0 -= x1;
		x1 = W6*(x3+x2);
		x2 = x1 - W2pW6*x2;
		x3 = x1 + W2mW6*x3;
		x1 = x4 + x6;
		x4 -= x6;
		x6 = x5 + x7;
		x5 -= x7;
		/* third stage*/
		x7 = x8 + x3;
		x8 -= x3;
		x3 = x0 + x2;
		x0 -= x2;
		x2 = (R2*(x4+x5)+128)>>8;
		x4 = (R2*(x4-x5)+128)>>8;
		/* fourth stage*/
		p[0] = (x7+x1)>>8;
		p[1] = (x3+x2)>>8;
		p[2] = (x0+x4)>>8;
		p[3] = (x8+x6)>>8;
		p[4] = (x8-x6)>>8;
		p[5] = (x0-x4)>>8;
		p[6] = (x3-x2)>>8;
		p[7] = (x7-x1)>>8;
	}
	/* transform vertically*/
	for(x=0; x<8; x++){
		/* if all non-DC components are zero, just propagate the DC term*/
		p = b+x;
		if(p[8*1]==0)
		if(p[8*2]==0 && p[8*3]==0)
		if(p[8*4]==0 && p[8*5]==0)
		if(p[8*6]==0 && p[8*7]==0){
			v = (p[8*0]+32)>>6;
			p[8*0] = v;
			p[8*1] = v;
			p[8*2] = v;
			p[8*3] = v;
			p[8*4] = v;
			p[8*5] = v;
			p[8*6] = v;
			p[8*7] = v;
			continue;
		}
		/* prescale*/
		x0 = (p[8*0]<<8)+8192;
		x1 = p[8*4]<<8;
		x2 = p[8*6];
		x3 = p[8*2];
		x4 = p[8*1];
		x5 = p[8*7];
		x6 = p[8*5];
		x7 = p[8*3];
		/* first stage*/
		x8 = W7*(x4+x5) + 4;
		x4 = (x8+W1mW7*x4)>>3;
		x5 = (x8-W1pW7*x5)>>3;
		x8 = W3*(x6+x7) + 4;
		x6 = (x8-W3mW5*x6)>>3;
		x7 = (x8-W3pW5*x7)>>3;
		/* second stage*/
		x8 = x0 + x1;
		x0 -= x1;
		x1 = W6*(x3+x2) + 4;
		x2 = (x1-W2pW6*x2)>>3;
		x3 = (x1+W2mW6*x3)>>3;
		x1 = x4 + x6;
		x4 -= x6;
		x6 = x5 + x7;
		x5 -= x7;
		/* third stage*/
		x7 = x8 + x3;
		x8 -= x3;
		x3 = x0 + x2;
		x0 -= x2;
		x2 = (R2*(x4+x5)+128)>>8;
		x4 = (R2*(x4-x5)+128)>>8;
		/* fourth stage*/
		p[8*0] = (x7+x1)>>14;
		p[8*1] = (x3+x2)>>14;
		p[8*2] = (x0+x4)>>14;
		p[8*3] = (x8+x6)>>14;
		p[8*4] = (x8-x6)>>14;
		p[8*5] = (x0-x4)>>14;
		p[8*6] = (x3-x2)>>14;
		p[8*7] = (x7-x1)>>14;
	}
}
