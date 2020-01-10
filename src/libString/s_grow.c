#include <u.h>
#include <libc.h>
#include "libString.h"

/* grow a String's allocation by at least `incr' bytes */
extern String*
s_grow(String *s, int incr)
{
	char *cp;
	int size;

	if(s->fixed)
		sysfatal("s_grow of constant string");
	s = s_unique(s);

	/*
	 *  take a larger increment to avoid mallocing too often
	 */
	size = s->end-s->base;
	if(size/2 < incr)
		size += incr;
	else
		size += size/2;

	cp = realloc(s->base, size);
	if (cp == 0)
		sysfatal("s_grow: %r");
	s->ptr = (s->ptr - s->base) + cp;
	s->end = cp + size;
	s->base = cp;

	return s;
}
