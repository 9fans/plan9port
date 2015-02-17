#include <u.h>
#include <libc.h>
#include <draw.h>

static char*
skip(char *s)
{
	while(*s==' ' || *s=='\n' || *s=='\t')
		s++;
	return s;
}

Font*
buildfont(Display *d, char *buf, char *name)
{
	Font *fnt;
	Cachefont *c;
	char *s, *t;
	ulong min, max;
	int offset;
	char badform[] = "bad font format: number expected (char position %d)";

	s = buf;
	fnt = malloc(sizeof(Font));
	if(fnt == 0)
		return 0;
	memset(fnt, 0, sizeof(Font));
	fnt->scale = 1;
	fnt->display = d;
	fnt->name = strdup(name);
	fnt->namespec = strdup(name);
	fnt->ncache = NFCACHE+NFLOOK;
	fnt->nsubf = NFSUBF;
	fnt->cache = malloc(fnt->ncache * sizeof(fnt->cache[0]));
	fnt->subf = malloc(fnt->nsubf * sizeof(fnt->subf[0]));
	if(fnt->name==0 || fnt->cache==0 || fnt->subf==0){
    Err2:
		free(fnt->name);
		free(fnt->cache);
		free(fnt->subf);
		free(fnt->sub);
		free(fnt);
		return 0;
	}
	fnt->height = strtol(s, &s, 0);
	s = skip(s);
	fnt->ascent = strtol(s, &s, 0);
	s = skip(s);
	if(fnt->height<=0 || fnt->ascent<=0){
		werrstr("bad height or ascent in font file");
		goto Err2;
	}
	fnt->width = 0;
	fnt->nsub = 0;
	fnt->sub = 0;

	memset(fnt->subf, 0, fnt->nsubf * sizeof(fnt->subf[0]));
	memset(fnt->cache, 0, fnt->ncache*sizeof(fnt->cache[0]));
	fnt->age = 1;
	do{
		/* must be looking at a number now */
		if(*s<'0' || '9'<*s){
			werrstr(badform, s-buf);
			goto Err3;
		}
		min = strtol(s, &s, 0);
		s = skip(s);
		/* must be looking at a number now */
		if(*s<'0' || '9'<*s){
			werrstr(badform, s-buf);
			goto Err3;
		}
		max = strtol(s, &s, 0);
		s = skip(s);
		if(*s==0 || min>Runemax || max>Runemax || min>max){
			werrstr("illegal subfont range");
    Err3:
			freefont(fnt);
			return 0;
		}
		t = s;
		offset = strtol(s, &t, 0);
		if(t>s && (*t==' ' || *t=='\t' || *t=='\n'))
			s = skip(t);
		else
			offset = 0;
		fnt->sub = realloc(fnt->sub, (fnt->nsub+1)*sizeof(Cachefont*));
		if(fnt->sub == 0){
			/* realloc manual says fnt->sub may have been destroyed */
			fnt->nsub = 0;
			goto Err3;
		}
		c = malloc(sizeof(Cachefont));
		if(c == 0)
			goto Err3;
		fnt->sub[fnt->nsub] = c;
		c->min = min;
		c->max = max;
		c->offset = offset;
		t = s;
		while(*s && *s!=' ' && *s!='\n' && *s!='\t')
			s++;
		*s++ = 0;
		c->subfontname = 0;
		c->name = strdup(t);
		if(c->name == 0){
			free(c);
			goto Err3;
		}
		s = skip(s);
		fnt->nsub++;
	}while(*s);
	return fnt;
}

void
freefont(Font *f)
{
	int i;
	Cachefont *c;
	Subfont *s;

	if(f == 0)
		return;

	for(i=0; i<f->nsub; i++){
		c = f->sub[i];
		free(c->subfontname);
		free(c->name);
		free(c);
	}
	for(i=0; i<f->nsubf; i++){
		s = f->subf[i].f;
		if(s && (!display || s!=display->defaultsubfont))
			freesubfont(s);
	}
	freeimage(f->cacheimage);
	free(f->name);
	free(f->namespec);
	free(f->cache);
	free(f->subf);
	free(f->sub);

	if(f->ondisplaylist) {
		f->ondisplaylist = 0;
		if(f->next)
			f->next->prev = f->prev;
		else
			f->display->lastfont = f->prev;
		if(f->prev)
			f->prev->next = f->next;
		else
			f->display->firstfont = f->next;
	}

	if(f->lodpi != f)	
		freefont(f->lodpi);
	if(f->hidpi != f)
		freefont(f->hidpi);

	free(f);
}
