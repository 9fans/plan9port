#include	<u.h>
#include	<libc.h>
#include	<libg.h>
#include	<bio.h>

static void usage(void);
static void snarf(char *, int);
static void choose(Fontchar *, Bitmap *, int, int, int);
struct {
	char *name;
	Bitmap *bm;
	Subfont *sf;
} ft[1024];
int nf;

void
main(int argc, char **argv)
{
	int i, errs;
	Fontchar *fc;
	Bitmap *b;
	int nc, ht, as;
	Subfont *f;

	binit(0, 0, "font merge");
	if(argc < 1)
		usage();
	nf = argc-1;
	for(i = 0; i < nf; i++)
		snarf(argv[i+1], i);
	nc = ft[0].sf->n;
	ht = ft[0].sf->height;
	as = ft[0].sf->ascent;
	errs = 0;
	for(i = 0; i < nf; i++){
		if(nc < ft[i].sf->n) nc = ft[i].sf->n;
		if(ht != ft[1].sf->height){
			fprint(2, "%s: %s.height=%d (!= %s.height=%d)\n", argv[0],
				ft[i].name, ft[i].sf->height, ft[0].name, ht);
			errs = 1;
		}
		if(as != ft[1].sf->ascent){
			fprint(2, "%s: %s.ascent=%d (!= %s.ascent=%d)\n", argv[0],
				ft[i].name, ft[i].sf->ascent, ft[0].name, ht);
			errs = 1;
		}
	}
	if(errs)
		exits("param mismatch");
	fc = (Fontchar *)malloc(nc*sizeof(Fontchar));
	b = balloc(Rect(0, 0, nc*64, ht), ft[0].bm->ldepth);
	if(b == 0 || fc == 0){
		fprint(2, "%s: couldn't malloc %d chars\n", argv0, nc);
		exits("out of memory");
	}
	bitblt(b, b->r.min, b, b->r, Zero);
	choose(fc, b, nc, ht, as);
	wrbitmapfile(1, b);
bitblt(&screen, screen.r.min, b, b->r, S); bflush();sleep(5000);
	f = subfalloc(nc, ht, as, fc, b, ~0, ~0);
	wrsubfontfile(1, f);
	exits(0);
}

static void
usage(void)
{
	fprint(2, "Usage: %s file ...\n", argv0);
	exits("usage");
}

static void
snarf(char *name, int i)
{
	int fd;
	Bitmap *b;

	ft[i].name = name;
	if((fd = open(name, OREAD)) < 0){
		perror(name);
		exits("font read");
	}
	if((b = rdbitmapfile(fd)) == 0){
		fprint(2, "rdbitmapfile failed\n");
		exits("font read");
	}
	if((ft[i].bm = balloc(b->r, b->ldepth)) == 0){
		fprint(2, "ballocsnarf failed\n");
		exits("font read");
	}
	bitblt(ft[i].bm, b->r.min, b, b->r, S);
	if((ft[i].sf = rdsubfontfile(fd, b)) == 0){
		fprint(2, "rdsubfontfile failed\n");
		exits("font read");
	}
	close(fd);
}

static void
choose(Fontchar *f, Bitmap *b, int nc, int ht, int as)
{
	int j;
	Fontchar *info;
	int lastx = 0;
	int w, n;

	for(n = 0; n < nc; n++, f++){
		f->x = lastx;
		for(j = 0; j < nf; j++){
			if(n >= ft[j].sf->n)
				continue;
			info = ft[j].sf->info;
			if(info[n+1].x != info[n].x)
				goto found;
		}
		continue;
	found:
		f->left = info[n].left;
		f->top = info[n].top;
		f->bottom = info[n].bottom;
		f->width = info[n].width;
		w = info[n+1].x - info[n].x;
		bitblt(b, Pt(0, lastx), ft[j].bm, Rect(0, info[n].x, ht, info[n+1].x), S);
		lastx += w;
	}
	f->x = lastx;
}
