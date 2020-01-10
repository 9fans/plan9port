#include <u.h>
#include <libc.h>
#include <draw.h>

Subfont*
allocsubfont(char *name, int n, int height, int ascent, Fontchar *info, Image *i)
{
	Subfont *f, *cf;

	assert(height != 0 /* allocsubfont */);

	f = malloc(sizeof(Subfont));
	if(f == 0)
		return 0;
	f->n = n;
	f->height = height;
	f->ascent = ascent;
	f->info = info;
	f->bits = i;
	f->ref = 1;
	if(name){
		/*
		 * if already caching this subfont, leave older
		 * (and hopefully more widely used) copy in cache.
		 * this case should not happen -- we got called
		 * because cachechars needed this subfont and it
		 * wasn't in the cache.
		 */
		f->name = strdup(name);
		if((cf=lookupsubfont(i->display, name)) == 0)
			installsubfont(name, f);
		else
			freesubfont(cf);	/* drop ref we just picked up */
	}else
		f->name = 0;
	return f;
}
