#include <u.h>
#include <libc.h>
#include "libString.h"

#define STRLEN 128

extern void
s_free(String *sp)
{
	if (sp == nil)
		return;
	lock(&sp->lk);
	if(--(sp->ref) != 0){
		unlock(&sp->lk);
		return;
	}
	unlock(&sp->lk);

	if(sp->fixed == 0 && sp->base != nil)
		free(sp->base);
	free(sp);
}

/* get another reference to a string */
extern String *
s_incref(String *sp)
{
	lock(&sp->lk);
	sp->ref++;
	unlock(&sp->lk);

	return sp;
}

/* allocate a String head */
extern String *
_s_alloc(void)
{
	String *s;

	s = mallocz(sizeof *s, 1);
	if(s == nil)
		return s;
	s->ref = 1;
	s->fixed = 0;
	return s;
}

/* create a new `short' String */
extern String *
s_newalloc(int len)
{
	String *sp;

	sp = _s_alloc();
	if(sp == nil)
		sysfatal("s_newalloc: %r");
	setmalloctag(sp, getcallerpc(&len));
	if(len < STRLEN)
		len = STRLEN;
	sp->base = sp->ptr = malloc(len);
	if (sp->base == nil)
		sysfatal("s_newalloc: %r");
	setmalloctag(sp->base, getcallerpc(&len));

	sp->end = sp->base + len;
	s_terminate(sp);
	return sp;
}

/* create a new `short' String */
extern String *
s_new(void)
{
	String *sp;

	sp = _s_alloc();
	if(sp == nil)
		sysfatal("s_new: %r");
	sp->base = sp->ptr = malloc(STRLEN);
	if (sp->base == nil)
		sysfatal("s_new: %r");
	sp->end = sp->base + STRLEN;
	s_terminate(sp);
	return sp;
}
