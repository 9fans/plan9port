#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include <ndb.h>
#include "ndbhf.h"

/*
 *  free a parsed entry
 */
void
ndbfree(Ndbtuple *t)
{
	Ndbtuple *tn;

	for(; t; t = tn){
		tn = t->entry;
		if(t->val != t->valbuf){
			free(t->val);
		}
		free(t);
	}
}

/*
 *  set a value in a tuple
 */
void
ndbsetval(Ndbtuple *t, char *val, int n)
{
	if(n < Ndbvlen){
		if(t->val != t->valbuf){
			free(t->val);
			t->val = t->valbuf;
		}
	} else {
		if(t->val != t->valbuf)
			t->val = realloc(t->val, n+1);
		else
			t->val = malloc(n+1);
		if(t->val == nil)
			sysfatal("ndbsetval %r");
	}
	strncpy(t->val, val, n);
	t->val[n] = 0;
}

/*
 *  allocate a tuple
 */
Ndbtuple*
ndbnew(char *attr, char *val)
{
	Ndbtuple *t;

	t = mallocz(sizeof(*t), 1);
	if(t == nil)
		sysfatal("ndbnew %r");
	if(attr != nil)
		strncpy(t->attr, attr, sizeof(t->attr)-1);
	t->val = t->valbuf;
	if(val != nil)
		ndbsetval(t, val, strlen(val));
	return t;	
}
