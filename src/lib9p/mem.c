#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include "9p.h"

void*
emalloc9p(ulong sz)
{
	void *v;

	if((v = malloc(sz)) == nil)
		sysfatal("out of memory allocating %lud", sz);
	memset(v, 0, sz);
	setmalloctag(v, getcallerpc(&sz));
	return v;
}

void*
erealloc9p(void *v, ulong sz)
{
	void *nv;

	if((nv = realloc(v, sz)) == nil)
		sysfatal("out of memory reallocating %lud", sz);
	if(v == nil)
		setmalloctag(nv, getcallerpc(&v));
	setrealloctag(nv, getcallerpc(&v));
	return nv;
}

char*
estrdup9p(char *s)
{
	char *t;

	if((t = strdup(s)) == nil)
		sysfatal("out of memory in strdup(%.20s)", s);
	setmalloctag(t, getcallerpc(&s));
	return t;
}
