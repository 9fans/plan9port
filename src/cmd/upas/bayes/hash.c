#include <u.h>
#include <libc.h>
#include <bio.h>
#include "hash.h"

/***
 * String hash tables.
 */

Stringtab *tfree;

Stringtab*
taballoc(void)
{
	static Stringtab *t;
	static uint nt;

	if(tfree){
		Stringtab *tt = tfree;
		tfree = tt->link;
		return tt;
	}

	if(nt == 0){
		t = malloc(64000*sizeof(Stringtab));
		if(t == 0)
			sysfatal("out of memory");
		nt = 64000;
	}
	nt--;
	return t++;
}

void
tabfree(Stringtab *tt)
{
	tt->link = tfree;
	tfree = tt;
}

char*
xstrdup(char *s, int len)
{
	char *r;
	static char *t;
	static int nt;

	if(nt < len){
		t = malloc(512*1024+len);
		if(t == 0)
			sysfatal("out of memory");
		nt = 512*1024;
	}
	r = t;
	t += len;
	nt -= len;
	memmove(r, s, len);
	return r;
}

static uint
hash(char *s, int n)
{
	uint h;
	uchar *p, *ep;
	h = 0;
	for(p=(uchar*)s, ep=p+n; p<ep; p++)
		h = h*37 + *p;
	return h;
}

static void
rehash(Hash *hh)
{
	int h;
	Stringtab *s;

	if(hh->nstab == 0)
		hh->nstab = 1024;
	else
		hh->nstab = hh->ntab*2;

	free(hh->stab);
	hh->stab = mallocz(hh->nstab*sizeof(Stringtab*), 1);
	if(hh->stab == nil)
		sysfatal("out of memory");

	for(s=hh->all; s; s=s->link){
		h = hash(s->str, s->n) % hh->nstab;
		s->hash = hh->stab[h];
		hh->stab[h] = s;
	}
}

Stringtab*
findstab(Hash *hh, char *str, int n, int create)
{
	uint h;
	Stringtab *tab, **l;

	if(hh->nstab == 0)
		rehash(hh);

	h = hash(str, n) % hh->nstab;
	for(tab=hh->stab[h], l=&hh->stab[h]; tab; l=&tab->hash, tab=tab->hash)
		if(n==tab->n && memcmp(str, tab->str, n) == 0){
			*l = tab->hash;
			tab->hash = hh->stab[h];
			hh->stab[h] = tab;
			return tab;
		}

	if(!create)
		return nil;

	hh->sorted = 0;
	tab = taballoc();
	tab->str = xstrdup(str, n);
	tab->hash = hh->stab[h];
	tab->link = hh->all;
	hh->all = tab;
	tab->n = n;
	tab->count = 0;
	tab->date = 0;
	hh->stab[h] = tab;

	hh->ntab++;
	if(hh->ntab > 2*hh->nstab && !(hh->ntab&(hh->ntab-1)))
		rehash(hh);
	return tab;
}

int
scmp(Stringtab *a, Stringtab *b)
{
	int n, x;

	if(a == 0)
		return 1;
	if(b == 0)
		return -1;
	n = a->n;
	if(n > b->n)
		n = b->n;
	x = memcmp(a->str, b->str, n);
	if(x != 0)
		return x;
	if(a->n < b->n)
		return -1;
	if(a->n > b->n)
		return 1;
	return 0;	/* shouldn't happen */
}

Stringtab*
merge(Stringtab *a, Stringtab *b)
{
	Stringtab *s, **l;

	l = &s;
	while(a || b){
		if(scmp(a, b) < 0){
			*l = a;
			l = &a->link;
			a = a->link;
		}else{
			*l = b;
			l = &b->link;
			b = b->link;
		}
	}
	*l = 0;
	return s;
}

Stringtab*
mergesort(Stringtab *s)
{
	Stringtab *a, *b;
	int delay;

	if(s == nil)
		return nil;
	if(s->link == nil)
		return s;

	a = b = s;
	delay = 1;
	while(a && b){
		if(delay)	/* easy way to handle 2-element list */
			delay = 0;
		else
			a = a->link;
		if(b = b->link)
			b = b->link;
	}

	b = a->link;
	a->link = nil;

	a = mergesort(s);
	b = mergesort(b);

	return merge(a, b);
}

Stringtab*
sortstab(Hash *hh)
{
	if(!hh->sorted){
		hh->all = mergesort(hh->all);
		hh->sorted = 1;
	}
	return hh->all;
}

int
Bwritehash(Biobuf *b, Hash *hh)
{
	Stringtab *s;
	int now;

	now = time(0);
	s = sortstab(hh);
	Bprint(b, "# hash table\n");
	for(; s; s=s->link){
		if(s->count <= 0)
			continue;
		/*
		 * Entries that haven't been updated in thirty days get tossed.
		 */
		if(s->date+30*86400 < now)
			continue;
		Bwrite(b, s->str, s->n);
		Bprint(b, "\t%d %d\n", s->count, s->date);
	}
	if(Bflush(b) == Beof)
		return -1;
	return 0;
}

void
Breadhash(Biobuf *b, Hash *hh, int scale)
{
	char *s;
	char *t;
	int n;
	int date;
	Stringtab *st;

	s = Brdstr(b, '\n', 1);
	if(s == nil)
		return;
	if(strcmp(s, "# hash table") != 0)
		sysfatal("bad hash table format");

	while(s = Brdline(b, '\n')){
		s[Blinelen(b)-1] = 0;
		t = strrchr(s, '\t');
		if(t == nil)
			sysfatal("bad hash table format");
		*t++ = '\0';
		if(*t < '0' || *t > '9')
			sysfatal("bad hash table format");
		n = strtol(t, &t, 10);
		date = time(0);
		if(*t != 0){
			if(*t == ' '){
				t++;
				date = strtol(t, &t, 10);
			}
			if(*t != 0)
				sysfatal("bad hash table format");
		}
		st = findstab(hh, s, strlen(s), 1);
		if(date > st->date)
			st->date = date;
		st->count += n*scale;
	}
}

void
freehash(Hash *h)
{
	Stringtab *s, *next;

	for(s=h->all; s; s=next){
		next = s->link;
		tabfree(s);
	}
	free(h->stab);
	free(h);
}

Biobuf*
Bopenlock(char *file, int mode)
{
	int i;
	Biobuf *b;
	char err[ERRMAX];

	b = nil;
	for(i=0; i<120; i++){
		if((b = Bopen(file, mode)) != nil)
			break;
		rerrstr(err, sizeof err);
		if(strstr(err, "file is locked")==nil && strstr(err, "exclusive lock")==nil)
			break;
		sleep(1000);
	}
	return b;
}
