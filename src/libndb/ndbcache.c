#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ndb.h>

struct Ndbcache
{
	Ndbcache	*next;
	char		*attr;
	char		*val;
	Ndbs		s;
	Ndbtuple	*t;
};

enum
{
	Maxcached=	128
};

static void
ndbcachefree(Ndbcache *c)
{
	free(c->val);
	free(c->attr);
	if(c->t)
		ndbfree(c->t);
	free(c);
}

static Ndbtuple*
ndbcopy(Ndb *db, Ndbtuple *from_t, Ndbs *from_s, Ndbs *to_s)
{
	Ndbtuple *first, *to_t, *last, *line;
	int newline;

	*to_s = *from_s;
	to_s->t = nil;
	to_s->db = db;

	newline = 1;
	last = nil;
	first = nil;
	line = nil;
	for(; from_t != nil; from_t = from_t->entry){
		to_t = ndbnew(from_t->attr, from_t->val);

		/* have s point to matching tuple */
		if(from_s->t == from_t)
			to_s->t = to_t;

		if(newline)
			line = to_t;
		else
			last->line = to_t;

		if(last != nil)
			last->entry = to_t;
		else {
			first = to_t;
			line = to_t;
		}
		to_t->entry = nil;
		to_t->line = line;
		last = to_t;
		newline = from_t->line != from_t->entry;
	}
	return first;
}

/*
 *  if found, move to front
 */
int
_ndbcachesearch(Ndb *db, Ndbs *s, char *attr, char *val, Ndbtuple **t)
{
	Ndbcache *c, **l;

	*t = nil;
	c = nil;
	for(l = &db->cache; *l != nil; l = &(*l)->next){
		c = *l;
		if(strcmp(c->attr, attr) == 0 && strcmp(c->val, val) == 0)
			break;
	}
	if(*l == nil)
		return -1;

	/* move to front */
	*l = c->next;
	c->next = db->cache;
	db->cache = c;

	*t = ndbcopy(db, c->t, &c->s, s);
	return 0;
}

Ndbtuple*
_ndbcacheadd(Ndb *db, Ndbs *s, char *attr, char *val, Ndbtuple *t)
{
	Ndbcache *c, **l;

	c = mallocz(sizeof *c, 1);
	if(c == nil)
		return nil;
	c->attr = strdup(attr);
	if(c->attr == nil)
		goto err;
	c->val = strdup(val);
	if(c->val == nil)
		goto err;
	c->t = ndbcopy(db, t, s, &c->s);
	if(c->t == nil && t != nil)
		goto err;

	/* add to front */
	c->next = db->cache;
	db->cache = c;

	/* trim list */
	if(db->ncache < Maxcached){
		db->ncache++;
		return t;
	}
	for(l = &db->cache; (*l)->next; l = &(*l)->next)
		;
	c = *l;
	*l = nil;
err:
	ndbcachefree(c);
	return t;
}

void
_ndbcacheflush(Ndb *db)
{
	Ndbcache *c;

	while(db->cache != nil){
		c = db->cache;
		db->cache = c->next;
		ndbcachefree(c);
	}
	db->ncache = 0;
}
