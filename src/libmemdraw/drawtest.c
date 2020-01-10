#include <u.h>
#include <libc.h>
#include <bio.h>
#include <draw.h>
#include <memdraw.h>

#define DBG if(0)
#define RGB2K(r,g,b)	((299*((u32int)(r))+587*((u32int)(g))+114*((u32int)(b)))/1000)

/*
 * This program tests the 'memimagedraw' primitive stochastically.
 * It tests the combination aspects of it thoroughly, but since the
 * three images it uses are disjoint, it makes no check of the
 * correct behavior when images overlap.  That is, however, much
 * easier to get right and to test.
 */

void	drawonepixel(Memimage*, Point, Memimage*, Point, Memimage*, Point);
void	verifyone(void);
void	verifyline(void);
void	verifyrect(void);
void	verifyrectrepl(int, int);
void putpixel(Memimage *img, Point pt, u32int nv);
u32int rgbatopix(uchar, uchar, uchar, uchar);

char *dchan, *schan, *mchan;
int dbpp, sbpp, mbpp;

int drawdebug=0;
int	seed;
int	niters = 100;
int	dbpp;	/* bits per pixel in destination */
int	sbpp;	/* bits per pixel in src */
int	mbpp;	/* bits per pixel in mask */
int	dpm;	/* pixel mask at high part of byte, in destination */
int	nbytes;	/* in destination */

int	Xrange	= 64;
int	Yrange	= 8;

Memimage	*dst;
Memimage	*src;
Memimage	*mask;
Memimage	*stmp;
Memimage	*mtmp;
Memimage	*ones;
uchar	*dstbits;
uchar	*srcbits;
uchar	*maskbits;
u32int	*savedstbits;

void
rdb(void)
{
}

int
iprint(char *fmt, ...)
{
	int n;
	va_list va;
	char buf[1024];

	va_start(va, fmt);
	n = vseprint(buf, buf+sizeof buf, fmt, va) - buf;
	va_end(va);

	write(1,buf,n);
	return 1;
}

void
main(int argc, char *argv[])
{
	memimageinit();
	seed = time(0);

	ARGBEGIN{
	case 'x':
		Xrange = atoi(ARGF());
		break;
	case 'y':
		Yrange = atoi(ARGF());
		break;
	case 'n':
		niters = atoi(ARGF());
		break;
	case 's':
		seed = atoi(ARGF());
		break;
	}ARGEND

	dchan = "r8g8b8";
	schan = "r8g8b8";
	mchan = "r8g8b8";
	switch(argc){
	case 3:	mchan = argv[2];
	case 2:	schan = argv[1];
	case 1:	dchan = argv[0];
	case 0:	break;
	default:	goto Usage;
	Usage:
		fprint(2, "usage: dtest [dchan [schan [mchan]]]\n");
		exits("usage");
	}

	fprint(2, "%s -x %d -y %d -s 0x%x %s %s %s\n", argv0, Xrange, Yrange, seed, dchan, schan, mchan);
	srand(seed);

	dst = allocmemimage(Rect(0, 0, Xrange, Yrange), strtochan(dchan));
	src = allocmemimage(Rect(0, 0, Xrange, Yrange), strtochan(schan));
	mask = allocmemimage(Rect(0, 0, Xrange, Yrange), strtochan(mchan));
	stmp = allocmemimage(Rect(0, 0, Xrange, Yrange), strtochan(schan));
	mtmp = allocmemimage(Rect(0, 0, Xrange, Yrange), strtochan(mchan));
	ones = allocmemimage(Rect(0, 0, Xrange, Yrange), strtochan(mchan));
/*	print("chan %lux %lux %lux %lux %lux %lux\n", dst->chan, src->chan, mask->chan, stmp->chan, mtmp->chan, ones->chan); */
	if(dst==0 || src==0 || mask==0 || mtmp==0 || ones==0) {
	Alloc:
		fprint(2, "dtest: allocation failed: %r\n");
		exits("alloc");
	}
	nbytes = (4*Xrange+4)*Yrange;
	srcbits = malloc(nbytes);
	dstbits = malloc(nbytes);
	maskbits = malloc(nbytes);
	savedstbits = malloc(nbytes);
	if(dstbits==0 || srcbits==0 || maskbits==0 || savedstbits==0)
		goto Alloc;
	dbpp = dst->depth;
	sbpp = src->depth;
	mbpp = mask->depth;
	dpm = 0xFF ^ (0xFF>>dbpp);
	memset(ones->data->bdata, 0xFF, ones->width*sizeof(u32int)*Yrange);


	fprint(2, "dtest: verify single pixel operation\n");
	verifyone();

	fprint(2, "dtest: verify full line non-replicated\n");
	verifyline();

	fprint(2, "dtest: verify full rectangle non-replicated\n");
	verifyrect();

	fprint(2, "dtest: verify full rectangle source replicated\n");
	verifyrectrepl(1, 0);

	fprint(2, "dtest: verify full rectangle mask replicated\n");
	verifyrectrepl(0, 1);

	fprint(2, "dtest: verify full rectangle source and mask replicated\n");
	verifyrectrepl(1, 1);

	exits(0);
}

/*
 * Dump out an ASCII representation of an image.  The label specifies
 * a list of characters to put at various points in the picture.
 */
static void
Bprintr5g6b5(Biobuf *bio, char* _, u32int v)
{
	int r,g,b;
	r = (v>>11)&31;
	g = (v>>5)&63;
	b = v&31;
	Bprint(bio, "%.2x%.2x%.2x", r,g,b);
}

static void
Bprintr5g5b5a1(Biobuf *bio, char* _, u32int v)
{
	int r,g,b,a;
	r = (v>>11)&31;
	g = (v>>6)&31;
	b = (v>>1)&31;
	a = v&1;
	Bprint(bio, "%.2x%.2x%.2x%.2x", r,g,b,a);
}

void
dumpimage(char *name, Memimage *img, void *vdata, Point labelpt)
{
	Biobuf b;
	uchar *data;
	uchar *p;
	char *arg;
	void (*fmt)(Biobuf*, char*, u32int);
	int npr, x, y, nb, bpp;
	u32int v, mask;
	Rectangle r;

	fmt = nil;
	arg = nil;
	switch(img->depth){
	case 1:
	case 2:
	case 4:
		fmt = (void(*)(Biobuf*,char*,u32int))Bprint;
		arg = "%.1ux";
		break;
	case 8:
		fmt = (void(*)(Biobuf*,char*,u32int))Bprint;
		arg = "%.2ux";
		break;
	case 16:
		arg = nil;
		if(img->chan == RGB16)
			fmt = Bprintr5g6b5;
		else{
			fmt = (void(*)(Biobuf*,char*,u32int))Bprint;
			arg = "%.4ux";
		}
		break;
	case 24:
		fmt = (void(*)(Biobuf*,char*,u32int))Bprint;
		arg = "%.6lux";
		break;
	case 32:
		fmt = (void(*)(Biobuf*,char*,u32int))Bprint;
		arg = "%.8lux";
		break;
	}
	if(fmt == nil){
		fprint(2, "bad format\n");
		abort();
	}

	r  = img->r;
	Binit(&b, 2, OWRITE);
	data = vdata;
	bpp = img->depth;
	Bprint(&b, "%s\t%d\tr %R clipr %R repl %d data %p *%P\n", name, r.min.x, r, img->clipr, (img->flags&Frepl) ? 1 : 0, vdata, labelpt);
	mask = (1ULL<<bpp)-1;
/*	for(y=r.min.y; y<r.max.y; y++){ */
	for(y=0; y<Yrange; y++){
		nb = 0;
		v = 0;
		p = data+(byteaddr(img, Pt(0,y))-(uchar*)img->data->bdata);
		Bprint(&b, "%-4d\t", y);
/*		for(x=r.min.x; x<r.max.x; x++){ */
		for(x=0; x<Xrange; x++){
			if(x==0)
				Bprint(&b, "\t");

			if(x != 0 && (x%8)==0)
				Bprint(&b, " ");

			npr = 0;
			if(x==labelpt.x && y==labelpt.y){
				Bprint(&b, "*");
				npr++;
			}
			if(npr == 0)
				Bprint(&b, " ");

			while(nb < bpp){
				v &= (1<<nb)-1;
				v |= (u32int)(*p++) << nb;
				nb += 8;
			}
			nb -= bpp;
/*			print("bpp %d v %.8lux mask %.8lux nb %d\n", bpp, v, mask, nb); */
			fmt(&b, arg, (v>>nb)&mask);
		}
		Bprint(&b, "\n");
	}
	Bterm(&b);
}

/*
 * Verify that the destination pixel has the specified value.
 * The value is in the high bits of v, suitably masked, but must
 * be extracted from the destination Memimage.
 */
void
checkone(Point p, Point sp, Point mp)
{
	int delta;
	uchar *dp, *sdp;

	delta = (uchar*)byteaddr(dst, p)-(uchar*)dst->data->bdata;
	dp = (uchar*)dst->data->bdata+delta;
	sdp = (uchar*)savedstbits+delta;

	if(memcmp(dp, sdp, (dst->depth+7)/8) != 0) {
		fprint(2, "dtest: one bad pixel drawing at dst %P from source %P mask %P\n", p, sp, mp);
		fprint(2, " %.2ux %.2ux %.2ux %.2ux should be %.2ux %.2ux %.2ux %.2ux\n",
			dp[0], dp[1], dp[2], dp[3], sdp[0], sdp[1], sdp[2], sdp[3]);
		fprint(2, "addresses dst %p src %p mask %p\n", dp, byteaddr(src, sp), byteaddr(mask, mp));
		dumpimage("src", src, src->data->bdata, sp);
		dumpimage("mask", mask, mask->data->bdata, mp);
		dumpimage("origdst", dst, dstbits, p);
		dumpimage("dst", dst, dst->data->bdata, p);
		dumpimage("gooddst", dst, savedstbits, p);
		abort();
	}
}

/*
 * Verify that the destination line has the same value as the saved line.
 */
#define RECTPTS(r) (r).min.x, (r).min.y, (r).max.x, (r).max.y
void
checkline(Rectangle r, Point sp, Point mp, int y, Memimage *stmp, Memimage *mtmp)
{
	u32int *dp;
	int nb;
	u32int *saved;

	dp = wordaddr(dst, Pt(0, y));
	saved = savedstbits + y*dst->width;
	if(dst->depth < 8)
		nb = Xrange/(8/dst->depth);
	else
		nb = Xrange*(dst->depth/8);
	if(memcmp(dp, saved, nb) != 0){
		fprint(2, "dtest: bad line at y=%d; saved %p dp %p\n", y, saved, dp);
		fprint(2, "draw dst %R src %P mask %P\n", r, sp, mp);
		dumpimage("src", src, src->data->bdata, sp);
		if(stmp) dumpimage("stmp", stmp, stmp->data->bdata, sp);
		dumpimage("mask", mask, mask->data->bdata, mp);
		if(mtmp) dumpimage("mtmp", mtmp, mtmp->data->bdata, mp);
		dumpimage("origdst", dst, dstbits, r.min);
		dumpimage("dst", dst, dst->data->bdata, r.min);
		dumpimage("gooddst", dst, savedstbits, r.min);
		abort();
	}
}

/*
 * Fill the bits of an image with random data.
 * The Memimage parameter is used only to make sure
 * the data is well formatted: only ucbits is written.
 */
void
fill(Memimage *img, uchar *ucbits)
{
	int i, x, y;
	ushort *up;
	uchar alpha, r, g, b;
	void *data;

	if((img->flags&Falpha) == 0){
		up = (ushort*)ucbits;
		for(i=0; i<nbytes/2; i++)
			*up++ = lrand() >> 7;
		if(i+i != nbytes)
			*(uchar*)up = lrand() >> 7;
	}else{
		data = img->data->bdata;
		img->data->bdata = ucbits;

		for(x=img->r.min.x; x<img->r.max.x; x++)
		for(y=img->r.min.y; y<img->r.max.y; y++){
			alpha = rand() >> 4;
			r = rand()%(alpha+1);
			g = rand()%(alpha+1);
			b = rand()%(alpha+1);
			putpixel(img, Pt(x,y), rgbatopix(r,g,b,alpha));
		}
		img->data->bdata = data;
	}

}

/*
 * Mask is preset; do the rest
 */
void
verifyonemask(void)
{
	Point dp, sp, mp;

	fill(dst, dstbits);
	fill(src, srcbits);
	memmove(dst->data->bdata, dstbits, dst->width*sizeof(u32int)*Yrange);
	memmove(src->data->bdata, srcbits, src->width*sizeof(u32int)*Yrange);
	memmove(mask->data->bdata, maskbits, mask->width*sizeof(u32int)*Yrange);

	dp.x = nrand(Xrange);
	dp.y = nrand(Yrange);

	sp.x = nrand(Xrange);
	sp.y = nrand(Yrange);

	mp.x = nrand(Xrange);
	mp.y = nrand(Yrange);

	drawonepixel(dst, dp, src, sp, mask, mp);
	memmove(mask->data->bdata, maskbits, mask->width*sizeof(u32int)*Yrange);
	memmove(savedstbits, dst->data->bdata, dst->width*sizeof(u32int)*Yrange);

	memmove(dst->data->bdata, dstbits, dst->width*sizeof(u32int)*Yrange);
	memimagedraw(dst, Rect(dp.x, dp.y, dp.x+1, dp.y+1), src, sp, mask, mp, SoverD);
	memmove(mask->data->bdata, maskbits, mask->width*sizeof(u32int)*Yrange);

	checkone(dp, sp, mp);
}

void
verifyone(void)
{
	int i;

	/* mask all zeros */
	memset(maskbits, 0, nbytes);
	for(i=0; i<niters; i++)
		verifyonemask();

	/* mask all ones */
	memset(maskbits, 0xFF, nbytes);
	for(i=0; i<niters; i++)
		verifyonemask();

	/* random mask */
	for(i=0; i<niters; i++){
		fill(mask, maskbits);
		verifyonemask();
	}
}

/*
 * Mask is preset; do the rest
 */
void
verifylinemask(void)
{
	Point sp, mp, tp, up;
	Rectangle dr;
	int x;

	fill(dst, dstbits);
	fill(src, srcbits);
	memmove(dst->data->bdata, dstbits, dst->width*sizeof(u32int)*Yrange);
	memmove(src->data->bdata, srcbits, src->width*sizeof(u32int)*Yrange);
	memmove(mask->data->bdata, maskbits, mask->width*sizeof(u32int)*Yrange);

	dr.min.x = nrand(Xrange-1);
	dr.min.y = nrand(Yrange-1);
	dr.max.x = dr.min.x + 1 + nrand(Xrange-1-dr.min.x);
	dr.max.y = dr.min.y + 1;

	sp.x = nrand(Xrange);
	sp.y = nrand(Yrange);

	mp.x = nrand(Xrange);
	mp.y = nrand(Yrange);

	tp = sp;
	up = mp;
	for(x=dr.min.x; x<dr.max.x && tp.x<Xrange && up.x<Xrange; x++,tp.x++,up.x++)
		memimagedraw(dst, Rect(x, dr.min.y, x+1, dr.min.y+1), src, tp, mask, up, SoverD);
	memmove(savedstbits, dst->data->bdata, dst->width*sizeof(u32int)*Yrange);

	memmove(dst->data->bdata, dstbits, dst->width*sizeof(u32int)*Yrange);

	memimagedraw(dst, dr, src, sp, mask, mp, SoverD);
	checkline(dr, drawrepl(src->r, sp), drawrepl(mask->r, mp), dr.min.y, nil, nil);
}

void
verifyline(void)
{
	int i;

	/* mask all ones */
	memset(maskbits, 0xFF, nbytes);
	for(i=0; i<niters; i++)
		verifylinemask();

	/* mask all zeros */
	memset(maskbits, 0, nbytes);
	for(i=0; i<niters; i++)
		verifylinemask();

	/* random mask */
	for(i=0; i<niters; i++){
		fill(mask, maskbits);
		verifylinemask();
	}
}

/*
 * Mask is preset; do the rest
 */
void
verifyrectmask(void)
{
	Point sp, mp, tp, up;
	Rectangle dr;
	int x, y;

	fill(dst, dstbits);
	fill(src, srcbits);
	memmove(dst->data->bdata, dstbits, dst->width*sizeof(u32int)*Yrange);
	memmove(src->data->bdata, srcbits, src->width*sizeof(u32int)*Yrange);
	memmove(mask->data->bdata, maskbits, mask->width*sizeof(u32int)*Yrange);

	dr.min.x = nrand(Xrange-1);
	dr.min.y = nrand(Yrange-1);
	dr.max.x = dr.min.x + 1 + nrand(Xrange-1-dr.min.x);
	dr.max.y = dr.min.y + 1 + nrand(Yrange-1-dr.min.y);

	sp.x = nrand(Xrange);
	sp.y = nrand(Yrange);

	mp.x = nrand(Xrange);
	mp.y = nrand(Yrange);

	tp = sp;
	up = mp;
	for(y=dr.min.y; y<dr.max.y && tp.y<Yrange && up.y<Yrange; y++,tp.y++,up.y++){
		for(x=dr.min.x; x<dr.max.x && tp.x<Xrange && up.x<Xrange; x++,tp.x++,up.x++)
			memimagedraw(dst, Rect(x, y, x+1, y+1), src, tp, mask, up, SoverD);
		tp.x = sp.x;
		up.x = mp.x;
	}
	memmove(savedstbits, dst->data->bdata, dst->width*sizeof(u32int)*Yrange);

	memmove(dst->data->bdata, dstbits, dst->width*sizeof(u32int)*Yrange);

	memimagedraw(dst, dr, src, sp, mask, mp, SoverD);
	for(y=0; y<Yrange; y++)
		checkline(dr, drawrepl(src->r, sp), drawrepl(mask->r, mp), y, nil, nil);
}

void
verifyrect(void)
{
	int i;

	/* mask all zeros */
	memset(maskbits, 0, nbytes);
	for(i=0; i<niters; i++)
		verifyrectmask();

	/* mask all ones */
	memset(maskbits, 0xFF, nbytes);
	for(i=0; i<niters; i++)
		verifyrectmask();

	/* random mask */
	for(i=0; i<niters; i++){
		fill(mask, maskbits);
		verifyrectmask();
	}
}

Rectangle
randrect(void)
{
	Rectangle r;

	r.min.x = nrand(Xrange-1);
	r.min.y = nrand(Yrange-1);
	r.max.x = r.min.x + 1 + nrand(Xrange-1-r.min.x);
	r.max.y = r.min.y + 1 + nrand(Yrange-1-r.min.y);
	return r;
}

/*
 * Return coordinate corresponding to x withing range [minx, maxx)
 */
int
tilexy(int minx, int maxx, int x)
{
	int sx;

	sx = (x-minx) % (maxx-minx);
	if(sx < 0)
		sx += maxx-minx;
	return sx+minx;
}

void
replicate(Memimage *i, Memimage *tmp)
{
	Rectangle r, r1;
	int x, y, nb;

	/* choose the replication window (i->r) */
	r.min.x = nrand(Xrange-1);
	r.min.y = nrand(Yrange-1);
	/* make it trivial more often than pure chance allows */
	switch(lrand()&0){
	case 1:
		r.max.x = r.min.x + 2;
		r.max.y = r.min.y + 2;
		if(r.max.x < Xrange && r.max.y < Yrange)
			break;
		/* fall through */
	case 0:
		r.max.x = r.min.x + 1;
		r.max.y = r.min.y + 1;
		break;
	default:
		if(r.min.x+3 >= Xrange)
			r.max.x = Xrange;
		else
			r.max.x = r.min.x+3 + nrand(Xrange-(r.min.x+3));

		if(r.min.y+3 >= Yrange)
			r.max.y = Yrange;
		else
			r.max.y = r.min.y+3 + nrand(Yrange-(r.min.y+3));
	}
	assert(r.min.x >= 0);
	assert(r.max.x <= Xrange);
	assert(r.min.y >= 0);
	assert(r.max.y <= Yrange);
	/* copy from i to tmp so we have just the replicated bits */
	nb = tmp->width*sizeof(u32int)*Yrange;
	memset(tmp->data->bdata, 0, nb);
	memimagedraw(tmp, r, i, r.min, ones, r.min, SoverD);
	memmove(i->data->bdata, tmp->data->bdata, nb);
	/* i is now a non-replicated instance of the replication */
	/* replicate it by hand through tmp */
	memset(tmp->data->bdata, 0, nb);
	x = -(tilexy(r.min.x, r.max.x, 0)-r.min.x);
	for(; x<Xrange; x+=Dx(r)){
		y = -(tilexy(r.min.y, r.max.y, 0)-r.min.y);
		for(; y<Yrange; y+=Dy(r)){
			/* set r1 to instance of tile by translation */
			r1.min.x = x;
			r1.min.y = y;
			r1.max.x = r1.min.x+Dx(r);
			r1.max.y = r1.min.y+Dy(r);
			memimagedraw(tmp, r1, i, r.min, ones, r.min, SoverD);
		}
	}
	i->flags |= Frepl;
	i->r = r;
	i->clipr = randrect();
/*	fprint(2, "replicate [[%d %d] [%d %d]] [[%d %d][%d %d]]\n", r.min.x, r.min.y, r.max.x, r.max.y, */
/*		i->clipr.min.x, i->clipr.min.y, i->clipr.max.x, i->clipr.max.y); */
	tmp->clipr = i->clipr;
}

/*
 * Mask is preset; do the rest
 */
void
verifyrectmaskrepl(int srcrepl, int maskrepl)
{
	Point sp, mp, tp, up;
	Rectangle dr;
	int x, y;
	Memimage *s, *m;

/*	print("verfrect %d %d\n", srcrepl, maskrepl); */
	src->flags &= ~Frepl;
	src->r = Rect(0, 0, Xrange, Yrange);
	src->clipr = src->r;
	stmp->flags &= ~Frepl;
	stmp->r = Rect(0, 0, Xrange, Yrange);
	stmp->clipr = src->r;
	mask->flags &= ~Frepl;
	mask->r = Rect(0, 0, Xrange, Yrange);
	mask->clipr = mask->r;
	mtmp->flags &= ~Frepl;
	mtmp->r = Rect(0, 0, Xrange, Yrange);
	mtmp->clipr = mask->r;

	fill(dst, dstbits);
	fill(src, srcbits);

	memmove(dst->data->bdata, dstbits, dst->width*sizeof(u32int)*Yrange);
	memmove(src->data->bdata, srcbits, src->width*sizeof(u32int)*Yrange);
	memmove(mask->data->bdata, maskbits, mask->width*sizeof(u32int)*Yrange);

	if(srcrepl){
		replicate(src, stmp);
		s = stmp;
	}else
		s = src;
	if(maskrepl){
		replicate(mask, mtmp);
		m = mtmp;
	}else
		m = mask;

	dr = randrect();

	sp.x = nrand(Xrange);
	sp.y = nrand(Yrange);

	mp.x = nrand(Xrange);
	mp.y = nrand(Yrange);

DBG	print("smalldraws\n");
	for(tp.y=sp.y,up.y=mp.y,y=dr.min.y; y<dr.max.y && tp.y<Yrange && up.y<Yrange; y++,tp.y++,up.y++)
		for(tp.x=sp.x,up.x=mp.x,x=dr.min.x; x<dr.max.x && tp.x<Xrange && up.x<Xrange; x++,tp.x++,up.x++)
			memimagedraw(dst, Rect(x, y, x+1, y+1), s, tp, m, up, SoverD);
	memmove(savedstbits, dst->data->bdata, dst->width*sizeof(u32int)*Yrange);

	memmove(dst->data->bdata, dstbits, dst->width*sizeof(u32int)*Yrange);

DBG	print("bigdraw\n");
	memimagedraw(dst, dr, src, sp, mask, mp, SoverD);
	for(y=0; y<Yrange; y++)
		checkline(dr, drawrepl(src->r, sp), drawrepl(mask->r, mp), y, srcrepl?stmp:nil, maskrepl?mtmp:nil);
}

void
verifyrectrepl(int srcrepl, int maskrepl)
{
	int i;

	/* mask all ones */
	memset(maskbits, 0xFF, nbytes);
	for(i=0; i<niters; i++)
		verifyrectmaskrepl(srcrepl, maskrepl);

	/* mask all zeros */
	memset(maskbits, 0, nbytes);
	for(i=0; i<niters; i++)
		verifyrectmaskrepl(srcrepl, maskrepl);

	/* random mask */
	for(i=0; i<niters; i++){
		fill(mask, maskbits);
		verifyrectmaskrepl(srcrepl, maskrepl);
	}
}

/*
 * Trivial draw implementation.
 * Color values are passed around as u32ints containing ααRRGGBB
 */

/*
 * Convert v, which is nhave bits wide, into its nwant bits wide equivalent.
 * Replicates to widen the value, truncates to narrow it.
 */
u32int
replbits(u32int v, int nhave, int nwant)
{
	v &= (1<<nhave)-1;
	for(; nhave<nwant; nhave*=2)
		v |= v<<nhave;
	v >>= (nhave-nwant);
	return v & ((1<<nwant)-1);
}

/*
 * Decode a pixel into the uchar* values.
 */
void
pixtorgba(u32int v, uchar *r, uchar *g, uchar *b, uchar *a)
{
	*a = v>>24;
	*r = v>>16;
	*g = v>>8;
	*b = v;
}

/*
 * Convert uchar channels into u32int pixel.
 */
u32int
rgbatopix(uchar r, uchar g, uchar b, uchar a)
{
	return (a<<24)|(r<<16)|(g<<8)|b;
}

/*
 * Retrieve the pixel value at pt in the image.
 */
u32int
getpixel(Memimage *img, Point pt)
{
	uchar r, g, b, a, *p;
	int nbits, npack, bpp;
	u32int v, c, rbits, bits;

	r = g = b = 0;
	a = ~0;	/* default alpha is full */

	p = byteaddr(img, pt);
	v = p[0]|(p[1]<<8)|(p[2]<<16)|(p[3]<<24);
	bpp = img->depth;
	if(bpp<8){
		/*
		 * Sub-byte greyscale pixels.
		 *
		 * We want to throw away the top pt.x%npack pixels and then use the next bpp bits
		 * in the bottom byte of v.  This madness is due to having big endian bits
		 * but little endian bytes.
		 */
		npack = 8/bpp;
		v >>= 8 - bpp*(pt.x%npack+1);
		v &= (1<<bpp)-1;
		r = g = b = replbits(v, bpp, 8);
	}else{
		/*
		 * General case.  We need to parse the channel descriptor and do what it says.
		 * In all channels but the color map, we replicate to 8 bits because that's the
		 * precision that all calculations are done at.
		 *
		 * In the case of the color map, we leave the bits alone, in case a color map
		 * with less than 8 bits of index is used.  This is currently disallowed, so it's
		 * sort of silly.
		 */

		for(c=img->chan; c; c>>=8){
			nbits = NBITS(c);
			bits = v & ((1<<nbits)-1);
			rbits = replbits(bits, nbits, 8);
			v >>= nbits;
			switch(TYPE(c)){
			case CRed:
				r = rbits;
				break;
			case CGreen:
				g = rbits;
				break;
			case CBlue:
				b = rbits;
				break;
			case CGrey:
				r = g = b = rbits;
				break;
			case CAlpha:
				a = rbits;
				break;
			case CMap:
				p = img->cmap->cmap2rgb + 3*bits;
				r = p[0];
				g = p[1];
				b = p[2];
				break;
			case CIgnore:
				break;
			default:
				fprint(2, "unknown channel type %lud\n", TYPE(c));
				abort();
			}
		}
	}
	return rgbatopix(r, g, b, a);
}

/*
 * Return the greyscale equivalent of a pixel.
 */
uchar
getgrey(Memimage *img, Point pt)
{
	uchar r, g, b, a;
	pixtorgba(getpixel(img, pt), &r, &g, &b, &a);
	return RGB2K(r, g, b);
}

/*
 * Return the value at pt in image, if image is interpreted
 * as a mask.  This means the alpha channel if present, else
 * the greyscale or its computed equivalent.
 */
uchar
getmask(Memimage *img, Point pt)
{
	if(img->flags&Falpha)
		return getpixel(img, pt)>>24;
	else
		return getgrey(img, pt);
}
#undef DBG

#define DBG if(0)
/*
 * Write a pixel to img at point pt.
 *
 * We do this by reading a 32-bit little endian
 * value from p and then writing it back
 * after tweaking the appropriate bits.  Because
 * the data is little endian, we don't have to worry
 * about what the actual depth is, as long as it is
 * less than 32 bits.
 */
void
putpixel(Memimage *img, Point pt, u32int nv)
{
	uchar r, g, b, a, *p, *q;
	u32int c, mask, bits, v;
	int bpp, sh, npack, nbits;

	pixtorgba(nv, &r, &g, &b, &a);

	p = byteaddr(img, pt);
	v = p[0]|(p[1]<<8)|(p[2]<<16)|(p[3]<<24);
	bpp = img->depth;
DBG print("v %.8lux...", v);
	if(bpp < 8){
		/*
		 * Sub-byte greyscale pixels.  We need to skip the leftmost pt.x%npack pixels,
		 * which is equivalent to skipping the rightmost npack - pt.x%npack - 1 pixels.
		 */
		npack = 8/bpp;
		sh = bpp*(npack - pt.x%npack - 1);
		bits = RGB2K(r,g,b);
DBG print("repl %lux 8 %d = %lux...", bits, bpp, replbits(bits, 8, bpp));
		bits = replbits(bits, 8, bpp);
		mask = (1<<bpp)-1;
DBG print("bits %lux mask %lux sh %d...", bits, mask, sh);
		mask <<= sh;
		bits <<= sh;
DBG print("(%lux & %lux) | (%lux & %lux)", v, ~mask, bits, mask);
		v = (v & ~mask) | (bits & mask);
	} else {
		/*
		 * General case.  We need to parse the channel descriptor again.
		 */
		sh = 0;
		for(c=img->chan; c; c>>=8){
			nbits = NBITS(c);
			switch(TYPE(c)){
			case CRed:
				bits = r;
				break;
			case CGreen:
				bits = g;
				break;
			case CBlue:
				bits = b;
				break;
			case CGrey:
				bits = RGB2K(r, g, b);
				break;
			case CAlpha:
				bits = a;
				break;
			case CIgnore:
				bits = 0;
				break;
			case CMap:
				q = img->cmap->rgb2cmap;
				bits = q[(r>>4)*16*16+(g>>4)*16+(b>>4)];
				break;
			default:
				SET(bits);
				fprint(2, "unknown channel type %lud\n", TYPE(c));
				abort();
			}

DBG print("repl %lux 8 %d = %lux...", bits, nbits, replbits(bits, 8, nbits));
			if(TYPE(c) != CMap)
				bits = replbits(bits, 8, nbits);
			mask = (1<<nbits)-1;
DBG print("bits %lux mask %lux sh %d...", bits, mask, sh);
			bits <<= sh;
			mask <<= sh;
			v = (v & ~mask) | (bits & mask);
			sh += nbits;
		}
	}
DBG print("v %.8lux\n", v);
	p[0] = v;
	p[1] = v>>8;
	p[2] = v>>16;
	p[3] = v>>24;
}
#undef DBG

#define DBG if(0)
void
drawonepixel(Memimage *dst, Point dp, Memimage *src, Point sp, Memimage *mask, Point mp)
{
	uchar m, M, sr, sg, sb, sa, sk, dr, dg, db, da, dk;

	pixtorgba(getpixel(dst, dp), &dr, &dg, &db, &da);
	pixtorgba(getpixel(src, sp), &sr, &sg, &sb, &sa);
	m = getmask(mask, mp);
	M = 255-(sa*m + 127)/255;

DBG print("dst %x %x %x %x src %x %x %x %x m %x = ", dr,dg,db,da, sr,sg,sb,sa, m);
	if(dst->flags&Fgrey){
		/*
		 * We need to do the conversion to grey before the alpha calculation
		 * because the draw operator does this, and we need to be operating
		 * at the same precision so we get exactly the same answers.
		 */
		sk = RGB2K(sr, sg, sb);
		dk = RGB2K(dr, dg, db);
		dk = (sk*m + dk*M + 127)/255;
		dr = dg = db = dk;
		da = (sa*m + da*M + 127)/255;
	}else{
		/*
		 * True color alpha calculation treats all channels (including alpha)
		 * the same.  It might have been nice to use an array, but oh well.
		 */
		dr = (sr*m + dr*M + 127)/255;
		dg = (sg*m + dg*M + 127)/255;
		db = (sb*m + db*M + 127)/255;
		da = (sa*m + da*M + 127)/255;
	}

DBG print("%x %x %x %x\n", dr,dg,db,da);
	putpixel(dst, dp, rgbatopix(dr, dg, db, da));
}
