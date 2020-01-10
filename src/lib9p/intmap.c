#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

enum {
	NHASH = 128
};

typedef struct Intlist	Intlist;
struct Intlist
{
	ulong	id;
	void*	aux;
	Intlist*	link;
};

struct Intmap
{
	RWLock	rwlock;
	Intlist*	hash[NHASH];
	void (*inc)(void*);
};

static ulong
hashid(ulong id)
{
	return id%NHASH;
}

static void
nop(void *v)
{
	USED(v);
}

Intmap*
allocmap(void (*inc)(void*))
{
	Intmap *m;

	m = emalloc9p(sizeof(*m));
	if(inc == nil)
		inc = nop;
	m->inc = inc;
	return m;
}

void
freemap(Intmap *map, void (*destroy)(void*))
{
	int i;
	Intlist *p, *nlink;

	if(destroy == nil)
		destroy = nop;
	for(i=0; i<NHASH; i++){
		for(p=map->hash[i]; p; p=nlink){
			nlink = p->link;
			destroy(p->aux);
			free(p);
		}
	}

	free(map);
}

static Intlist**
llookup(Intmap *map, ulong id)
{
	Intlist **lf;

	for(lf=&map->hash[hashid(id)]; *lf; lf=&(*lf)->link)
		if((*lf)->id == id)
			break;
	return lf;
}

/*
 * The RWlock is used as expected except that we allow
 * inc() to be called while holding it.  This is because we're
 * locking changes to the tree structure, not to the references.
 * Inc() is expected to have its own locking.
 */
void*
lookupkey(Intmap *map, ulong id)
{
	Intlist *f;
	void *v;

	rlock(&map->rwlock);
	if(f = *llookup(map, id)){
		v = f->aux;
		map->inc(v);
	}else
		v = nil;
	runlock(&map->rwlock);
	return v;
}

void*
insertkey(Intmap *map, ulong id, void *v)
{
	Intlist *f;
	void *ov;
	ulong h;

	wlock(&map->rwlock);
	if(f = *llookup(map, id)){
		/* no decrement for ov because we're returning it */
		ov = f->aux;
		f->aux = v;
	}else{
		f = emalloc9p(sizeof(*f));
		f->id = id;
		f->aux = v;
		h = hashid(id);
		f->link = map->hash[h];
		map->hash[h] = f;
		ov = nil;
	}
	wunlock(&map->rwlock);
	return ov;
}

int
caninsertkey(Intmap *map, ulong id, void *v)
{
	Intlist *f;
	int rv;
	ulong h;

	wlock(&map->rwlock);
	if(*llookup(map, id))
		rv = 0;
	else{
		f = emalloc9p(sizeof *f);
		f->id = id;
		f->aux = v;
		h = hashid(id);
		f->link = map->hash[h];
		map->hash[h] = f;
		rv = 1;
	}
	wunlock(&map->rwlock);
	return rv;
}

void*
deletekey(Intmap *map, ulong id)
{
	Intlist **lf, *f;
	void *ov;

	wlock(&map->rwlock);
	if(f = *(lf = llookup(map, id))){
		ov = f->aux;
		*lf = f->link;
		free(f);
	}else
		ov = nil;
	wunlock(&map->rwlock);
	return ov;
}
