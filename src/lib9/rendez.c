#include <u.h>
#include <libc.h>
#include "9proc.h"

static Lock rendlock;
static Uproc *rendhash[RENDHASH];

ulong
rendezvous(ulong tag, ulong val)
{
	char c;
	ulong ret;
	Uproc *t, *self, **l;

	self = _p9uproc();
	lock(&rendlock);
	l = &rendhash[tag%RENDHASH];
	for(t=*l; t; l=&t->rendhash, t=*l){
		if(t->rendtag==tag){
			*l = t->rendhash;
			ret = t->rendval;
			t->rendval = val;
			t->rendtag++;
			c = 0;
			unlock(&rendlock);
			write(t->pipe[1], &c, 1);
			return ret;
		}
	}

	/* Going to sleep here. */
	t = self;
	t->rendtag = tag;
	t->rendval = val;
	t->rendhash = *l;
	*l = t;
	unlock(&rendlock);
	do
		read(t->pipe[0], &c, 1);
	while(t->rendtag == tag);
	return t->rendval;
}
