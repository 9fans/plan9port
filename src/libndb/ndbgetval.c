#include <u.h>
#include <libc.h>
#include <bio.h>
#include "ndb.h"

/*
 *  search for a tuple that has the given 'attr=val' and also 'rattr=x'.
 *  copy 'x' into 'buf' and return the whole tuple.
 *
 *  return 0 if not found.
 */
char*
ndbgetvalue(Ndb *db, Ndbs *s, char *attr, char *val, char *rattr, Ndbtuple **pp)
{
	Ndbtuple *t, *nt;
	char *rv;
	Ndbs temps;

	if(s == nil)
		s = &temps;
	if(pp)
		*pp = nil;
	t = ndbsearch(db, s, attr, val);
	while(t){
		/* first look on same line (closer binding) */
		nt = s->t;
		for(;;){
			if(strcmp(rattr, nt->attr) == 0){
				rv = strdup(nt->val);
				if(pp != nil)
					*pp = t;
				else
					ndbfree(t);
				return rv;
			}
			nt = nt->line;
			if(nt == s->t)
				break;
		}
		/* search whole tuple */
		for(nt = t; nt; nt = nt->entry){
			if(strcmp(rattr, nt->attr) == 0){
				rv = strdup(nt->val);
				if(pp != nil)
					*pp = t;
				else
					ndbfree(t);
				return rv;
			}
		}
		ndbfree(t);
		t = ndbsnext(s, attr, val);
	}
	return nil;
}

Ndbtuple*
ndbgetval(Ndb *db, Ndbs *s, char *attr, char *val, char *rattr, char *buf)
{
	Ndbtuple *t;
	char *p;

	p = ndbgetvalue(db, s, attr, val, rattr, &t);
	if(p == nil){
		if(buf != nil)
			*buf = 0;
	} else {
		if(buf != nil){
			strncpy(buf, p, Ndbvlen-1);
			buf[Ndbvlen-1] = 0;
		}
		free(p);
	}
	return t;
}
