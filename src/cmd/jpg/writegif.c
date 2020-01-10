#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>
#include <bio.h>
#include "imagefile.h"

enum
{
	Nhash	= 4001,
	Nbuf		= 300
};

typedef struct Entry Entry;
typedef struct IO IO;


struct Entry
{
	int		index;
	int		prefix;
	int		exten;
	Entry	*next;
};

struct IO
{
	Biobuf	*fd;
	uchar	buf[Nbuf];
	int		i;
	int		nbits;	/* bits in right side of shift register */
	int		sreg;		/* shift register */
};

static Rectangle	mainrect;
static Entry	tbl[4096];
static uchar	*colormap[5];	/* one for each ldepth: GREY1 GREY2 GREY4 CMAP8=rgbv plus GREY8 */
#define	GREYMAP	4
static int		colormapsize[] = { 2, 4, 16, 256, 256 };	/* 2 for zero is an odd property of GIF */

static void		writeheader(Biobuf*, Rectangle, int, ulong, int);
static void		writedescriptor(Biobuf*, Rectangle);
static char*	writedata(Biobuf*, Image*, Memimage*);
static void		writecomment(Biobuf *fd, char*);
static void		writegraphiccontrol(Biobuf *fd, int, int);
static void*	gifmalloc(ulong);
static void		encode(Biobuf*, Rectangle, int, uchar*, uint);

static
char*
startgif0(Biobuf *fd, ulong chan, Rectangle r, int depth, int loopcount)
{
	int i;

	for(i=0; i<nelem(tbl); i++){
		tbl[i].index = i;
		tbl[i].prefix = -1;
		tbl[i].exten = i;
	}

	switch(chan){
	case GREY1:
	case GREY2:
	case GREY4:
	case CMAP8:
	case GREY8:
		break;
	default:
		return "WriteGIF: can't handle channel type";
	}

	mainrect = r;
	writeheader(fd, r, depth, chan, loopcount);
	return nil;
}

char*
startgif(Biobuf *fd, Image *image, int loopcount)
{
	return startgif0(fd, image->chan, image->r, image->depth, loopcount);
}

char*
memstartgif(Biobuf *fd, Memimage *memimage, int loopcount)
{
	return startgif0(fd, memimage->chan, memimage->r, memimage->depth, loopcount);
}

static
char*
writegif0(Biobuf *fd, Image *image, Memimage *memimage, ulong chan, Rectangle r, char *comment, int dt, int trans)
{
	char *err;

	switch(chan){
	case GREY1:
	case GREY2:
	case GREY4:
	case CMAP8:
	case GREY8:
		break;
	default:
		return "WriteGIF: can't handle channel type";
	}

	writecomment(fd, comment);
	writegraphiccontrol(fd, dt, trans);
	writedescriptor(fd, r);

	err = writedata(fd, image, memimage);
	if(err != nil)
		return err;

	return nil;
}

char*
writegif(Biobuf *fd, Image *image, char *comment, int dt, int trans)
{
	return writegif0(fd, image, nil, image->chan, image->r, comment, dt, trans);
}

char*
memwritegif(Biobuf *fd, Memimage *memimage, char *comment, int dt, int trans)
{
	return writegif0(fd, nil, memimage, memimage->chan, memimage->r, comment, dt, trans);
}

/*
 * Write little-endian 16-bit integer
 */
static
void
put2(Biobuf *fd, int i)
{
	Bputc(fd, i);
	Bputc(fd, i>>8);
}

/*
 * Get color map for all ldepths, in format suitable for writing out
 */
static
void
getcolormap(void)
{
	int i, col;
	ulong rgb;
	uchar *c;

	if(colormap[0] != nil)
		return;
	for(i=0; i<nelem(colormap); i++)
		colormap[i] = gifmalloc(3* colormapsize[i]);
	c = colormap[GREYMAP];	/* GREY8 */
	for(i=0; i<256; i++){
		c[3*i+0] = i;	/* red */
		c[3*i+1] = i;	/* green */
		c[3*i+2] = i;	/* blue */
	}
	c = colormap[3];	/* RGBV */
	for(i=0; i<256; i++){
		rgb = cmap2rgb(i);
		c[3*i+0] = (rgb>>16) & 0xFF;	/* red */
		c[3*i+1] = (rgb>> 8) & 0xFF;	/* green */
		c[3*i+2] = (rgb>> 0) & 0xFF;	/* blue */
	}
	c = colormap[2];	/* GREY4 */
	for(i=0; i<16; i++){
		col = (i<<4)|i;
		rgb = cmap2rgb(col);
		c[3*i+0] = (rgb>>16) & 0xFF;	/* red */
		c[3*i+1] = (rgb>> 8) & 0xFF;	/* green */
		c[3*i+2] = (rgb>> 0) & 0xFF;	/* blue */
	}
	c = colormap[1];	/* GREY2 */
	for(i=0; i<4; i++){
		col = (i<<6)|(i<<4)|(i<<2)|i;
		rgb = cmap2rgb(col);
		c[3*i+0] = (rgb>>16) & 0xFF;	/* red */
		c[3*i+1] = (rgb>> 8) & 0xFF;	/* green */
		c[3*i+2] = (rgb>> 0) & 0xFF;	/* blue */
	}
	c = colormap[0];	/* GREY1 */
	for(i=0; i<2; i++){
		if(i == 0)
			col = 0;
		else
			col = 0xFF;
		rgb = cmap2rgb(col);
		c[3*i+0] = (rgb>>16) & 0xFF;	/* red */
		c[3*i+1] = (rgb>> 8) & 0xFF;	/* green */
		c[3*i+2] = (rgb>> 0) & 0xFF;	/* blue */
	}
}

/*
 * Write header, logical screen descriptor, and color map
 */
static
void
writeheader(Biobuf *fd, Rectangle r, int depth, ulong chan, int loopcount)
{
	/* Header */
	Bprint(fd, "%s", "GIF89a");

	/*  Logical Screen Descriptor */
	put2(fd, Dx(r));
	put2(fd, Dy(r));

	/* Color table present, 4 bits per color (for RGBV best case), size of color map */
	Bputc(fd, (1<<7)|(3<<4)|(depth-1));	/* not right for GREY8, but GIF doesn't let us specify enough bits */
	Bputc(fd, 0xFF);	/* white background (doesn't matter anyway) */
	Bputc(fd, 0);	/* pixel aspect ratio - unused */

	/* Global Color Table */
	getcolormap();
	if(chan == GREY8)
		depth = GREYMAP;
	else
		depth = drawlog2[depth];
	Bwrite(fd, colormap[depth], 3*colormapsize[depth]);

	if(loopcount >= 0){	/* hard-to-discover way to force cycled animation */
		/* Application Extension with (1 loopcountlo loopcounthi) as data */
		Bputc(fd, 0x21);
		Bputc(fd, 0xFF);
		Bputc(fd, 11);
		Bwrite(fd, "NETSCAPE2.0", 11);
		Bputc(fd, 3);
		Bputc(fd, 1);
		put2(fd, loopcount);
		Bputc(fd, 0);
	}
}

/*
 * Write optional comment block
 */
static
void
writecomment(Biobuf *fd, char *comment)
{
	int n;

	if(comment==nil || comment[0]=='\0')
		return;

	/* Comment extension and label */
	Bputc(fd, 0x21);
	Bputc(fd, 0xFE);

	/* Comment data */
	n = strlen(comment);
	if(n > 255)
		n = 255;
	Bputc(fd, n);
	Bwrite(fd, comment, n);

	/* Block terminator */
	Bputc(fd, 0x00);
}

/*
 * Write optional control block (sets Delay Time)
 */
static
void
writegraphiccontrol(Biobuf *fd, int dt, int trans)
{
	if(dt < 0 && trans < 0)
		return;

	/* Comment extension and label and block size*/
	Bputc(fd, 0x21);
	Bputc(fd, 0xF9);
	Bputc(fd, 0x04);

	/* Disposal method and other flags (none) */
	if(trans >= 0)
		Bputc(fd, 0x01);
	else
		Bputc(fd, 0x00);

	/* Delay time, in centisec (argument is millisec for sanity) */
	if(dt < 0)
		dt = 0;
	else if(dt < 10)
		dt = 1;
	else
		dt = (dt+5)/10;
	put2(fd, dt);

	/* Transparency index */
	if(trans < 0)
		trans = 0;
	Bputc(fd, trans);

	/* Block terminator */
	Bputc(fd, 0x00);
}

/*
 * Write image descriptor
 */
static
void
writedescriptor(Biobuf *fd, Rectangle r)
{
	/* Image Separator */
	Bputc(fd, 0x2C);

	/* Left, top, width, height */
	put2(fd, r.min.x-mainrect.min.x);
	put2(fd, r.min.y-mainrect.min.y);
	put2(fd, Dx(r));
	put2(fd, Dy(r));
	/* no special processing */
	Bputc(fd, 0);
}

/*
 * Write data
 */
static
char*
writedata(Biobuf *fd, Image *image, Memimage *memimage)
{
	char *err;
	uchar *data;
	int ndata, depth;
	Rectangle r;

	if(memimage != nil){
		r = memimage->r;
		depth = memimage->depth;
	}else{
		r = image->r;
		depth = image->depth;
	}

	/* LZW Minimum code size */
	if(depth == 1)
		Bputc(fd, 2);
	else
		Bputc(fd, depth);

	/*
	 * Read image data into memory
	 * potentially one extra byte on each end of each scan line
	 */
	ndata = Dy(r)*(2+(Dx(r)>>(3-drawlog2[depth])));
	data = gifmalloc(ndata);
	if(memimage != nil)
		ndata = unloadmemimage(memimage, r, data, ndata);
	else
		ndata = unloadimage(image, r, data, ndata);
	if(ndata < 0){
		err = gifmalloc(ERRMAX);
		snprint(err, ERRMAX, "WriteGIF: %r");
		free(data);
		return err;
	}

	/* Encode and emit the data */
	encode(fd, r, depth, data, ndata);
	free(data);

	/*  Block Terminator */
	Bputc(fd, 0);
	return nil;
}

/*
 * Write trailer
 */
void
endgif(Biobuf *fd)
{
	Bputc(fd, 0x3B);
	Bflush(fd);
}

void
memendgif(Biobuf *fd)
{
	endgif(fd);
}

/*
 * Put n bits of c into output at io.buf[i];
 */
static
void
output(IO *io, int c, int n)
{
	if(c < 0){
		if(io->nbits != 0)
			io->buf[io->i++] = io->sreg;
		Bputc(io->fd, io->i);
		Bwrite(io->fd, io->buf, io->i);
		io->nbits = 0;
		return;
	}

	if(io->nbits+n >= 31){
		fprint(2, "panic: WriteGIF sr overflow\n");
		exits("WriteGIF panic");
	}
	io->sreg |= c<<io->nbits;
	io->nbits += n;

	while(io->nbits >= 8){
		io->buf[io->i++] = io->sreg;
		io->sreg >>= 8;
		io->nbits -= 8;
	}

	if(io->i >= 255){
		Bputc(io->fd, 255);
		Bwrite(io->fd, io->buf, 255);
		memmove(io->buf, io->buf+255, io->i-255);
		io->i -= 255;
	}
}

/*
 * LZW encoder
 */
static
void
encode(Biobuf *fd, Rectangle r, int depth, uchar *data, uint ndata)
{
	int i, c, h, csize, prefix, first, sreg, nbits, bitsperpixel;
	int CTM, EOD, codesize, ld0, datai, x, ld, pm;
	int nentry, maxentry, early;
	Entry *e, *oe;
	IO *io;
	Entry **hash;

	first = 1;
	ld = drawlog2[depth];
	/* ldepth 0 must generate codesize 2 with values 0 and 1 (see the spec.) */
	ld0 = ld;
	if(ld0 == 0)
		ld0 = 1;
	codesize = (1<<ld0);
	CTM = 1<<codesize;
	EOD = CTM+1;

	io = gifmalloc(sizeof(IO));
	io->fd = fd;
	sreg = 0;
	nbits = 0;
	bitsperpixel = 1<<ld;
	pm = (1<<bitsperpixel)-1;

	datai = 0;
	x = r.min.x;
	hash = gifmalloc(Nhash*sizeof(Entry*));

Init:
	memset(hash, 0, Nhash*sizeof(Entry*));
	csize = codesize+1;
	nentry = EOD+1;
	maxentry = (1<<csize);
	for(i = 0; i<nentry; i++){
		e = &tbl[i];
		h = (e->prefix<<24) | (e->exten<<8);
		h %= Nhash;
		if(h < 0)
			h += Nhash;
		e->next = hash[h];
		hash[h] = e;
	}
	prefix = -1;
	if(first)
		output(io, CTM, csize);
	first = 0;

	/*
	 * Scan over pixels.  Because of partially filled bytes on ends of scan lines,
	 * which must be ignored in the data stream passed to GIF, this is more
	 * complex than we'd like.
	 */
Next:
	for(;;){
		if(ld != 3){
			/* beginning of scan line is difficult; prime the shift register */
			if(x == r.min.x){
				if(datai == ndata)
					break;
				sreg = data[datai++];
				nbits = 8-((x&(7>>ld))<<ld);
			}
			x++;
			if(x == r.max.x)
				x = r.min.x;
		}
		if(nbits == 0){
			if(datai == ndata)
				break;
			sreg = data[datai++];
			nbits = 8;
		}
		nbits -= bitsperpixel;
		c = sreg>>nbits & pm;
		h = prefix<<24 | c<<8;
		h %= Nhash;
		if(h < 0)
			h += Nhash;
		oe = nil;
		for(e = hash[h]; e!=nil; e=e->next){
			if(e->prefix == prefix && e->exten == c){
				if(oe != nil){
					oe->next = e->next;
					e->next = hash[h];
					hash[h] = e;
				}
				prefix = e->index;
				goto Next;
			}
			oe = e;
		}

		output(io, prefix, csize);
		early = 0; /* peculiar tiff feature here for reference */
		if(nentry == maxentry-early){
			if(csize == 12){
				nbits += bitsperpixel;	/* unget pixel */
				x--;
				if(ld != 3 && x == r.min.x)
					datai--;
				output(io, CTM, csize);
				goto Init;
			}
			csize++;
			maxentry = (1<<csize);
		}

		e = &tbl[nentry];
		e->prefix = prefix;
		e->exten = c;
		e->next = hash[h];
		hash[h] = e;

		prefix = c;
		nentry++;
	}

	output(io, prefix, csize);
	output(io, EOD, csize);
	output(io, -1, csize);
	free(io);
	free(hash);
}

static
void*
gifmalloc(ulong sz)
{
	void *v;
	v = malloc(sz);
	if(v == nil) {
		fprint(2, "WriteGIF: out of memory allocating %ld\n", sz);
abort();
		exits("mem");
	}
	memset(v, 0, sz);
	return v;
}
