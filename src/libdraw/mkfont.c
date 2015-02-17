#include <u.h>
#include <libc.h>
#include <draw.h>

/*
 * Cobble fake font using existing subfont
 */
Font*
mkfont(Subfont *subfont, Rune min)
{
	Font *font;
	Cachefont *c;

	font = malloc(sizeof(Font));
	if(font == 0)
		return 0;
	memset(font, 0, sizeof(Font));
	font->scale = 1;
	font->display = subfont->bits->display;
	font->name = strdup("<synthetic>");
	font->namespec = strdup("<synthetic>");
	font->ncache = NFCACHE+NFLOOK;
	font->nsubf = NFSUBF;
	font->cache = malloc(font->ncache * sizeof(font->cache[0]));
	font->subf = malloc(font->nsubf * sizeof(font->subf[0]));
	if(font->name==0 || font->cache==0 || font->subf==0){
    Err:
		free(font->name);
		free(font->cache);
		free(font->subf);
		free(font->sub);
		free(font);
		return 0;
	}
	memset(font->cache, 0, font->ncache*sizeof(font->cache[0]));
	memset(font->subf, 0, font->nsubf*sizeof(font->subf[0]));
	font->height = subfont->height;
	font->ascent = subfont->ascent;
	font->age = 1;
	font->sub = malloc(sizeof(Cachefont*));
	if(font->sub == 0)
		goto Err;
	c = malloc(sizeof(Cachefont));
	if(c == 0)
		goto Err;
	font->nsub = 1;
	font->sub[0] = c;
	c->min = min;
	c->max = min+subfont->n-1;
	c->offset = 0;
	c->name = 0;	/* noticed by freeup() and agefont() */
	c->subfontname = 0;
	font->subf[0].age = 0;
	font->subf[0].cf = c;
	font->subf[0].f = subfont;
	return font;
}
