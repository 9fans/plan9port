#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

int drawdebug;
static int	tablesbuilt;

/* perfect approximation to NTSC = .299r+.587g+.114b when 0 ≤ r,g,b < 256 */
#define RGB2K(r,g,b)	((156763*(r)+307758*(g)+59769*(b))>>19)

/*
 * For 16-bit values, x / 255 == (t = x+1, (t+(t>>8)) >> 8).
 * We add another 127 to round to the nearest value rather
 * than truncate.
 *
 * CALCxy does x bytewise calculations on y input images (x=1,4; y=1,2).
 * CALC2x does two parallel 16-bit calculations on y input images (y=1,2).
 */
#define CALC11(a, v, tmp) \
	(tmp=(a)*(v)+128, (tmp+(tmp>>8))>>8)

#define CALC12(a1, v1, a2, v2, tmp) \
	(tmp=(a1)*(v1)+(a2)*(v2)+128, (tmp+(tmp>>8))>>8)

#define MASK 0xFF00FF

#define CALC21(a, vvuu, tmp) \
	(tmp=(a)*(vvuu)+0x00800080, ((tmp+((tmp>>8)&MASK))>>8)&MASK)

#define CALC41(a, rgba, tmp1, tmp2) \
	(CALC21(a, rgba & MASK, tmp1) | \
	 (CALC21(a, (rgba>>8)&MASK, tmp2)<<8))

#define CALC22(a1, vvuu1, a2, vvuu2, tmp) \
	(tmp=(a1)*(vvuu1)+(a2)*(vvuu2)+0x00800080, ((tmp+((tmp>>8)&MASK))>>8)&MASK)

#define CALC42(a1, rgba1, a2, rgba2, tmp1, tmp2) \
	(CALC22(a1, rgba1 & MASK, a2, rgba2 & MASK, tmp1) | \
	 (CALC22(a1, (rgba1>>8) & MASK, a2, (rgba2>>8) & MASK, tmp2)<<8))

static void mktables(void);
typedef int Subdraw(Memdrawparam*);
static Subdraw chardraw, alphadraw, memoptdraw;

static Memimage*	memones;
static Memimage*	memzeros;
Memimage *memwhite;
Memimage *memblack;
Memimage *memtransparent;
Memimage *memopaque;

int	__ifmt(Fmt*);

void
memimageinit(void)
{
	static int didinit = 0;

	if(didinit)
		return;

	didinit = 1;

	mktables();
	_memmkcmap();

	fmtinstall('R', Rfmt);
	fmtinstall('P', Pfmt);
	fmtinstall('b', __ifmt);

	memones = allocmemimage(Rect(0,0,1,1), GREY1);
	memones->flags |= Frepl;
	memones->clipr = Rect(-0x3FFFFFF, -0x3FFFFFF, 0x3FFFFFF, 0x3FFFFFF);
	*byteaddr(memones, ZP) = ~0;

	memzeros = allocmemimage(Rect(0,0,1,1), GREY1);
	memzeros->flags |= Frepl;
	memzeros->clipr = Rect(-0x3FFFFFF, -0x3FFFFFF, 0x3FFFFFF, 0x3FFFFFF);
	*byteaddr(memzeros, ZP) = 0;

	if(memones == nil || memzeros == nil)
		assert(0 /*cannot initialize memimage library */);	/* RSC BUG */

	memwhite = memones;
	memblack = memzeros;
	memopaque = memones;
	memtransparent = memzeros;
}

u32int _imgtorgba(Memimage*, u32int);
u32int _rgbatoimg(Memimage*, u32int);
u32int _pixelbits(Memimage*, Point);

#define DBG if(drawdebug)
static Memdrawparam par;

Memdrawparam*
_memimagedrawsetup(Memimage *dst, Rectangle r, Memimage *src, Point p0, Memimage *mask, Point p1, int op)
{
	if(mask == nil)
		mask = memopaque;

DBG	print("memimagedraw %p/%luX %R @ %p %p/%luX %P %p/%luX %P... ", dst, dst->chan, r, dst->data->bdata, src, src->chan, p0, mask, mask->chan, p1);

	if(drawclip(dst, &r, src, &p0, mask, &p1, &par.sr, &par.mr) == 0){
/*		if(drawdebug) */
/*			iprint("empty clipped rectangle\n"); */
		return nil;
	}

	if(op < Clear || op > SoverD){
/*		if(drawdebug) */
/*			iprint("op out of range: %d\n", op); */
		return nil;
	}

	par.op = op;
	par.dst = dst;
	par.r = r;
	par.src = src;
	/* par.sr set by drawclip */
	par.mask = mask;
	/* par.mr set by drawclip */

	par.state = 0;
	if(src->flags&Frepl){
		par.state |= Replsrc;
		if(Dx(src->r)==1 && Dy(src->r)==1){
			par.sval = pixelbits(src, src->r.min);
			par.state |= Simplesrc;
			par.srgba = _imgtorgba(src, par.sval);
			par.sdval = _rgbatoimg(dst, par.srgba);
			if((par.srgba&0xFF) == 0 && (op&DoutS)){
/*				if (drawdebug) iprint("fill with transparent source\n"); */
				return nil;	/* no-op successfully handled */
			}
			if((par.srgba&0xFF) == 0xFF)
				par.state |= Fullsrc;
		}
	}

	if(mask->flags & Frepl){
		par.state |= Replmask;
		if(Dx(mask->r)==1 && Dy(mask->r)==1){
			par.mval = pixelbits(mask, mask->r.min);
			if(par.mval == 0 && (op&DoutS)){
/*				if(drawdebug) iprint("fill with zero mask\n"); */
				return nil;	/* no-op successfully handled */
			}
			par.state |= Simplemask;
			if(par.mval == ~0)
				par.state |= Fullmask;
			par.mrgba = _imgtorgba(mask, par.mval);
		}
	}

/*	if(drawdebug) */
/*		iprint("dr %R sr %R mr %R...", r, par.sr, par.mr); */
DBG print("draw dr %R sr %R mr %R %lux\n", r, par.sr, par.mr, par.state);

	return &par;
}

void
_memimagedraw(Memdrawparam *par)
{
	/*
	 * Now that we've clipped the parameters down to be consistent, we
	 * simply try sub-drawing routines in order until we find one that was able
	 * to handle us.  If the sub-drawing routine returns zero, it means it was
	 * unable to satisfy the request, so we do not return.
	 */

	/*
	 * Hardware support.  Each video driver provides this function,
	 * which checks to see if there is anything it can help with.
	 * There could be an if around this checking to see if dst is in video memory.
	 */
DBG print("test hwdraw\n");
	if(hwdraw(par)){
/*if(drawdebug) iprint("hw handled\n"); */
DBG print("hwdraw handled\n");
		return;
	}
	/*
	 * Optimizations using memmove and memset.
	 */
DBG print("test memoptdraw\n");
	if(memoptdraw(par)){
/*if(drawdebug) iprint("memopt handled\n"); */
DBG print("memopt handled\n");
		return;
	}

	/*
	 * Character drawing.
	 * Solid source color being painted through a boolean mask onto a high res image.
	 */
DBG print("test chardraw\n");
	if(chardraw(par)){
/*if(drawdebug) iprint("chardraw handled\n"); */
DBG print("chardraw handled\n");
		return;
	}

	/*
	 * General calculation-laden case that does alpha for each pixel.
	 */
DBG print("do alphadraw\n");
	alphadraw(par);
/*if(drawdebug) iprint("alphadraw handled\n"); */
DBG print("alphadraw handled\n");
}
#undef DBG

/*
 * Clip the destination rectangle further based on the properties of the
 * source and mask rectangles.  Once the destination rectangle is properly
 * clipped, adjust the source and mask rectangles to be the same size.
 * Then if source or mask is replicated, move its clipped rectangle
 * so that its minimum point falls within the repl rectangle.
 *
 * Return zero if the final rectangle is null.
 */
int
drawclip(Memimage *dst, Rectangle *r, Memimage *src, Point *p0, Memimage *mask, Point *p1, Rectangle *sr, Rectangle *mr)
{
	Point rmin, delta;
	int splitcoords;
	Rectangle omr;

	if(r->min.x>=r->max.x || r->min.y>=r->max.y)
		return 0;
	splitcoords = (p0->x!=p1->x) || (p0->y!=p1->y);
	/* clip to destination */
	rmin = r->min;
	if(!rectclip(r, dst->r) || !rectclip(r, dst->clipr))
		return 0;
	/* move mask point */
	p1->x += r->min.x-rmin.x;
	p1->y += r->min.y-rmin.y;
	/* move source point */
	p0->x += r->min.x-rmin.x;
	p0->y += r->min.y-rmin.y;
	/* map destination rectangle into source */
	sr->min = *p0;
	sr->max.x = p0->x+Dx(*r);
	sr->max.y = p0->y+Dy(*r);
	/* sr is r in source coordinates; clip to source */
	if(!(src->flags&Frepl) && !rectclip(sr, src->r))
		return 0;
	if(!rectclip(sr, src->clipr))
		return 0;
	/* compute and clip rectangle in mask */
	if(splitcoords){
		/* move mask point with source */
		p1->x += sr->min.x-p0->x;
		p1->y += sr->min.y-p0->y;
		mr->min = *p1;
		mr->max.x = p1->x+Dx(*sr);
		mr->max.y = p1->y+Dy(*sr);
		omr = *mr;
		/* mr is now rectangle in mask; clip it */
		if(!(mask->flags&Frepl) && !rectclip(mr, mask->r))
			return 0;
		if(!rectclip(mr, mask->clipr))
			return 0;
		/* reflect any clips back to source */
		sr->min.x += mr->min.x-omr.min.x;
		sr->min.y += mr->min.y-omr.min.y;
		sr->max.x += mr->max.x-omr.max.x;
		sr->max.y += mr->max.y-omr.max.y;
		*p1 = mr->min;
	}else{
		if(!(mask->flags&Frepl) && !rectclip(sr, mask->r))
			return 0;
		if(!rectclip(sr, mask->clipr))
			return 0;
		*p1 = sr->min;
	}

	/* move source clipping back to destination */
	delta.x = r->min.x - p0->x;
	delta.y = r->min.y - p0->y;
	r->min.x = sr->min.x + delta.x;
	r->min.y = sr->min.y + delta.y;
	r->max.x = sr->max.x + delta.x;
	r->max.y = sr->max.y + delta.y;

	/* move source rectangle so sr->min is in src->r */
	if(src->flags&Frepl) {
		delta.x = drawreplxy(src->r.min.x, src->r.max.x, sr->min.x) - sr->min.x;
		delta.y = drawreplxy(src->r.min.y, src->r.max.y, sr->min.y) - sr->min.y;
		sr->min.x += delta.x;
		sr->min.y += delta.y;
		sr->max.x += delta.x;
		sr->max.y += delta.y;
	}
	*p0 = sr->min;

	/* move mask point so it is in mask->r */
	*p1 = drawrepl(mask->r, *p1);
	mr->min = *p1;
	mr->max.x = p1->x+Dx(*sr);
	mr->max.y = p1->y+Dy(*sr);

	assert(Dx(*sr) == Dx(*mr) && Dx(*mr) == Dx(*r));
	assert(Dy(*sr) == Dy(*mr) && Dy(*mr) == Dy(*r));
	assert(ptinrect(*p0, src->r));
	assert(ptinrect(*p1, mask->r));
	assert(ptinrect(r->min, dst->r));

	return 1;
}

/*
 * Conversion tables.
 */
static uchar replbit[1+8][256];		/* replbit[x][y] is the replication of the x-bit quantity y to 8-bit depth */
static uchar conv18[256][8];		/* conv18[x][y] is the yth pixel in the depth-1 pixel x */
static uchar conv28[256][4];		/* ... */
static uchar conv48[256][2];

/*
 * bitmap of how to replicate n bits to fill 8, for 1 ≤ n ≤ 8.
 * the X's are where to put the bottom (ones) bit of the n-bit pattern.
 * only the top 8 bits of the result are actually used.
 * (the lower 8 bits are needed to get bits in the right place
 * when n is not a divisor of 8.)
 *
 * Should check to see if its easier to just refer to replmul than
 * use the precomputed values in replbit.  On PCs it may well
 * be; on machines with slow multiply instructions it probably isn't.
 */
#define a ((((((((((((((((0
#define X *2+1)
#define _ *2)
static int replmul[1+8] = {
	0,
	a X X X X X X X X X X X X X X X X,
	a _ X _ X _ X _ X _ X _ X _ X _ X,
	a _ _ X _ _ X _ _ X _ _ X _ _ X _,
	a _ _ _ X _ _ _ X _ _ _ X _ _ _ X,
	a _ _ _ _ X _ _ _ _ X _ _ _ _ X _,
	a _ _ _ _ _ X _ _ _ _ _ X _ _ _ _,
	a _ _ _ _ _ _ X _ _ _ _ _ _ X _ _,
	a _ _ _ _ _ _ _ X _ _ _ _ _ _ _ X,
};
#undef a
#undef X
#undef _

static void
mktables(void)
{
	int i, j, mask, sh, small;

	if(tablesbuilt)
		return;

	fmtinstall('R', Rfmt);
	fmtinstall('P', Pfmt);
	tablesbuilt = 1;

	/* bit replication up to 8 bits */
	for(i=0; i<256; i++){
		for(j=0; j<=8; j++){	/* j <= 8 [sic] */
			small = i & ((1<<j)-1);
			replbit[j][i] = (small*replmul[j])>>8;
		}
	}

	/* bit unpacking up to 8 bits, only powers of 2 */
	for(i=0; i<256; i++){
		for(j=0, sh=7, mask=1; j<8; j++, sh--)
			conv18[i][j] = replbit[1][(i>>sh)&mask];

		for(j=0, sh=6, mask=3; j<4; j++, sh-=2)
			conv28[i][j] = replbit[2][(i>>sh)&mask];

		for(j=0, sh=4, mask=15; j<2; j++, sh-=4)
			conv48[i][j] = replbit[4][(i>>sh)&mask];
	}
}

static uchar ones = 0xff;

/*
 * General alpha drawing case.  Can handle anything.
 */
typedef struct	Buffer	Buffer;
struct Buffer {
	/* used by most routines */
	uchar	*red;
	uchar	*grn;
	uchar	*blu;
	uchar	*alpha;
	uchar	*grey;
	u32int	*rgba;
	int	delta;	/* number of bytes to add to pointer to get next pixel to the right */

	/* used by boolcalc* for mask data */
	uchar	*m;		/* ptr to mask data r.min byte; like p->bytermin */
	int		mskip;	/* no. of left bits to skip in *m */
	uchar	*bm;		/* ptr to mask data img->r.min byte; like p->bytey0s */
	int		bmskip;	/* no. of left bits to skip in *bm */
	uchar	*em;		/* ptr to mask data img->r.max.x byte; like p->bytey0e */
	int		emskip;	/* no. of right bits to skip in *em */
};

typedef struct	Param	Param;
typedef Buffer	Readfn(Param*, uchar*, int);
typedef void	Writefn(Param*, uchar*, Buffer);
typedef Buffer	Calcfn(Buffer, Buffer, Buffer, int, int, int);

enum {
	MAXBCACHE = 16
};

/* giant rathole to customize functions with */
struct Param {
	Readfn	*replcall;
	Readfn	*greymaskcall;
	Readfn	*convreadcall;
	Writefn	*convwritecall;

	Memimage *img;
	Rectangle	r;
	int	dx;	/* of r */
	int	needbuf;
	int	convgrey;
	int	alphaonly;

	uchar	*bytey0s;		/* byteaddr(Pt(img->r.min.x, img->r.min.y)) */
	uchar	*bytermin;	/* byteaddr(Pt(r.min.x, img->r.min.y)) */
	uchar	*bytey0e;		/* byteaddr(Pt(img->r.max.x, img->r.min.y)) */
	int		bwidth;

	int	replcache;	/* if set, cache buffers */
	Buffer	bcache[MAXBCACHE];
	u32int	bfilled;
	uchar	*bufbase;
	int	bufoff;
	int	bufdelta;

	int	dir;

	int	convbufoff;
	uchar	*convbuf;
	Param	*convdpar;
	int	convdx;
};

static uchar *drawbuf;
static int	ndrawbuf;
static int	mdrawbuf;
static Param spar, mpar, dpar;	/* easier on the stacks */
static Readfn	greymaskread, replread, readptr;
static Writefn	nullwrite;
static Calcfn	alphacalc0, alphacalc14, alphacalc2810, alphacalc3679, alphacalc5, alphacalc11, alphacalcS;
static Calcfn	boolcalc14, boolcalc236789, boolcalc1011;

static Readfn*	readfn(Memimage*);
static Readfn*	readalphafn(Memimage*);
static Writefn*	writefn(Memimage*);

static Calcfn*	boolcopyfn(Memimage*, Memimage*);
static Readfn*	convfn(Memimage*, Param*, Memimage*, Param*);

static Calcfn *alphacalc[Ncomp] =
{
	alphacalc0,		/* Clear */
	alphacalc14,		/* DoutS */
	alphacalc2810,		/* SoutD */
	alphacalc3679,		/* DxorS */
	alphacalc14,		/* DinS */
	alphacalc5,		/* D */
	alphacalc3679,		/* DatopS */
	alphacalc3679,		/* DoverS */
	alphacalc2810,		/* SinD */
	alphacalc3679,		/* SatopD */
	alphacalc2810,		/* S */
	alphacalc11,		/* SoverD */
};

static Calcfn *boolcalc[Ncomp] =
{
	alphacalc0,		/* Clear */
	boolcalc14,		/* DoutS */
	boolcalc236789,		/* SoutD */
	boolcalc236789,		/* DxorS */
	boolcalc14,		/* DinS */
	alphacalc5,		/* D */
	boolcalc236789,		/* DatopS */
	boolcalc236789,		/* DoverS */
	boolcalc236789,		/* SinD */
	boolcalc236789,		/* SatopD */
	boolcalc1011,		/* S */
	boolcalc1011,		/* SoverD */
};

static int
allocdrawbuf(void)
{
	uchar *p;

	if(ndrawbuf > mdrawbuf){
		p = realloc(drawbuf, ndrawbuf);
		if(p == nil){
			werrstr("memimagedraw out of memory");
			return -1;
		}
		drawbuf = p;
		mdrawbuf = ndrawbuf;
	}
	return 0;
}

static void
getparam(Param *p, Memimage *img, Rectangle r, int convgrey, int needbuf)
{
	int nbuf;

	memset(p, 0, sizeof *p);

	p->img = img;
	p->r = r;
	p->dx = Dx(r);
	p->needbuf = needbuf;
	p->convgrey = convgrey;

	assert(img->r.min.x <= r.min.x && r.min.x < img->r.max.x);

	p->bytey0s = byteaddr(img, Pt(img->r.min.x, img->r.min.y));
	p->bytermin = byteaddr(img, Pt(r.min.x, img->r.min.y));
	p->bytey0e = byteaddr(img, Pt(img->r.max.x, img->r.min.y));
	p->bwidth = sizeof(u32int)*img->width;

	assert(p->bytey0s <= p->bytermin && p->bytermin <= p->bytey0e);

	if(p->r.min.x == p->img->r.min.x)
		assert(p->bytermin == p->bytey0s);

	nbuf = 1;
	if((img->flags&Frepl) && Dy(img->r) <= MAXBCACHE && Dy(img->r) < Dy(r)){
		p->replcache = 1;
		nbuf = Dy(img->r);
	}
	p->bufdelta = 4*p->dx;
	p->bufoff = ndrawbuf;
	ndrawbuf += p->bufdelta*nbuf;
}

static void
clipy(Memimage *img, int *y)
{
	int dy;

	dy = Dy(img->r);
	if(*y == dy)
		*y = 0;
	else if(*y == -1)
		*y = dy-1;
	assert(0 <= *y && *y < dy);
}

static void
dumpbuf(char *s, Buffer b, int n)
{
	int i;
	uchar *p;

	print("%s", s);
	for(i=0; i<n; i++){
		print(" ");
		if(p=b.grey){
			print(" k%.2uX", *p);
			b.grey += b.delta;
		}else{
			if(p=b.red){
				print(" r%.2uX", *p);
				b.red += b.delta;
			}
			if(p=b.grn){
				print(" g%.2uX", *p);
				b.grn += b.delta;
			}
			if(p=b.blu){
				print(" b%.2uX", *p);
				b.blu += b.delta;
			}
		}
		if((p=b.alpha) != &ones){
			print(" α%.2uX", *p);
			b.alpha += b.delta;
		}
	}
	print("\n");
}

/*
 * For each scan line, we expand the pixels from source, mask, and destination
 * into byte-aligned red, green, blue, alpha, and grey channels.  If buffering is not
 * needed and the channels were already byte-aligned (grey8, rgb24, rgba32, rgb32),
 * the readers need not copy the data: they can simply return pointers to the data.
 * If the destination image is grey and the source is not, it is converted using the NTSC
 * formula.
 *
 * Once we have all the channels, we call either rgbcalc or greycalc, depending on
 * whether the destination image is color.  This is allowed to overwrite the dst buffer (perhaps
 * the actual data, perhaps a copy) with its result.  It should only overwrite the dst buffer
 * with the same format (i.e. red bytes with red bytes, etc.)  A new buffer is returned from
 * the calculator, and that buffer is passed to a function to write it to the destination.
 * If the buffer is already pointing at the destination, the writing function is a no-op.
 */
#define DBG if(drawdebug)
static int
alphadraw(Memdrawparam *par)
{
	int isgrey, starty, endy, op;
	int needbuf, dsty, srcy, masky;
	int y, dir, dx, dy;
	Buffer bsrc, bdst, bmask;
	Readfn *rdsrc, *rdmask, *rddst;
	Calcfn *calc;
	Writefn *wrdst;
	Memimage *src, *mask, *dst;
	Rectangle r, sr, mr;

	if(drawdebug)
		print("alphadraw %R\n", par->r);
	r = par->r;
	dx = Dx(r);
	dy = Dy(r);

	ndrawbuf = 0;

	src = par->src;
	mask = par->mask;
	dst = par->dst;
	sr = par->sr;
	mr = par->mr;
	op = par->op;

	isgrey = dst->flags&Fgrey;

	/*
	 * Buffering when src and dst are the same bitmap is sufficient but not
	 * necessary.  There are stronger conditions we could use.  We could
	 * check to see if the rectangles intersect, and if simply moving in the
	 * correct y direction can avoid the need to buffer.
	 */
	needbuf = (src->data == dst->data);

	getparam(&spar, src, sr, isgrey, needbuf);
	getparam(&dpar, dst, r, isgrey, needbuf);
	getparam(&mpar, mask, mr, 0, needbuf);

	dir = (needbuf && byteaddr(dst, r.min) > byteaddr(src, sr.min)) ? -1 : 1;
	spar.dir = mpar.dir = dpar.dir = dir;

	/*
	 * If the mask is purely boolean, we can convert from src to dst format
	 * when we read src, and then just copy it to dst where the mask tells us to.
	 * This requires a boolean (1-bit grey) mask and lack of a source alpha channel.
	 *
	 * The computation is accomplished by assigning the function pointers as follows:
	 *	rdsrc - read and convert source into dst format in a buffer
	 * 	rdmask - convert mask to bytes, set pointer to it
	 * 	rddst - fill with pointer to real dst data, but do no reads
	 *	calc - copy src onto dst when mask says to.
	 *	wrdst - do nothing
	 * This is slightly sleazy, since things aren't doing exactly what their names say,
	 * but it avoids a fair amount of code duplication to make this a case here
	 * rather than have a separate booldraw.
	 */
/*if(drawdebug) iprint("flag %lud mchan %lux=?%x dd %d\n", src->flags&Falpha, mask->chan, GREY1, dst->depth); */
	if(!(src->flags&Falpha) && mask->chan == GREY1 && dst->depth >= 8 && op == SoverD){
/*if(drawdebug) iprint("boolcopy..."); */
		rdsrc = convfn(dst, &dpar, src, &spar);
		rddst = readptr;
		rdmask = readfn(mask);
		calc = boolcopyfn(dst, mask);
		wrdst = nullwrite;
	}else{
		/* usual alphadraw parameter fetching */
		rdsrc = readfn(src);
		rddst = readfn(dst);
		wrdst = writefn(dst);
		calc = alphacalc[op];

		/*
		 * If there is no alpha channel, we'll ask for a grey channel
		 * and pretend it is the alpha.
		 */
		if(mask->flags&Falpha){
			rdmask = readalphafn(mask);
			mpar.alphaonly = 1;
		}else{
			mpar.greymaskcall = readfn(mask);
			mpar.convgrey = 1;
			rdmask = greymaskread;

			/*
			 * Should really be above, but then boolcopyfns would have
			 * to deal with bit alignment, and I haven't written that.
			 *
			 * This is a common case for things like ellipse drawing.
			 * When there's no alpha involved and the mask is boolean,
			 * we can avoid all the division and multiplication.
			 */
			if(mask->chan == GREY1 && !(src->flags&Falpha))
				calc = boolcalc[op];
			else if(op == SoverD && !(src->flags&Falpha))
				calc = alphacalcS;
		}
	}

	/*
	 * If the image has a small enough repl rectangle,
	 * we can just read each line once and cache them.
	 */
	if(spar.replcache){
		spar.replcall = rdsrc;
		rdsrc = replread;
	}
	if(mpar.replcache){
		mpar.replcall = rdmask;
		rdmask = replread;
	}

	if(allocdrawbuf() < 0)
		return 0;

	/*
	 * Before we were saving only offsets from drawbuf in the parameter
	 * structures; now that drawbuf has been grown to accomodate us,
	 * we can fill in the pointers.
	 */
	spar.bufbase = drawbuf+spar.bufoff;
	mpar.bufbase = drawbuf+mpar.bufoff;
	dpar.bufbase = drawbuf+dpar.bufoff;
	spar.convbuf = drawbuf+spar.convbufoff;

	if(dir == 1){
		starty = 0;
		endy = dy;
	}else{
		starty = dy-1;
		endy = -1;
	}

	/*
	 * srcy, masky, and dsty are offsets from the top of their
	 * respective Rectangles.  they need to be contained within
	 * the rectangles, so clipy can keep them there without division.
 	 */
	srcy = (starty + sr.min.y - src->r.min.y)%Dy(src->r);
	masky = (starty + mr.min.y - mask->r.min.y)%Dy(mask->r);
	dsty = starty + r.min.y - dst->r.min.y;

	assert(0 <= srcy && srcy < Dy(src->r));
	assert(0 <= masky && masky < Dy(mask->r));
	assert(0 <= dsty && dsty < Dy(dst->r));

	if(drawdebug)
		print("alphadraw: rdsrc=%p rdmask=%p rddst=%p calc=%p wrdst=%p\n",
			rdsrc, rdmask, rddst, calc, wrdst);
	for(y=starty; y!=endy; y+=dir, srcy+=dir, masky+=dir, dsty+=dir){
		clipy(src, &srcy);
		clipy(dst, &dsty);
		clipy(mask, &masky);

		bsrc = rdsrc(&spar, spar.bufbase, srcy);
DBG print("[");
		bmask = rdmask(&mpar, mpar.bufbase, masky);
DBG print("]\n");
		bdst = rddst(&dpar, dpar.bufbase, dsty);
DBG		dumpbuf("src", bsrc, dx);
DBG		dumpbuf("mask", bmask, dx);
DBG		dumpbuf("dst", bdst, dx);
		bdst = calc(bdst, bsrc, bmask, dx, isgrey, op);
DBG		dumpbuf("bdst", bdst, dx);
		wrdst(&dpar, dpar.bytermin+dsty*dpar.bwidth, bdst);
	}

	return 1;
}
#undef DBG

static Buffer
alphacalc0(Buffer bdst, Buffer b1, Buffer b2, int dx, int grey, int op)
{
	USED(grey);
	USED(op);
	memset(bdst.rgba, 0, dx*bdst.delta);
	return bdst;
}

/*
 * Do the channels in the buffers match enough
 * that we can do word-at-a-time operations
 * on the pixels?
 */
static int
chanmatch(Buffer *bdst, Buffer *bsrc)
{
	uchar *drgb, *srgb;

	/*
	 * first, r, g, b must be in the same place
	 * in the rgba word.
	 */
	drgb = (uchar*)bdst->rgba;
	srgb = (uchar*)bsrc->rgba;
	if(bdst->red - drgb != bsrc->red - srgb
	|| bdst->blu - drgb != bsrc->blu - srgb
	|| bdst->grn - drgb != bsrc->grn - srgb)
		return 0;

	/*
	 * that implies alpha is in the same place,
	 * if it is there at all (it might be == &ones).
	 * if the destination is &ones, we can scribble
	 * over the rgba slot just fine.
	 */
	if(bdst->alpha == &ones)
		return 1;

	/*
	 * if the destination is not ones but the src is,
	 * then the simultaneous calculation will use
	 * bogus bytes from the src's rgba.  no good.
	 */
	if(bsrc->alpha == &ones)
		return 0;

	/*
	 * otherwise, alphas are in the same place.
	 */
	return 1;
}

static Buffer
alphacalc14(Buffer bdst, Buffer bsrc, Buffer bmask, int dx, int grey, int op)
{
	Buffer obdst;
	int fd, sadelta;
	int i, sa, ma, q;
	u32int t, t1;

	obdst = bdst;
	sadelta = bsrc.alpha == &ones ? 0 : bsrc.delta;
	q = bsrc.delta == 4 && bdst.delta == 4 && chanmatch(&bdst, &bsrc);

	for(i=0; i<dx; i++){
		sa = *bsrc.alpha;
		ma = *bmask.alpha;
		fd = CALC11(sa, ma, t);
		if(op == DoutS)
			fd = 255-fd;

		if(grey){
			*bdst.grey = CALC11(fd, *bdst.grey, t);
			bsrc.grey += bsrc.delta;
			bdst.grey += bdst.delta;
		}else{
			if(q){
				*bdst.rgba = CALC41(fd, *bdst.rgba, t, t1);
				bsrc.rgba++;
				bdst.rgba++;
				bsrc.alpha += sadelta;
				bmask.alpha += bmask.delta;
				continue;
			}
			*bdst.red = CALC11(fd, *bdst.red, t);
			*bdst.grn = CALC11(fd, *bdst.grn, t);
			*bdst.blu = CALC11(fd, *bdst.blu, t);
			bsrc.red += bsrc.delta;
			bsrc.blu += bsrc.delta;
			bsrc.grn += bsrc.delta;
			bdst.red += bdst.delta;
			bdst.blu += bdst.delta;
			bdst.grn += bdst.delta;
		}
		if(bdst.alpha != &ones){
			*bdst.alpha = CALC11(fd, *bdst.alpha, t);
			bdst.alpha += bdst.delta;
		}
		bmask.alpha += bmask.delta;
		bsrc.alpha += sadelta;
	}
	return obdst;
}

static Buffer
alphacalc2810(Buffer bdst, Buffer bsrc, Buffer bmask, int dx, int grey, int op)
{
	Buffer obdst;
	int fs, sadelta;
	int i, ma, da, q;
	u32int t, t1;

	obdst = bdst;
	sadelta = bsrc.alpha == &ones ? 0 : bsrc.delta;
	q = bsrc.delta == 4 && bdst.delta == 4 && chanmatch(&bdst, &bsrc);

	for(i=0; i<dx; i++){
		ma = *bmask.alpha;
		da = *bdst.alpha;
		if(op == SoutD)
			da = 255-da;
		fs = ma;
		if(op != S)
			fs = CALC11(fs, da, t);

		if(grey){
			*bdst.grey = CALC11(fs, *bsrc.grey, t);
			bsrc.grey += bsrc.delta;
			bdst.grey += bdst.delta;
		}else{
			if(q){
				*bdst.rgba = CALC41(fs, *bsrc.rgba, t, t1);
				bsrc.rgba++;
				bdst.rgba++;
				bmask.alpha += bmask.delta;
				bdst.alpha += bdst.delta;
				continue;
			}
			*bdst.red = CALC11(fs, *bsrc.red, t);
			*bdst.grn = CALC11(fs, *bsrc.grn, t);
			*bdst.blu = CALC11(fs, *bsrc.blu, t);
			bsrc.red += bsrc.delta;
			bsrc.blu += bsrc.delta;
			bsrc.grn += bsrc.delta;
			bdst.red += bdst.delta;
			bdst.blu += bdst.delta;
			bdst.grn += bdst.delta;
		}
		if(bdst.alpha != &ones){
			*bdst.alpha = CALC11(fs, *bsrc.alpha, t);
			bdst.alpha += bdst.delta;
		}
		bmask.alpha += bmask.delta;
		bsrc.alpha += sadelta;
	}
	return obdst;
}

static Buffer
alphacalc3679(Buffer bdst, Buffer bsrc, Buffer bmask, int dx, int grey, int op)
{
	Buffer obdst;
	int fs, fd, sadelta;
	int i, sa, ma, da, q;
	u32int t, t1;

	obdst = bdst;
	sadelta = bsrc.alpha == &ones ? 0 : bsrc.delta;
	q = bsrc.delta == 4 && bdst.delta == 4 && chanmatch(&bdst, &bsrc);

	for(i=0; i<dx; i++){
		sa = *bsrc.alpha;
		ma = *bmask.alpha;
		da = *bdst.alpha;
		if(op == SatopD)
			fs = CALC11(ma, da, t);
		else
			fs = CALC11(ma, 255-da, t);
		if(op == DoverS)
			fd = 255;
		else{
			fd = CALC11(sa, ma, t);
			if(op != DatopS)
				fd = 255-fd;
		}

		if(grey){
			*bdst.grey = CALC12(fs, *bsrc.grey, fd, *bdst.grey, t);
			bsrc.grey += bsrc.delta;
			bdst.grey += bdst.delta;
		}else{
			if(q){
				*bdst.rgba = CALC42(fs, *bsrc.rgba, fd, *bdst.rgba, t, t1);
				bsrc.rgba++;
				bdst.rgba++;
				bsrc.alpha += sadelta;
				bmask.alpha += bmask.delta;
				bdst.alpha += bdst.delta;
				continue;
			}
			*bdst.red = CALC12(fs, *bsrc.red, fd, *bdst.red, t);
			*bdst.grn = CALC12(fs, *bsrc.grn, fd, *bdst.grn, t);
			*bdst.blu = CALC12(fs, *bsrc.blu, fd, *bdst.blu, t);
			bsrc.red += bsrc.delta;
			bsrc.blu += bsrc.delta;
			bsrc.grn += bsrc.delta;
			bdst.red += bdst.delta;
			bdst.blu += bdst.delta;
			bdst.grn += bdst.delta;
		}
		if(bdst.alpha != &ones){
			*bdst.alpha = CALC12(fs, sa, fd, da, t);
			bdst.alpha += bdst.delta;
		}
		bmask.alpha += bmask.delta;
		bsrc.alpha += sadelta;
	}
	return obdst;
}

static Buffer
alphacalc5(Buffer bdst, Buffer b1, Buffer b2, int dx, int grey, int op)
{
	USED(dx);
	USED(grey);
	USED(op);
	return bdst;
}

static Buffer
alphacalc11(Buffer bdst, Buffer bsrc, Buffer bmask, int dx, int grey, int op)
{
	Buffer obdst;
	int fd, sadelta;
	int i, sa, ma, q;
	u32int t, t1;

	USED(op);
	obdst = bdst;
	sadelta = bsrc.alpha == &ones ? 0 : bsrc.delta;
	q = bsrc.delta == 4 && bdst.delta == 4 && chanmatch(&bdst, &bsrc);

	for(i=0; i<dx; i++){
		sa = *bsrc.alpha;
		ma = *bmask.alpha;
		fd = 255-CALC11(sa, ma, t);

		if(grey){
			*bdst.grey = CALC12(ma, *bsrc.grey, fd, *bdst.grey, t);
			bsrc.grey += bsrc.delta;
			bdst.grey += bdst.delta;
		}else{
			if(q){
				*bdst.rgba = CALC42(ma, *bsrc.rgba, fd, *bdst.rgba, t, t1);
				bsrc.rgba++;
				bdst.rgba++;
				bsrc.alpha += sadelta;
				bmask.alpha += bmask.delta;
				continue;
			}
			*bdst.red = CALC12(ma, *bsrc.red, fd, *bdst.red, t);
			*bdst.grn = CALC12(ma, *bsrc.grn, fd, *bdst.grn, t);
			*bdst.blu = CALC12(ma, *bsrc.blu, fd, *bdst.blu, t);
			bsrc.red += bsrc.delta;
			bsrc.blu += bsrc.delta;
			bsrc.grn += bsrc.delta;
			bdst.red += bdst.delta;
			bdst.blu += bdst.delta;
			bdst.grn += bdst.delta;
		}
		if(bdst.alpha != &ones){
			*bdst.alpha = CALC12(ma, sa, fd, *bdst.alpha, t);
			bdst.alpha += bdst.delta;
		}
		bmask.alpha += bmask.delta;
		bsrc.alpha += sadelta;
	}
	return obdst;
}

/*
not used yet
source and mask alpha 1
static Buffer
alphacalcS0(Buffer bdst, Buffer bsrc, Buffer bmask, int dx, int grey, int op)
{
	Buffer obdst;
	int i;

	USED(op);
	obdst = bdst;
	if(bsrc.delta == bdst.delta){
		memmove(bdst.rgba, bsrc.rgba, dx*bdst.delta);
		return obdst;
	}
	for(i=0; i<dx; i++){
		if(grey){
			*bdst.grey = *bsrc.grey;
			bsrc.grey += bsrc.delta;
			bdst.grey += bdst.delta;
		}else{
			*bdst.red = *bsrc.red;
			*bdst.grn = *bsrc.grn;
			*bdst.blu = *bsrc.blu;
			bsrc.red += bsrc.delta;
			bsrc.blu += bsrc.delta;
			bsrc.grn += bsrc.delta;
			bdst.red += bdst.delta;
			bdst.blu += bdst.delta;
			bdst.grn += bdst.delta;
		}
		if(bdst.alpha != &ones){
			*bdst.alpha = 255;
			bdst.alpha += bdst.delta;
		}
	}
	return obdst;
}
*/

/* source alpha 1 */
static Buffer
alphacalcS(Buffer bdst, Buffer bsrc, Buffer bmask, int dx, int grey, int op)
{
	Buffer obdst;
	int fd;
	int i, ma;
	u32int t;

	USED(op);
	obdst = bdst;

	for(i=0; i<dx; i++){
		ma = *bmask.alpha;
		fd = 255-ma;

		if(grey){
			*bdst.grey = CALC12(ma, *bsrc.grey, fd, *bdst.grey, t);
			bsrc.grey += bsrc.delta;
			bdst.grey += bdst.delta;
		}else{
			*bdst.red = CALC12(ma, *bsrc.red, fd, *bdst.red, t);
			*bdst.grn = CALC12(ma, *bsrc.grn, fd, *bdst.grn, t);
			*bdst.blu = CALC12(ma, *bsrc.blu, fd, *bdst.blu, t);
			bsrc.red += bsrc.delta;
			bsrc.blu += bsrc.delta;
			bsrc.grn += bsrc.delta;
			bdst.red += bdst.delta;
			bdst.blu += bdst.delta;
			bdst.grn += bdst.delta;
		}
		if(bdst.alpha != &ones){
			*bdst.alpha = ma+CALC11(fd, *bdst.alpha, t);
			bdst.alpha += bdst.delta;
		}
		bmask.alpha += bmask.delta;
	}
	return obdst;
}

static Buffer
boolcalc14(Buffer bdst, Buffer b1, Buffer bmask, int dx, int grey, int op)
{
	Buffer obdst;
	int i, ma, zero;

	obdst = bdst;

	for(i=0; i<dx; i++){
		ma = *bmask.alpha;
		zero = ma ? op == DoutS : op == DinS;

		if(grey){
			if(zero)
				*bdst.grey = 0;
			bdst.grey += bdst.delta;
		}else{
			if(zero)
				*bdst.red = *bdst.grn = *bdst.blu = 0;
			bdst.red += bdst.delta;
			bdst.blu += bdst.delta;
			bdst.grn += bdst.delta;
		}
		bmask.alpha += bmask.delta;
		if(bdst.alpha != &ones){
			if(zero)
				*bdst.alpha = 0;
			bdst.alpha += bdst.delta;
		}
	}
	return obdst;
}

static Buffer
boolcalc236789(Buffer bdst, Buffer bsrc, Buffer bmask, int dx, int grey, int op)
{
	Buffer obdst;
	int fs, fd;
	int i, ma, da, zero;
	u32int t;

	obdst = bdst;
	zero = !(op&1);

	for(i=0; i<dx; i++){
		ma = *bmask.alpha;
		da = *bdst.alpha;
		fs = da;
		if(op&2)
			fs = 255-da;
		fd = 0;
		if(op&4)
			fd = 255;

		if(grey){
			if(ma)
				*bdst.grey = CALC12(fs, *bsrc.grey, fd, *bdst.grey, t);
			else if(zero)
				*bdst.grey = 0;
			bsrc.grey += bsrc.delta;
			bdst.grey += bdst.delta;
		}else{
			if(ma){
				*bdst.red = CALC12(fs, *bsrc.red, fd, *bdst.red, t);
				*bdst.grn = CALC12(fs, *bsrc.grn, fd, *bdst.grn, t);
				*bdst.blu = CALC12(fs, *bsrc.blu, fd, *bdst.blu, t);
			}
			else if(zero)
				*bdst.red = *bdst.grn = *bdst.blu = 0;
			bsrc.red += bsrc.delta;
			bsrc.blu += bsrc.delta;
			bsrc.grn += bsrc.delta;
			bdst.red += bdst.delta;
			bdst.blu += bdst.delta;
			bdst.grn += bdst.delta;
		}
		bmask.alpha += bmask.delta;
		if(bdst.alpha != &ones){
			if(ma)
				*bdst.alpha = fs+CALC11(fd, da, t);
			else if(zero)
				*bdst.alpha = 0;
			bdst.alpha += bdst.delta;
		}
	}
	return obdst;
}

static Buffer
boolcalc1011(Buffer bdst, Buffer bsrc, Buffer bmask, int dx, int grey, int op)
{
	Buffer obdst;
	int i, ma, zero;

	obdst = bdst;
	zero = !(op&1);

	for(i=0; i<dx; i++){
		ma = *bmask.alpha;

		if(grey){
			if(ma)
				*bdst.grey = *bsrc.grey;
			else if(zero)
				*bdst.grey = 0;
			bsrc.grey += bsrc.delta;
			bdst.grey += bdst.delta;
		}else{
			if(ma){
				*bdst.red = *bsrc.red;
				*bdst.grn = *bsrc.grn;
				*bdst.blu = *bsrc.blu;
			}
			else if(zero)
				*bdst.red = *bdst.grn = *bdst.blu = 0;
			bsrc.red += bsrc.delta;
			bsrc.blu += bsrc.delta;
			bsrc.grn += bsrc.delta;
			bdst.red += bdst.delta;
			bdst.blu += bdst.delta;
			bdst.grn += bdst.delta;
		}
		bmask.alpha += bmask.delta;
		if(bdst.alpha != &ones){
			if(ma)
				*bdst.alpha = 255;
			else if(zero)
				*bdst.alpha = 0;
			bdst.alpha += bdst.delta;
		}
	}
	return obdst;
}
/*
 * Replicated cached scan line read.  Call the function listed in the Param,
 * but cache the result so that for replicated images we only do the work once.
 */
static Buffer
replread(Param *p, uchar *s, int y)
{
	Buffer *b;

	USED(s);
	b = &p->bcache[y];
	if((p->bfilled & (1<<y)) == 0){
		p->bfilled |= 1<<y;
		*b = p->replcall(p, p->bufbase+y*p->bufdelta, y);
	}
	return *b;
}

/*
 * Alpha reading function that simply relabels the grey pointer.
 */
static Buffer
greymaskread(Param *p, uchar *buf, int y)
{
	Buffer b;

	b = p->greymaskcall(p, buf, y);
	b.alpha = b.grey;
	return b;
}

#define DBG if(0)
static Buffer
readnbit(Param *p, uchar *buf, int y)
{
	Buffer b;
	Memimage *img;
	uchar *repl, *r, *w, *ow, bits;
	int i, n, sh, depth, x, dx, npack, nbits;

	memset(&b, 0, sizeof b);
	b.rgba = (u32int*)buf;
	b.grey = w = buf;
	b.red = b.blu = b.grn = w;
	b.alpha = &ones;
	b.delta = 1;

	dx = p->dx;
	img = p->img;
	depth = img->depth;
	repl = &replbit[depth][0];
	npack = 8/depth;
	sh = 8-depth;

	/* copy from p->r.min.x until end of repl rectangle */
	x = p->r.min.x;
	n = dx;
	if(n > p->img->r.max.x - x)
		n = p->img->r.max.x - x;

	r = p->bytermin + y*p->bwidth;
DBG print("readnbit dx %d %p=%p+%d*%d, *r=%d fetch %d ", dx, r, p->bytermin, y, p->bwidth, *r, n);
	bits = *r++;
	nbits = 8;
	if(i=x&(npack-1)){
DBG print("throwaway %d...", i);
		bits <<= depth*i;
		nbits -= depth*i;
	}
	for(i=0; i<n; i++){
		if(nbits == 0){
DBG print("(%.2ux)...", *r);
			bits = *r++;
			nbits = 8;
		}
		*w++ = repl[bits>>sh];
DBG print("bit %x...", repl[bits>>sh]);
		bits <<= depth;
		nbits -= depth;
	}
	dx -= n;
	if(dx == 0)
		return b;

	assert(x+i == p->img->r.max.x);

	/* copy from beginning of repl rectangle until where we were before. */
	x = p->img->r.min.x;
	n = dx;
	if(n > p->r.min.x - x)
		n = p->r.min.x - x;

	r = p->bytey0s + y*p->bwidth;
DBG print("x=%d r=%p...", x, r);
	bits = *r++;
	nbits = 8;
	if(i=x&(npack-1)){
		bits <<= depth*i;
		nbits -= depth*i;
	}
DBG print("nbits=%d...", nbits);
	for(i=0; i<n; i++){
		if(nbits == 0){
			bits = *r++;
			nbits = 8;
		}
		*w++ = repl[bits>>sh];
DBG print("bit %x...", repl[bits>>sh]);
		bits <<= depth;
		nbits -= depth;
DBG print("bits %x nbits %d...", bits, nbits);
	}
	dx -= n;
	if(dx == 0)
		return b;

	assert(dx > 0);
	/* now we have exactly one full scan line: just replicate the buffer itself until we are done */
	ow = buf;
	while(dx--)
		*w++ = *ow++;

	return b;
}
#undef DBG

#define DBG if(0)
static void
writenbit(Param *p, uchar *w, Buffer src)
{
	uchar *r;
	u32int bits;
	int i, sh, depth, npack, nbits, x, ex;

	assert(src.grey != nil && src.delta == 1);

	x = p->r.min.x;
	ex = x+p->dx;
	depth = p->img->depth;
	npack = 8/depth;

	i=x&(npack-1);
	bits = i ? (*w >> (8-depth*i)) : 0;
	nbits = depth*i;
	sh = 8-depth;
	r = src.grey;

	for(; x<ex; x++){
		bits <<= depth;
DBG print(" %x", *r);
		bits |= (*r++ >> sh);
		nbits += depth;
		if(nbits == 8){
			*w++ = bits;
			nbits = 0;
		}
	}

	if(nbits){
		sh = 8-nbits;
		bits <<= sh;
		bits |= *w & ((1<<sh)-1);
		*w = bits;
	}
DBG print("\n");
	return;
}
#undef DBG

static Buffer
readcmap(Param *p, uchar *buf, int y)
{
	Buffer b;
	int a, convgrey, copyalpha, dx, i, m;
	uchar *q, *cmap, *begin, *end, *r, *w;

	memset(&b, 0, sizeof b);
	begin = p->bytey0s + y*p->bwidth;
	r = p->bytermin + y*p->bwidth;
	end = p->bytey0e + y*p->bwidth;
	cmap = p->img->cmap->cmap2rgb;
	convgrey = p->convgrey;
	copyalpha = (p->img->flags&Falpha) ? 1 : 0;

	w = buf;
	dx = p->dx;
	if(copyalpha){
		b.alpha = buf++;
		a = p->img->shift[CAlpha]/8;
		m = p->img->shift[CMap]/8;
		for(i=0; i<dx; i++){
			*w++ = r[a];
			q = cmap+r[m]*3;
			r += 2;
			if(r == end)
				r = begin;
			if(convgrey){
				*w++ = RGB2K(q[0], q[1], q[2]);
			}else{
				*w++ = q[2];	/* blue */
				*w++ = q[1];	/* green */
				*w++ = q[0];	/* red */
			}
		}
	}else{
		b.alpha = &ones;
		for(i=0; i<dx; i++){
			q = cmap+*r++*3;
			if(r == end)
				r = begin;
			if(convgrey){
				*w++ = RGB2K(q[0], q[1], q[2]);
			}else{
				*w++ = q[2];	/* blue */
				*w++ = q[1];	/* green */
				*w++ = q[0];	/* red */
			}
		}
	}

	b.rgba = (u32int*)(buf-copyalpha);

	if(convgrey){
		b.grey = buf;
		b.red = b.blu = b.grn = buf;
		b.delta = 1+copyalpha;
	}else{
		b.blu = buf;
		b.grn = buf+1;
		b.red = buf+2;
		b.grey = nil;
		b.delta = 3+copyalpha;
	}
	return b;
}

static void
writecmap(Param *p, uchar *w, Buffer src)
{
	uchar *cmap, *red, *grn, *blu;
	int i, dx, delta;

	cmap = p->img->cmap->rgb2cmap;

	delta = src.delta;
	red= src.red;
	grn = src.grn;
	blu = src.blu;

	dx = p->dx;
	for(i=0; i<dx; i++, red+=delta, grn+=delta, blu+=delta)
		*w++ = cmap[(*red>>4)*256+(*grn>>4)*16+(*blu>>4)];
}

#define DBG if(drawdebug)
static Buffer
readbyte(Param *p, uchar *buf, int y)
{
	Buffer b;
	Memimage *img;
	int dx, isgrey, convgrey, alphaonly, copyalpha, i, nb;
	uchar *begin, *end, *r, *w, *rrepl, *grepl, *brepl, *arepl, *krepl;
	uchar ured, ugrn, ublu;
	u32int u;

	img = p->img;
	begin = p->bytey0s + y*p->bwidth;
	r = p->bytermin + y*p->bwidth;
	end = p->bytey0e + y*p->bwidth;

	w = buf;
	dx = p->dx;
	nb = img->depth/8;

	convgrey = p->convgrey;	/* convert rgb to grey */
	isgrey = img->flags&Fgrey;
	alphaonly = p->alphaonly;
	copyalpha = (img->flags&Falpha) ? 1 : 0;

	/* if we can, avoid processing everything */
	if(!(img->flags&Frepl) && !convgrey && (img->flags&Fbytes)){
		memset(&b, 0, sizeof b);
		if(p->needbuf){
			memmove(buf, r, dx*nb);
			r = buf;
		}
		b.rgba = (u32int*)r;
		if(copyalpha)
			b.alpha = r+img->shift[CAlpha]/8;
		else
			b.alpha = &ones;
		if(isgrey){
			b.grey = r+img->shift[CGrey]/8;
			b.red = b.grn = b.blu = b.grey;
		}else{
			b.red = r+img->shift[CRed]/8;
			b.grn = r+img->shift[CGreen]/8;
			b.blu = r+img->shift[CBlue]/8;
		}
		b.delta = nb;
		return b;
	}

	rrepl = replbit[img->nbits[CRed]];
	grepl = replbit[img->nbits[CGreen]];
	brepl = replbit[img->nbits[CBlue]];
	arepl = replbit[img->nbits[CAlpha]];
	krepl = replbit[img->nbits[CGrey]];

	for(i=0; i<dx; i++){
		u = r[0] | (r[1]<<8) | (r[2]<<16) | (r[3]<<24);
		if(copyalpha)
			*w++ = arepl[(u>>img->shift[CAlpha]) & img->mask[CAlpha]];

		if(isgrey)
			*w++ = krepl[(u >> img->shift[CGrey]) & img->mask[CGrey]];
		else if(!alphaonly){
			ured = rrepl[(u >> img->shift[CRed]) & img->mask[CRed]];
			ugrn = grepl[(u >> img->shift[CGreen]) & img->mask[CGreen]];
			ublu = brepl[(u >> img->shift[CBlue]) & img->mask[CBlue]];
			if(convgrey){
				*w++ = RGB2K(ured, ugrn, ublu);
			}else{
				*w++ = brepl[(u >> img->shift[CBlue]) & img->mask[CBlue]];
				*w++ = grepl[(u >> img->shift[CGreen]) & img->mask[CGreen]];
				*w++ = rrepl[(u >> img->shift[CRed]) & img->mask[CRed]];
			}
		}
		r += nb;
		if(r == end)
			r = begin;
	}

	b.alpha = copyalpha ? buf : &ones;
	b.rgba = (u32int*)buf;
	if(alphaonly){
		b.red = b.grn = b.blu = b.grey = nil;
		if(!copyalpha)
			b.rgba = nil;
		b.delta = 1;
	}else if(isgrey || convgrey){
		b.grey = buf+copyalpha;
		b.red = b.grn = b.blu = buf+copyalpha;
		b.delta = copyalpha+1;
	}else{
		b.blu = buf+copyalpha;
		b.grn = buf+copyalpha+1;
		b.grey = nil;
		b.red = buf+copyalpha+2;
		b.delta = copyalpha+3;
	}
	return b;
}
#undef DBG

#define DBG if(drawdebug)
static void
writebyte(Param *p, uchar *w, Buffer src)
{
	Memimage *img;
	int i, isalpha, isgrey, nb, delta, dx, adelta;
	uchar ff, *red, *grn, *blu, *grey, *alpha;
	u32int u, mask;

	img = p->img;

	red = src.red;
	grn = src.grn;
	blu = src.blu;
	alpha = src.alpha;
	delta = src.delta;
	grey = src.grey;
	dx = p->dx;

	nb = img->depth/8;
	mask = (nb==4) ? 0 : ~((1<<img->depth)-1);

	isalpha = img->flags&Falpha;
	isgrey = img->flags&Fgrey;
	adelta = src.delta;

	if(isalpha && (alpha == nil || alpha == &ones)){
		ff = 0xFF;
		alpha = &ff;
		adelta = 0;
	}

	for(i=0; i<dx; i++){
		u = w[0] | (w[1]<<8) | (w[2]<<16) | (w[3]<<24);
DBG print("u %.8lux...", u);
		u &= mask;
DBG print("&mask %.8lux...", u);
		if(isgrey){
			u |= ((*grey >> (8-img->nbits[CGrey])) & img->mask[CGrey]) << img->shift[CGrey];
DBG print("|grey %.8lux...", u);
			grey += delta;
		}else{
			u |= ((*red >> (8-img->nbits[CRed])) & img->mask[CRed]) << img->shift[CRed];
			u |= ((*grn >> (8-img->nbits[CGreen])) & img->mask[CGreen]) << img->shift[CGreen];
			u |= ((*blu >> (8-img->nbits[CBlue])) & img->mask[CBlue]) << img->shift[CBlue];
			red += delta;
			grn += delta;
			blu += delta;
DBG print("|rgb %.8lux...", u);
		}

		if(isalpha){
			u |= ((*alpha >> (8-img->nbits[CAlpha])) & img->mask[CAlpha]) << img->shift[CAlpha];
			alpha += adelta;
DBG print("|alpha %.8lux...", u);
		}

		w[0] = u;
		w[1] = u>>8;
		w[2] = u>>16;
		w[3] = u>>24;
DBG print("write back %.8lux...", u);
		w += nb;
	}
}
#undef DBG

static Readfn*
readfn(Memimage *img)
{
	if(img->depth < 8)
		return readnbit;
	if(img->nbits[CMap] == 8)
		return readcmap;
	return readbyte;
}

static Readfn*
readalphafn(Memimage *m)
{
	USED(m);
	return readbyte;
}

static Writefn*
writefn(Memimage *img)
{
	if(img->depth < 8)
		return writenbit;
	if(img->chan == CMAP8)
		return writecmap;
	return writebyte;
}

static void
nullwrite(Param *p, uchar *s, Buffer b)
{
	USED(p);
	USED(s);
}

static Buffer
readptr(Param *p, uchar *s, int y)
{
	Buffer b;
	uchar *q;

	USED(s);
	memset(&b, 0, sizeof b);
	q = p->bytermin + y*p->bwidth;
	b.red = q;	/* ptr to data */
	b.grn = b.blu = b.grey = b.alpha = nil;
	b.rgba = (u32int*)q;
	b.delta = p->img->depth/8;
	return b;
}

static Buffer
boolmemmove(Buffer bdst, Buffer bsrc, Buffer b1, int dx, int i, int o)
{
	USED(i);
	USED(o);
	memmove(bdst.red, bsrc.red, dx*bdst.delta);
	return bdst;
}

static Buffer
boolcopy8(Buffer bdst, Buffer bsrc, Buffer bmask, int dx, int i, int o)
{
	uchar *m, *r, *w, *ew;

	USED(i);
	USED(o);
	m = bmask.grey;
	w = bdst.red;
	r = bsrc.red;
	ew = w+dx;
	for(; w < ew; w++,r++)
		if(*m++)
			*w = *r;
	return bdst;	/* not used */
}

static Buffer
boolcopy16(Buffer bdst, Buffer bsrc, Buffer bmask, int dx, int i, int o)
{
	uchar *m;
	ushort *r, *w, *ew;

	USED(i);
	USED(o);
	m = bmask.grey;
	w = (ushort*)bdst.red;
	r = (ushort*)bsrc.red;
	ew = w+dx;
	for(; w < ew; w++,r++)
		if(*m++)
			*w = *r;
	return bdst;	/* not used */
}

static Buffer
boolcopy24(Buffer bdst, Buffer bsrc, Buffer bmask, int dx, int i, int o)
{
	uchar *m;
	uchar *r, *w, *ew;

	USED(i);
	USED(o);
	m = bmask.grey;
	w = bdst.red;
	r = bsrc.red;
	ew = w+dx*3;
	while(w < ew){
		if(*m++){
			*w++ = *r++;
			*w++ = *r++;
			*w++ = *r++;
		}else{
			w += 3;
			r += 3;
		}
	}
	return bdst;	/* not used */
}

static Buffer
boolcopy32(Buffer bdst, Buffer bsrc, Buffer bmask, int dx, int i, int o)
{
	uchar *m;
	u32int *r, *w, *ew;

	USED(i);
	USED(o);
	m = bmask.grey;
	w = (u32int*)bdst.red;
	r = (u32int*)bsrc.red;
	ew = w+dx;
	for(; w < ew; w++,r++)
		if(*m++)
			*w = *r;
	return bdst;	/* not used */
}

static Buffer
genconv(Param *p, uchar *buf, int y)
{
	Buffer b;
	int nb;
	uchar *r, *w, *ew;

	/* read from source into RGB format in convbuf */
	b = p->convreadcall(p, p->convbuf, y);

	/* write RGB format into dst format in buf */
	p->convwritecall(p->convdpar, buf, b);

	if(p->convdx){
		nb = p->convdpar->img->depth/8;
		r = buf;
		w = buf+nb*p->dx;
		ew = buf+nb*p->convdx;
		while(w<ew)
			*w++ = *r++;
	}

	b.red = buf;
	b.blu = b.grn = b.grey = b.alpha = nil;
	b.rgba = (u32int*)buf;
	b.delta = 0;

	return b;
}

static Readfn*
convfn(Memimage *dst, Param *dpar, Memimage *src, Param *spar)
{
	if(dst->chan == src->chan && !(src->flags&Frepl)){
/*if(drawdebug) iprint("readptr..."); */
		return readptr;
	}

	if(dst->chan==CMAP8 && (src->chan==GREY1||src->chan==GREY2||src->chan==GREY4)){
		/* cheat because we know the replicated value is exactly the color map entry. */
/*if(drawdebug) iprint("Readnbit..."); */
		return readnbit;
	}

	spar->convreadcall = readfn(src);
	spar->convwritecall = writefn(dst);
	spar->convdpar = dpar;

	/* allocate a conversion buffer */
	spar->convbufoff = ndrawbuf;
	ndrawbuf += spar->dx*4;

	if(spar->dx > Dx(spar->img->r)){
		spar->convdx = spar->dx;
		spar->dx = Dx(spar->img->r);
	}

/*if(drawdebug) iprint("genconv..."); */
	return genconv;
}

/*
 * Do NOT call this directly.  pixelbits is a wrapper
 * around this that fetches the bits from the X server
 * when necessary.
 */
u32int
_pixelbits(Memimage *i, Point pt)
{
	uchar *p;
	u32int val;
	int off, bpp, npack;

	val = 0;
	p = byteaddr(i, pt);
	switch(bpp=i->depth){
	case 1:
	case 2:
	case 4:
		npack = 8/bpp;
		off = pt.x%npack;
		val = p[0] >> bpp*(npack-1-off);
		val &= (1<<bpp)-1;
		break;
	case 8:
		val = p[0];
		break;
	case 16:
		val = p[0]|(p[1]<<8);
		break;
	case 24:
		val = p[0]|(p[1]<<8)|(p[2]<<16);
		break;
	case 32:
		val = p[0]|(p[1]<<8)|(p[2]<<16)|(p[3]<<24);
		break;
	}
	while(bpp<32){
		val |= val<<bpp;
		bpp *= 2;
	}
	return val;
}

static Calcfn*
boolcopyfn(Memimage *img, Memimage *mask)
{
	if(mask->flags&Frepl && Dx(mask->r)==1 && Dy(mask->r)==1 && pixelbits(mask, mask->r.min)==~0)
		return boolmemmove;

	switch(img->depth){
	case 8:
		return boolcopy8;
	case 16:
		return boolcopy16;
	case 24:
		return boolcopy24;
	case 32:
		return boolcopy32;
	default:
		assert(0 /* boolcopyfn */);
	}
	return 0;
}

/*
 * Optimized draw for filling and scrolling; uses memset and memmove.
 */
static void
memsets(void *vp, ushort val, int n)
{
	ushort *p, *ep;

	p = vp;
	ep = p+n;
	while(p<ep)
		*p++ = val;
}

static void
memsetl(void *vp, u32int val, int n)
{
	u32int *p, *ep;

	p = vp;
	ep = p+n;
	while(p<ep)
		*p++ = val;
}

static void
memset24(void *vp, u32int val, int n)
{
	uchar *p, *ep;
	uchar a,b,c;

	p = vp;
	ep = p+3*n;
	a = val;
	b = val>>8;
	c = val>>16;
	while(p<ep){
		*p++ = a;
		*p++ = b;
		*p++ = c;
	}
}

u32int
_imgtorgba(Memimage *img, u32int val)
{
	uchar r, g, b, a;
	int nb, ov, v;
	u32int chan;
	uchar *p;

	a = 0xFF;
	r = g = b = 0xAA;	/* garbage */
	for(chan=img->chan; chan; chan>>=8){
		nb = NBITS(chan);
		ov = v = val&((1<<nb)-1);
		val >>= nb;

		while(nb < 8){
			v |= v<<nb;
			nb *= 2;
		}
		v >>= (nb-8);

		switch(TYPE(chan)){
		case CRed:
			r = v;
			break;
		case CGreen:
			g = v;
			break;
		case CBlue:
			b = v;
			break;
		case CAlpha:
			a = v;
			break;
		case CGrey:
			r = g = b = v;
			break;
		case CMap:
			p = img->cmap->cmap2rgb+3*ov;
			r = *p++;
			g = *p++;
			b = *p;
			break;
		}
	}
	return (r<<24)|(g<<16)|(b<<8)|a;
}

u32int
_rgbatoimg(Memimage *img, u32int rgba)
{
	u32int chan;
	int d, nb;
	u32int v;
	uchar *p, r, g, b, a, m;

	v = 0;
	r = rgba>>24;
	g = rgba>>16;
	b = rgba>>8;
	a = rgba;
	d = 0;
	for(chan=img->chan; chan; chan>>=8){
		nb = NBITS(chan);
		switch(TYPE(chan)){
		case CRed:
			v |= (r>>(8-nb))<<d;
			break;
		case CGreen:
			v |= (g>>(8-nb))<<d;
			break;
		case CBlue:
			v |= (b>>(8-nb))<<d;
			break;
		case CAlpha:
			v |= (a>>(8-nb))<<d;
			break;
		case CMap:
			p = img->cmap->rgb2cmap;
			m = p[(r>>4)*256+(g>>4)*16+(b>>4)];
			v |= (m>>(8-nb))<<d;
			break;
		case CGrey:
			m = RGB2K(r,g,b);
			v |= (m>>(8-nb))<<d;
			break;
		}
		d += nb;
	}
/*	print("rgba2img %.8lux = %.*lux\n", rgba, 2*d/8, v); */
	return v;
}

#define DBG if(0)
static int
memoptdraw(Memdrawparam *par)
{
	int m, y, dy, dx, op;
	u32int v;
	u16int u16;
	Memimage *src;
	Memimage *dst;

	dx = Dx(par->r);
	dy = Dy(par->r);
	src = par->src;
	dst = par->dst;
	op = par->op;

DBG print("state %lux mval %lux dd %d\n", par->state, par->mval, dst->depth);
	/*
	 * If we have an opaque mask and source is one opaque pixel we can convert to the
	 * destination format and just replicate with memset.
	 */
	m = Simplesrc|Simplemask|Fullmask;
	if((par->state&m)==m && (par->srgba&0xFF) == 0xFF && (op ==S || op == SoverD)){
		uchar *dp, p[4];
		int d, dwid, ppb, np, nb;
		uchar lm, rm;

DBG print("memopt, dst %p, dst->data->bdata %p\n", dst, dst->data->bdata);
		dwid = dst->width*sizeof(u32int);
		dp = byteaddr(dst, par->r.min);
		v = par->sdval;
DBG print("sdval %lud, depth %d\n", v, dst->depth);
		switch(dst->depth){
		case 1:
		case 2:
		case 4:
			for(d=dst->depth; d<8; d*=2)
				v |= (v<<d);
			ppb = 8/dst->depth;	/* pixels per byte */
			m = ppb-1;
			/* left edge */
			np = par->r.min.x&m;		/* no. pixels unused on left side of word */
			dx -= (ppb-np);
			nb = 8 - np * dst->depth;		/* no. bits used on right side of word */
			lm = (1<<nb)-1;
DBG print("np %d x %d nb %d lm %ux ppb %d m %ux\n", np, par->r.min.x, nb, lm, ppb, m);

			/* right edge */
			np = par->r.max.x&m;	/* no. pixels used on left side of word */
			dx -= np;
			nb = 8 - np * dst->depth;		/* no. bits unused on right side of word */
			rm = ~((1<<nb)-1);
DBG print("np %d x %d nb %d rm %ux ppb %d m %ux\n", np, par->r.max.x, nb, rm, ppb, m);

DBG print("dx %d Dx %d\n", dx, Dx(par->r));
			/* lm, rm are masks that are 1 where we should touch the bits */
			if(dx < 0){	/* just one byte */
				lm &= rm;
				for(y=0; y<dy; y++, dp+=dwid)
					*dp ^= (v ^ *dp) & lm;
			}else if(dx == 0){	/* no full bytes */
				if(lm)
					dwid--;

				for(y=0; y<dy; y++, dp+=dwid){
					if(lm){
DBG print("dp %p v %lux lm %ux (v ^ *dp) & lm %lux\n", dp, v, lm, (v^*dp)&lm);
						*dp ^= (v ^ *dp) & lm;
						dp++;
					}
					*dp ^= (v ^ *dp) & rm;
				}
			}else{		/* full bytes in middle */
				dx /= ppb;
				if(lm)
					dwid--;
				dwid -= dx;

				for(y=0; y<dy; y++, dp+=dwid){
					if(lm){
						*dp ^= (v ^ *dp) & lm;
						dp++;
					}
					memset(dp, v, dx);
					dp += dx;
					*dp ^= (v ^ *dp) & rm;
				}
			}
			return 1;
		case 8:
			for(y=0; y<dy; y++, dp+=dwid)
				memset(dp, v, dx);
			return 1;
		case 16:
			p[0] = v;		/* make little endian */
			p[1] = v>>8;
			memmove(&u16, p, 2);
			v = u16;
DBG print("dp=%p; dx=%d; for(y=0; y<%d; y++, dp+=%d)\nmemsets(dp, v, dx);\n",
	dp, dx, dy, dwid);
			for(y=0; y<dy; y++, dp+=dwid)
				memsets(dp, v, dx);
			return 1;
		case 24:
			for(y=0; y<dy; y++, dp+=dwid)
				memset24(dp, v, dx);
			return 1;
		case 32:
			p[0] = v;		/* make little endian */
			p[1] = v>>8;
			p[2] = v>>16;
			p[3] = v>>24;
			memmove(&v, p, 4);
			for(y=0; y<dy; y++, dp+=dwid)
				memsetl(dp, v, dx);
			return 1;
		default:
			assert(0 /* bad dest depth in memoptdraw */);
		}
	}

	/*
	 * If no source alpha, an opaque mask, we can just copy the
	 * source onto the destination.  If the channels are the same and
	 * the source is not replicated, memmove suffices.
	 */
	m = Simplemask|Fullmask;
	if((par->state&(m|Replsrc))==m && src->depth >= 8
	&& src->chan == dst->chan && !(src->flags&Falpha) && (op == S || op == SoverD)){
		uchar *sp, *dp;
		long swid, dwid, nb;
		int dir;

		if(src->data == dst->data && byteaddr(dst, par->r.min) > byteaddr(src, par->sr.min))
			dir = -1;
		else
			dir = 1;

		swid = src->width*sizeof(u32int);
		dwid = dst->width*sizeof(u32int);
		sp = byteaddr(src, par->sr.min);
		dp = byteaddr(dst, par->r.min);
		if(dir == -1){
			sp += (dy-1)*swid;
			dp += (dy-1)*dwid;
			swid = -swid;
			dwid = -dwid;
		}
		nb = (dx*src->depth)/8;
		for(y=0; y<dy; y++, sp+=swid, dp+=dwid)
			memmove(dp, sp, nb);
		return 1;
	}

	/*
	 * If we have a 1-bit mask, 1-bit source, and 1-bit destination, and
	 * they're all bit aligned, we can just use bit operators.  This happens
	 * when we're manipulating boolean masks, e.g. in the arc code.
	 */
	if((par->state&(Simplemask|Simplesrc|Replmask|Replsrc))==0
	&& dst->chan==GREY1 && src->chan==GREY1 && par->mask->chan==GREY1
	&& (par->r.min.x&7)==(par->sr.min.x&7) && (par->r.min.x&7)==(par->mr.min.x&7)){
		uchar *sp, *dp, *mp;
		uchar lm, rm;
		long swid, dwid, mwid;
		int i, x, dir;

		sp = byteaddr(src, par->sr.min);
		dp = byteaddr(dst, par->r.min);
		mp = byteaddr(par->mask, par->mr.min);
		swid = src->width*sizeof(u32int);
		dwid = dst->width*sizeof(u32int);
		mwid = par->mask->width*sizeof(u32int);

		if(src->data == dst->data && byteaddr(dst, par->r.min) > byteaddr(src, par->sr.min)){
			dir = -1;
		}else
			dir = 1;

		lm = 0xFF>>(par->r.min.x&7);
		rm = 0xFF<<(8-(par->r.max.x&7));
		dx -= (8-(par->r.min.x&7)) + (par->r.max.x&7);

		if(dx < 0){	/* one byte wide */
			lm &= rm;
			if(dir == -1){
				dp += dwid*(dy-1);
				sp += swid*(dy-1);
				mp += mwid*(dy-1);
				dwid = -dwid;
				swid = -swid;
				mwid = -mwid;
			}
			for(y=0; y<dy; y++){
				*dp ^= (*dp ^ *sp) & *mp & lm;
				dp += dwid;
				sp += swid;
				mp += mwid;
			}
			return 1;
		}

		dx /= 8;
		if(dir == 1){
			i = (lm!=0)+dx+(rm!=0);
			mwid -= i;
			swid -= i;
			dwid -= i;
			for(y=0; y<dy; y++, dp+=dwid, sp+=swid, mp+=mwid){
				if(lm){
					*dp ^= (*dp ^ *sp++) & *mp++ & lm;
					dp++;
				}
				for(x=0; x<dx; x++){
					*dp ^= (*dp ^ *sp++) & *mp++;
					dp++;
				}
				if(rm){
					*dp ^= (*dp ^ *sp++) & *mp++ & rm;
					dp++;
				}
			}
			return 1;
		}else{
		/* dir == -1 */
			i = (lm!=0)+dx+(rm!=0);
			dp += dwid*(dy-1)+i-1;
			sp += swid*(dy-1)+i-1;
			mp += mwid*(dy-1)+i-1;
			dwid = -dwid+i;
			swid = -swid+i;
			mwid = -mwid+i;
			for(y=0; y<dy; y++, dp+=dwid, sp+=swid, mp+=mwid){
				if(rm){
					*dp ^= (*dp ^ *sp--) & *mp-- & rm;
					dp--;
				}
				for(x=0; x<dx; x++){
					*dp ^= (*dp ^ *sp--) & *mp--;
					dp--;
				}
				if(lm){
					*dp ^= (*dp ^ *sp--) & *mp-- & lm;
					dp--;
				}
			}
		}
		return 1;
	}
	return 0;
}
#undef DBG

/*
 * Boolean character drawing.
 * Solid opaque color through a 1-bit greyscale mask.
 */
#define DBG if(0)
static int
chardraw(Memdrawparam *par)
{
	u32int bits;
	int i, ddepth, dy, dx, x, bx, ex, y, npack, bsh, depth, op;
	u32int v, maskwid, dstwid;
	uchar *wp, *rp, *q, *wc;
	ushort *ws;
	u32int *wl;
	uchar sp[4];
	Rectangle r, mr;
	Memimage *mask, *src, *dst;
	union {
		// black box to hide pointer conversions from gcc.
		// we'll see how long this works.
		uchar *u8;
		u16int *u16;
		u32int *u32;
	} gcc_black_box;

if(0) if(drawdebug) iprint("chardraw? mf %lux md %d sf %lux dxs %d dys %d dd %d ddat %p sdat %p\n",
		par->mask->flags, par->mask->depth, par->src->flags,
		Dx(par->src->r), Dy(par->src->r), par->dst->depth, par->dst->data, par->src->data);

	mask = par->mask;
	src = par->src;
	dst = par->dst;
	r = par->r;
	mr = par->mr;
	op = par->op;

	if((par->state&(Replsrc|Simplesrc|Fullsrc|Replmask)) != (Replsrc|Simplesrc|Fullsrc)
	|| mask->depth != 1 || dst->depth<8 || dst->data==src->data
	|| op != SoverD)
		return 0;

/*if(drawdebug) iprint("chardraw..."); */

	depth = mask->depth;
	maskwid = mask->width*sizeof(u32int);
	rp = byteaddr(mask, mr.min);
	npack = 8/depth;
	bsh = (mr.min.x % npack) * depth;

	wp = byteaddr(dst, r.min);
	dstwid = dst->width*sizeof(u32int);
DBG print("bsh %d\n", bsh);
	dy = Dy(r);
	dx = Dx(r);

	ddepth = dst->depth;

	/*
	 * for loop counts from bsh to bsh+dx
	 *
	 * we want the bottom bits to be the amount
	 * to shift the pixels down, so for n≡0 (mod 8) we want
	 * bottom bits 7.  for n≡1, 6, etc.
	 * the bits come from -n-1.
	 */

	bx = -bsh-1;
	ex = -bsh-1-dx;
	SET(bits);
	v = par->sdval;

	/* make little endian */
	sp[0] = v;
	sp[1] = v>>8;
	sp[2] = v>>16;
	sp[3] = v>>24;

/*print("sp %x %x %x %x\n", sp[0], sp[1], sp[2], sp[3]); */
	for(y=0; y<dy; y++, rp+=maskwid, wp+=dstwid){
		q = rp;
		if(bsh)
			bits = *q++;
		switch(ddepth){
		case 8:
/*if(drawdebug) iprint("8loop..."); */
			wc = wp;
			for(x=bx; x>ex; x--, wc++){
				i = x&7;
				if(i == 8-1)
					bits = *q++;
DBG print("bits %lux sh %d...", bits, i);
				if((bits>>i)&1)
					*wc = v;
			}
			break;
		case 16:
			gcc_black_box.u8 = wp;
			ws = gcc_black_box.u16;
			gcc_black_box.u8 = sp;
			v = *gcc_black_box.u16;
			for(x=bx; x>ex; x--, ws++){
				i = x&7;
				if(i == 8-1)
					bits = *q++;
DBG print("bits %lux sh %d...", bits, i);
				if((bits>>i)&1)
					*ws = v;
			}
			break;
		case 24:
			wc = wp;
			for(x=bx; x>ex; x--, wc+=3){
				i = x&7;
				if(i == 8-1)
					bits = *q++;
DBG print("bits %lux sh %d...", bits, i);
				if((bits>>i)&1){
					wc[0] = sp[0];
					wc[1] = sp[1];
					wc[2] = sp[2];
				}
			}
			break;
		case 32:
			gcc_black_box.u8 = wp;
			wl = gcc_black_box.u32;
			gcc_black_box.u8 = sp;
			v = *gcc_black_box.u32;
			for(x=bx; x>ex; x--, wl++){
				i = x&7;
				if(i == 8-1)
					bits = *q++;
DBG iprint("bits %lux sh %d...", bits, i);
				if((bits>>i)&1)
					*wl = v;
			}
			break;
		}
	}

DBG print("\n");
	return 1;
}
#undef DBG


/*
 * Fill entire byte with replicated (if necessary) copy of source pixel,
 * assuming destination ldepth is >= source ldepth.
 *
 * This code is just plain wrong for >8bpp.
 *
u32int
membyteval(Memimage *src)
{
	int i, val, bpp;
	uchar uc;

	unloadmemimage(src, src->r, &uc, 1);
	bpp = src->depth;
	uc <<= (src->r.min.x&(7/src->depth))*src->depth;
	uc &= ~(0xFF>>bpp);
	* pixel value is now in high part of byte. repeat throughout byte
	val = uc;
	for(i=bpp; i<8; i<<=1)
		val |= val>>i;
	return val;
}
 *
 */

void
_memfillcolor(Memimage *i, u32int val)
{
	u32int bits;
	int d, y;
	uchar p[4];

	if(val == DNofill)
		return;

	bits = _rgbatoimg(i, val);
	switch(i->depth){
	case 24:	/* 24-bit images suck */
		for(y=i->r.min.y; y<i->r.max.y; y++)
			memset24(byteaddr(i, Pt(i->r.min.x, y)), bits, Dx(i->r));
		break;
	default:	/* 1, 2, 4, 8, 16, 32 */
		for(d=i->depth; d<32; d*=2)
			bits = (bits << d) | bits;
		p[0] = bits;		/* make little endian */
		p[1] = bits>>8;
		p[2] = bits>>16;
		p[3] = bits>>24;
		memmove(&bits, p, 4);
		memsetl(wordaddr(i, i->r.min), bits, i->width*Dy(i->r));
		break;
	}
}
