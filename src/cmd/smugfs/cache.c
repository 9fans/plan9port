#include "a.h"

struct Cache
{
	CEntry **hash;
	int nhash;
	CEntry *head;
	CEntry *tail;
	int nentry;
	int maxentry;
	int sizeofentry;
	void (*cefree)(CEntry*);
};

static void
nop(CEntry *ce)
{
}

static uint
hash(const char *s)
{
	uint h;
	uchar *p;

	h = 0;
	for(p=(uchar*)s; *p; p++)
		h = h*37 + *p;
	return h;
}

Cache*
newcache(int sizeofentry, int maxentry, void (*cefree)(CEntry*))
{
	Cache *c;
	int i;

	assert(sizeofentry >= sizeof(CEntry));
	c = emalloc(sizeof *c);
	c->sizeofentry = sizeofentry;
	c->maxentry = maxentry;
	c->nentry = 0;
	for(i=1; i<maxentry; i<<=1)
		;
	c->nhash = i;
	c->hash = emalloc(c->nhash * sizeof c->hash[0]);
	if(cefree == nil)
		cefree = nop;
	c->cefree = cefree;
	return c;
}

static void
popout(Cache *c, CEntry *e)
{
	if(e->list.prev)
		e->list.prev->list.next = e->list.next;
	else
		c->head = e->list.next;
	if(e->list.next)
		e->list.next->list.prev = e->list.prev;
	else
		c->tail = e->list.prev;
}

static void
insertfront(Cache *c, CEntry *e)
{
	e->list.next = c->head;
	c->head = e;
	if(e->list.next)
		e->list.next->list.prev = e;
	else
		c->tail = e;
}

static void
movetofront(Cache *c, CEntry *e)
{
	popout(c, e);
	insertfront(c, e);
}

static CEntry*
evict(Cache *c)
{
	CEntry *e;

	e = c->tail;
	popout(c, e);
	c->cefree(e);
	free(e->name);
	e->name = nil;
	memset(e, 0, c->sizeofentry);
	insertfront(c, e);
	return e;
}

CEntry*
cachelookup(Cache *c, char *name, int create)
{
	int h;
	CEntry *e;

	h = hash(name) % c->nhash;
	for(e=c->hash[h]; e; e=e->hash.next){
		if(strcmp(name, e->name) == 0){
			movetofront(c, e);
			return e;
		}
	}

	if(!create)
		return nil;

	if(c->nentry >= c->maxentry)
		e = evict(c);
	else{
		e = emalloc(c->sizeofentry);
		insertfront(c, e);
		c->nentry++;
	}
	e->name = estrdup(name);
	h = hash(name) % c->nhash;
	e->hash.next = c->hash[h];
	c->hash[h] = e;
	return e;
}

void
cacheflush(Cache *c, char *substr)
{
	CEntry **l, *e;
	int i;

	for(i=0; i<c->nhash; i++){
		for(l=&c->hash[i]; (e=*l); ){
			if(substr == nil || strstr(e->name, substr)){
				*l = e->hash.next;
				c->nentry--;
				popout(c, e);
				c->cefree(e);
				free(e->name);
				free(e);
			}else
				l = &e->hash.next;
		}
	}
}
