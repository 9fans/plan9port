#include <u.h>
#include <libc.h>
#include <draw.h>

static int	fontresize(Font*, int, int, int);
#if 0
static int	freeup(Font*);
#endif

#define	PJW	0	/* use NUL==pjw for invisible characters */

static Rune empty[] = { 0 };
int
cachechars(Font *f, char **ss, Rune **rr, ushort *cp, int max, int *wp, char **subfontname)
{
	int i, th, sh, h, ld, w, rw, wid, nc;
	char *sp;
	Rune r, *rp, vr;
	ulong a;
	Cacheinfo *c, *tc, *ec;

	if(ss){
		sp = *ss;
		rp = empty;
	}else{
		sp = "";
		rp = *rr;
	}
	wid = 0;
	*subfontname = 0;
	for(i=0; i<max && (*sp || *rp); sp+=w, rp+=rw){
		if(ss){
			r = *(uchar*)sp;
			if(r < Runeself)
				w = 1;
			else{
				w = chartorune(&vr, sp);
				r = vr;
			}
			rw = 0;
		}else{
			r = *rp;
			w = 0;
			rw = 1;
		}

		sh = (17 * (uint)r) & (f->ncache-NFLOOK-1);
		c = &f->cache[sh];
		ec = c+NFLOOK;
		h = sh;
		while(c < ec){
			if(c->value==r && c->age)
				goto Found;
			c++;
			h++;
		}

		/*
		 * Not found; toss out oldest entry
		 */
		a = ~0;
		th = sh;
		tc = &f->cache[th];
		while(tc < ec){
			if(tc->age < a){
				a = tc->age;
				h = th;
				c = tc;
			}
			tc++;
			th++;
		}

		if(a && (f->age-a)<500){	/* kicking out too recent; resize */
			nc = 2*(f->ncache-NFLOOK) + NFLOOK;
			if(nc <= MAXFCACHE){
				if(i == 0)
					fontresize(f, f->width, nc, f->maxdepth);
				/* else flush first; retry will resize */
				break;
			}
		}

		if(c->age == f->age)	/* flush pending string output */
			break;

		ld = loadchar(f, r, c, h, i, subfontname);
		if(ld <= 0){
			if(ld == 0)
				continue;
			break;
		}
		c = &f->cache[h];	/* may have reallocated f->cache */

	    Found:
		wid += c->width;
		c->age = f->age;
		cp[i] = h;
		i++;
	}
	if(ss)
		*ss = sp;
	else
		*rr = rp;
	*wp = wid;
	return i;
}

void
agefont(Font *f)
{
	Cacheinfo *c, *ec;
	Cachesubf *s, *es;

	f->age++;
	if(f->age == 65536){
		/*
		 * Renormalize ages
		 */
		c = f->cache;
		ec = c+f->ncache;
		while(c < ec){
			if(c->age){
				c->age >>= 2;
				c->age++;
			}
			c++;
		}
		s = f->subf;
		es = s+f->nsubf;
		while(s < es){
			if(s->age){
				if(s->age<SUBFAGE && s->cf->name != nil){
					/* clean up */
					freesubfont(s->f);
					s->cf = nil;
					s->f = nil;
					s->age = 0;
				}else{
					s->age >>= 2;
					s->age++;
				}
			}
			s++;
		}
		f->age = (65536>>2) + 1;
	}
}

static Subfont*
cf2subfont(Cachefont *cf, Font *f)
{
	int depth;
	char *name;
	Subfont *sf;

	name = cf->subfontname;
	if(name == nil){
		depth = 0;
		if(f->display){
			if(f->display->screenimage)
				depth = f->display->screenimage->depth;
		}else
			depth = 8;
		name = subfontname(cf->name, f->name, depth);
		if(name == nil)
			return nil;
		cf->subfontname = name;
	}
	sf = lookupsubfont(f->display, name);
	return sf;
}

/* return 1 if load succeeded, 0 if failed, -1 if must retry */
int
loadchar(Font *f, Rune r, Cacheinfo *c, int h, int noflush, char **subfontname)
{
	int i, oi, wid, top, bottom;
	int pic;	/* need >16 bits for adding offset below */
	Fontchar *fi;
	Cachefont *cf;
	Cachesubf *subf, *of;
	uchar *b;

	pic = r;
    Again:
	for(i=0; i<f->nsub; i++){
		cf = f->sub[i];
		if(cf->min<=pic && pic<=cf->max)
			goto Found;
	}
    TryPJW:
	if(pic != PJW){
		pic = PJW;
		goto Again;
	}
	return 0;

    Found:
	/*
	 * Choose exact or oldest
	 */
	oi = 0;
	subf = &f->subf[0];
	for(i=0; i<f->nsubf; i++){
		if(cf == subf->cf)
			goto Found2;
		if(subf->age < f->subf[oi].age)
			oi = i;
		subf++;
	}
	subf = &f->subf[oi];

	if(subf->f){
		if(f->age-subf->age>SUBFAGE || f->nsubf>MAXSUBF){
    Toss:
			/* ancient data; toss */
			freesubfont(subf->f);
			subf->cf = nil;
			subf->f = nil;
			subf->age = 0;
		}else{				/* too recent; grow instead */
			of = f->subf;
			f->subf = realloc(of, (f->nsubf+DSUBF)*sizeof *subf);
			if(f->subf == nil){
				f->subf = of;
				goto Toss;
			}
			memset(f->subf+f->nsubf, 0, DSUBF*sizeof *subf);
			subf = &f->subf[f->nsubf];
			f->nsubf += DSUBF;
		}
	}
	subf->age = 0;
	subf->cf = nil;
	subf->f = cf2subfont(cf, f);
	if(subf->f == nil){
		if(cf->subfontname == nil)
			goto TryPJW;
		*subfontname = cf->subfontname;
		return -1;
	}

	subf->cf = cf;
	if(subf->f->ascent > f->ascent && f->display){
		/* should print something? this is a mistake in the font file */
		/* must prevent c->top from going negative when loading cache */
		Image *b;
		int d, t;
		d = subf->f->ascent - f->ascent;
		b = subf->f->bits;
		draw(b, b->r, b, nil, addpt(b->r.min, Pt(0, d)));
		draw(b, Rect(b->r.min.x, b->r.max.y-d, b->r.max.x, b->r.max.y), f->display->black, nil, b->r.min);
		for(i=0; i<subf->f->n; i++){
			t = subf->f->info[i].top-d;
			if(t < 0)
				t = 0;
			subf->f->info[i].top = t;
			t = subf->f->info[i].bottom-d;
			if(t < 0)
				t = 0;
			subf->f->info[i].bottom = t;
		}
		subf->f->ascent = f->ascent;
	}

    Found2:
	subf->age = f->age;

	/* possible overflow here, but works out okay */
	pic += cf->offset;
	pic -= cf->min;
	if(pic >= subf->f->n)
		goto TryPJW;
	fi = &subf->f->info[pic];
	if(fi->width == 0)
		goto TryPJW;
	wid = (fi+1)->x - fi->x;
	if(f->width < wid || f->width == 0 || f->maxdepth < subf->f->bits->depth){
		/*
		 * Flush, free, reload (easier than reformatting f->b)
		 */
		if(noflush)
			return -1;
		if(f->width < wid)
			f->width = wid;
		if(f->maxdepth < subf->f->bits->depth)
			f->maxdepth = subf->f->bits->depth;
		i = fontresize(f, f->width, f->ncache, f->maxdepth);
		if(i <= 0)
			return i;
		/* c is still valid as didn't reallocate f->cache */
	}
	c->value = r;
	top = fi->top + (f->ascent-subf->f->ascent);
	bottom = fi->bottom + (f->ascent-subf->f->ascent);
	c->width = fi->width;
	c->x = h*f->width;
	c->left = fi->left;
	if(f->display == nil)
		return 1;
	flushimage(f->display, 0);	/* flush any pending errors */
	b = bufimage(f->display, 37);
	if(b == 0)
		return 0;
	b[0] = 'l';
	BPLONG(b+1, f->cacheimage->id);
	BPLONG(b+5, subf->f->bits->id);
	BPSHORT(b+9, c-f->cache);
	BPLONG(b+11, c->x);
	BPLONG(b+15, top);
	BPLONG(b+19, c->x+((fi+1)->x-fi->x));
	BPLONG(b+23, bottom);
	BPLONG(b+27, fi->x);
	BPLONG(b+31, fi->top);
	b[35] = fi->left;
	b[36] = fi->width;
	return 1;
}

/* release all subfonts, return number freed */
#if 0
static
int
freeup(Font *f)
{
	Cachesubf *s, *es;
	int nf;

	if(f->sub[0]->name == nil)	/* font from mkfont; don't free */
		return 0;
	s = f->subf;
	es = s+f->nsubf;
	nf = 0;
	while(s < es){
		if(s->age){
			freesubfont(s->f);
			s->cf = nil;
			s->f = nil;
			s->age = 0;
			nf++;
		}
		s++;
	}
	return nf;
}
#endif

/* return whether resize succeeded && f->cache is unchanged */
static int
fontresize(Font *f, int wid, int ncache, int depth)
{
	Cacheinfo *i;
	int ret;
	Image *new;
	uchar *b;
	Display *d;

	ret = 0;
	if(depth <= 0)
		depth = 1;

	d = f->display;
	if(d == nil)
		goto Nodisplay;

	new = allocimage(d, Rect(0, 0, ncache*wid, f->height), CHAN1(CGrey, depth), 0, 0);
	if(new == nil){
		fprint(2, "font cache resize failed: %r\n");
		abort();
		goto Return;
	}
	flushimage(d, 0);	/* flush any pending errors */
	b = bufimage(d, 1+4+4+1);
	if(b == 0){
		freeimage(new);
		goto Return;
	}
	b[0] = 'i';
	BPLONG(b+1, new->id);
	BPLONG(b+5, ncache);
	b[9] = f->ascent;
	if(flushimage(d, 0) < 0){
		fprint(2, "resize: init failed: %r\n");
		freeimage(new);
		goto Return;
	}
	freeimage(f->cacheimage);
	f->cacheimage = new;
    Nodisplay:
	f->width = wid;
	f->maxdepth = depth;
	ret = 1;
	if(f->ncache != ncache){
		i = malloc(ncache*sizeof f->cache[0]);
		if(i != nil){
			ret = 0;
			free(f->cache);
			f->ncache = ncache;
			f->cache = i;
		}
		/* else just wipe the cache clean and things will be ok */
	}
    Return:
	memset(f->cache, 0, f->ncache*sizeof f->cache[0]);
	return ret;
}
