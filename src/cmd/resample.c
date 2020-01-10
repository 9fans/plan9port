#include <u.h>
#include <libc.h>
#include <draw.h>
#include <memdraw.h>

#define K2 7	/* from -.7 to +.7 inclusive, meaning .2 into each adjacent pixel */
#define NK (2*K2+1)
double K[NK];

double
fac(int L)
{
	int i, f;

	f = 1;
	for(i=L; i>1; --i)
		f *= i;
	return f;
}

/*
 * i0(x) is the modified Bessel function, Σ (x/2)^2L / (L!)²
 * There are faster ways to calculate this, but we precompute
 * into a table so let's keep it simple.
 */
double
i0(double x)
{
	double v;
	int L;

	v = 1.0;
	for(L=1; L<10; L++)
		v += pow(x/2., 2*L)/pow(fac(L), 2);
	return v;
}

double
kaiser(double x, double tau, double alpha)
{
	if(fabs(x) > tau)
		return 0.;
	return i0(alpha*sqrt(1-(x*x/(tau*tau))))/i0(alpha);
}

void
usage(void)
{
	fprint(2, "usage: resample [-x xsize] [-y ysize] [imagefile]\n");
	fprint(2, "\twhere size is an integer or a percentage in the form 25%%\n");
	exits("usage");
}

int
getint(char *s, int *percent)
{
	if(s == nil)
		usage();
	*percent = (s[strlen(s)-1] == '%');
	if(*s == '+')
		return atoi(s+1);
	if(*s == '-')
		return -atoi(s+1);
	return atoi(s);
}

void
resamplex(uchar *in, int off, int d, int inx, uchar *out, int outx)
{
	int i, x, k;
	double X, xx, v, rat;


	rat = (double)inx/(double)outx;
	for(x=0; x<outx; x++){
		if(inx == outx){
			/* don't resample if size unchanged */
			out[off+x*d] = in[off+x*d];
			continue;
		}
		v = 0.0;
		X = x*rat;
		for(k=-K2; k<=K2; k++){
			xx = X + rat*k/10.;
			i = xx;
			if(i < 0)
				i = 0;
			if(i >= inx)
				i = inx-1;
			v += in[off+i*d] * K[K2+k];
		}
		out[off+x*d] = v;
	}
}

void
resampley(uchar **in, int off, int iny, uchar **out, int outy)
{
	int y, i, k;
	double Y, yy, v, rat;

	rat = (double)iny/(double)outy;
	for(y=0; y<outy; y++){
		if(iny == outy){
			/* don't resample if size unchanged */
			out[y][off] = in[y][off];
			continue;
		}
		v = 0.0;
		Y = y*rat;
		for(k=-K2; k<=K2; k++){
			yy = Y + rat*k/10.;
			i = yy;
			if(i < 0)
				i = 0;
			if(i >= iny)
				i = iny-1;
			v += in[i][off] * K[K2+k];
		}
		out[y][off] = v;
	}

}

int
max(int a, int b)
{
	if(a > b)
		return a;
	return b;
}

Memimage*
resample(int xsize, int ysize, Memimage *m)
{
	int i, j, bpl, nchan;
	Memimage *new;
	uchar **oscan, **nscan;

	new = allocmemimage(Rect(0, 0, xsize, ysize), m->chan);
	if(new == nil)
		sysfatal("can't allocate new image: %r");

	oscan = malloc(Dy(m->r)*sizeof(uchar*));
	nscan = malloc(max(ysize, Dy(m->r))*sizeof(uchar*));
	if(oscan == nil || nscan == nil)
		sysfatal("can't allocate: %r");

	/* unload original image into scan lines */
	bpl = bytesperline(m->r, m->depth);
	for(i=0; i<Dy(m->r); i++){
		oscan[i] = malloc(bpl);
		if(oscan[i] == nil)
			sysfatal("can't allocate: %r");
		j = unloadmemimage(m, Rect(m->r.min.x, m->r.min.y+i, m->r.max.x, m->r.min.y+i+1), oscan[i], bpl);
		if(j != bpl)
			sysfatal("unloadmemimage");
	}

	/* allocate scan lines for destination. we do y first, so need at least Dy(m->r) lines */
	bpl = bytesperline(Rect(0, 0, xsize, Dy(m->r)), m->depth);
	for(i=0; i<max(ysize, Dy(m->r)); i++){
		nscan[i] = malloc(bpl);
		if(nscan[i] == nil)
			sysfatal("can't allocate: %r");
	}

	/* resample in X */
	nchan = m->depth/8;
	for(i=0; i<Dy(m->r); i++){
		for(j=0; j<nchan; j++){
			if(j==0 && m->chan==XRGB32)
				continue;
			resamplex(oscan[i], j, nchan, Dx(m->r), nscan[i], xsize);
		}
		free(oscan[i]);
		oscan[i] = nscan[i];
		nscan[i] = malloc(bpl);
		if(nscan[i] == nil)
			sysfatal("can't allocate: %r");
	}

	/* resample in Y */
	for(i=0; i<xsize; i++)
		for(j=0; j<nchan; j++)
			resampley(oscan, nchan*i+j, Dy(m->r), nscan, ysize);

	/* pack data into destination */
	bpl = bytesperline(new->r, m->depth);
	for(i=0; i<ysize; i++){
		j = loadmemimage(new, Rect(0, i, xsize, i+1), nscan[i], bpl);
		if(j != bpl)
			sysfatal("loadmemimage: %r");
	}
	return new;
}

void
main(int argc, char *argv[])
{
	int i, fd, xsize, ysize, xpercent, ypercent;
	Rectangle rparam;
	Memimage *m, *new, *t1, *t2;
	char *file;
	ulong tchan;
	char tmp[100];
	double v;

	for(i=-K2; i<=K2; i++){
		K[K2+i] = kaiser(i/10., K2/10., 4.);
/*		print("%g %g\n", i/10., K[K2+i]); */
	}

	/* normalize */
	v = 0.0;
	for(i=0; i<NK; i++)
		v += K[i];
	for(i=0; i<NK; i++)
		K[i] /= v;

	memimageinit();
	memset(&rparam, 0, sizeof rparam);
	xsize = ysize = 0;
	xpercent = ypercent = 0;

	ARGBEGIN{
	case 'a':	/* compatibility; equivalent to just -x or -y */
		if(xsize != 0 || ysize != 0)
			usage();
		xsize = getint(ARGF(), &xpercent);
		if(xsize <= 0)
			usage();
		ysize = xsize;
		ypercent = xpercent;
		break;
	case 'x':
		if(xsize != 0)
			usage();
		xsize = getint(ARGF(), &xpercent);
		if(xsize <= 0)
			usage();
		break;
	case 'y':
		if(ysize != 0)
			usage();
		ysize = getint(ARGF(), &ypercent);
		if(ysize <= 0)
			usage();
		break;
	default:
		usage();
	}ARGEND

	if(xsize == 0 && ysize == 0)
		usage();

	file = "<stdin>";
	fd = 0;
	if(argc > 1)
		usage();
	else if(argc == 1){
		file = argv[0];
		fd = open(file, OREAD);
		if(fd < 0)
			sysfatal("can't open %s: %r", file);
	}

	m = readmemimage(fd);
	if(m == nil)
		sysfatal("can't read %s: %r", file);

	if(xpercent)
		xsize = Dx(m->r)*xsize/100;
	if(ypercent)
		ysize = Dy(m->r)*ysize/100;
	if(ysize == 0)
		ysize = (xsize * Dy(m->r)) / Dx(m->r);
	if(xsize == 0)
		xsize = (ysize * Dx(m->r)) / Dy(m->r);

	new = nil;
	switch(m->chan){

	case GREY8:
	case RGB24:
	case RGBA32:
	case ARGB32:
	case XRGB32:
		new = resample(xsize, ysize, m);
		break;

	case CMAP8:
	case RGB15:
	case RGB16:
		tchan = RGB24;
		goto Convert;

	case GREY1:
	case GREY2:
	case GREY4:
		tchan = GREY8;
	Convert:
		/* use library to convert to byte-per-chan form, then convert back */
		t1 = allocmemimage(m->r, tchan);
		if(t1 == nil)
			sysfatal("can't allocate temporary image: %r");
		memimagedraw(t1, t1->r, m, m->r.min, nil, ZP, S);
		t2 = resample(xsize, ysize, t1);
		freememimage(t1);
		new = allocmemimage(Rect(0, 0, xsize, ysize), m->chan);
		if(new == nil)
			sysfatal("can't allocate new image: %r");
		/* should do error diffusion here */
		memimagedraw(new, new->r, t2, t2->r.min, nil, ZP, S);
		freememimage(t2);
		break;

	default:
		sysfatal("can't handle channel type %s", chantostr(tmp, m->chan));
	}

	assert(new);
	if(writememimage(1, new) < 0)
		sysfatal("write error on output: %r");

	exits(nil);
}
