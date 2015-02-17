#include <u.h>
#include <libc.h>
#include <draw.h>

/*
 * Default version: treat as file name
 */

int _fontpipe(char*);

static void scalesubfont(Subfont*, int);

Subfont*
_getsubfont(Display *d, char *name)
{
	int fd;
	Subfont *f;
	int scale;
	char *fname;
	
	scale = parsefontscale(name, &fname);
	fd = open(fname, OREAD);
	if(fd < 0 && strncmp(fname, "/mnt/font/", 10) == 0)
		fd = _fontpipe(fname+10);

	if(fd < 0){
		fprint(2, "getsubfont: can't open %s: %r\n", fname);
		return 0;
	}
	/*
	 * unlock display so i/o happens with display released, unless
	 * user is doing his own locking, in which case this could break things.
	 * _getsubfont is called only from string.c and stringwidth.c,
	 * which are known to be safe to have this done.
	 */
	if(d && d->locking == 0)
		unlockdisplay(d);
	f = readsubfont(d, name, fd, d && d->locking==0);
	if(d && d->locking == 0)
		lockdisplay(d);
	if(f == 0)
		fprint(2, "getsubfont: can't read %s: %r\n", name);
	close(fd);
	if(scale > 1)
		scalesubfont(f, scale);
	return f;
}

static void
scalesubfont(Subfont *f, int scale)
{
	Image *i;
	Rectangle r, r2;
	int y, x, x2, j;
	uchar *src, *dst;
	int srcn, dstn, n, mask, v, pack;
	
	r = f->bits->r;
	r2 = r;
	r2.min.x *= scale;
	r2.min.y *= scale;
	r2.max.x *= scale;
	r2.max.y *= scale;
	
	srcn = bytesperline(r, f->bits->depth);
	src = malloc(srcn);
	dstn = bytesperline(r2, f->bits->depth);
	dst = malloc(dstn+1);
	i = allocimage(f->bits->display, r2, f->bits->chan, 0, DBlack);
	for(y=r.min.y; y < r.max.y; y++) {
		n = unloadimage(f->bits, Rect(r.min.x, y, r.max.x, y+1), src, srcn);
		if(n != srcn)
			sysfatal("scalesubfont: bad unload: %d < %d: %r", n, srcn);
		memset(dst, 0, dstn+1);
		pack = 8 / f->bits->depth;
		mask = (1<<f->bits->depth) - 1;
		for(x=0; x<Dx(r); x++) {
			v = ((src[x/pack] << ((x%pack)*f->bits->depth)) >> (8 - f->bits->depth)) & mask;
			for(j=0; j<scale; j++) {
				x2 = x*scale+j;
				dst[x2/pack] |= v << (8 - f->bits->depth) >> ((x2%pack)*f->bits->depth);
			}
		}
		if(dst[dstn] != 0)
			sysfatal("overflow dst");
		for(j=0; j<scale; j++)
			loadimage(i, Rect(r2.min.x, y*scale+j, r2.max.x, y*scale+j+1), dst, dstn);
	}
	freeimage(f->bits);
	f->bits = i;
	f->height *= scale;
	f->ascent *= scale;
	
	for(j=0; j<f->n; j++) {
		f->info[j].x *= scale;
		f->info[j].top *= scale;
		f->info[j].bottom *= scale;
		f->info[j].left *= scale;
		f->info[j].width *= scale;
	}
}
