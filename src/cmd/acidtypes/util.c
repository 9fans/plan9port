#include <u.h>
#include <libc.h>
#include <bio.h>
#include <mach.h>
#include "dat.h"

static int nwarn;

void
warn(char *fmt, ...)
{
	va_list arg;

	if(++nwarn < 5){
		va_start(arg, fmt);
		fprint(2, "warning: ");
		vfprint(2, fmt, arg);
		fprint(2, "\n");
		va_end(arg);
	}else if(nwarn == 5)
		fprint(2, "[additional warnings elided...]\n");
}

void*
erealloc(void *v, uint n)
{
	v = realloc(v, n);
	if(v == nil)
		sysfatal("realloc: %r");
	return v;
}

void*
emalloc(uint n)
{
	void *v;

	v = mallocz(n, 1);
	if(v == nil)
		sysfatal("malloc: %r");
	return v;
}

char*
estrdup(char *s)
{
	s = strdup(s);
	if(s == nil)
		sysfatal("strdup: %r");
	return s;
}

TypeList*
mktl(Type *hd, TypeList *tail)
{
	TypeList *tl;

	tl = emalloc(sizeof(*tl));
	tl->hd = hd;
	tl->tl = tail;
	return tl;
}

