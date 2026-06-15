#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>
#include <plumb.h>
#include <9pclient.h>
#include "dat.h"

void*
emalloc(uint n)
{
	void *p;

	p = malloc(n);
	if(p == nil)
		error("can't malloc: %r");
	memset(p, 0, n);
	setmalloctag(p, getcallerpc(&n));
	return p;
}

void*
erealloc(void *p, uint n)
{
	p = realloc(p, n);
	if(p == nil)
		error("can't realloc: %r");
	setmalloctag(p, getcallerpc(&n));
	return p;
}

char*
estrdup(char *s)
{
	char *t;

	t = emalloc(strlen(s)+1);
	strcpy(t, s);
	return t;
}

char*
estrstrdup(char *s, char *t)
{
	char *u;

	u = emalloc(strlen(s)+strlen(t)+1);
	strcpy(u, s);
	strcat(u, t);
	return u;
}

char*
eappend(char *s, char *sep, char *t)
{
	char *u;

	if(t == nil)
		u = estrstrdup(s, sep);
	else{
		u = emalloc(strlen(s)+strlen(sep)+strlen(t)+1);
		strcpy(u, s);
		strcat(u, sep);
		strcat(u, t);
	}
	free(s);
	return u;
}

char*
egrow(char *s, char *sep, char *t)
{
	s = eappend(s, sep, t);
	free(t);
	return s;
}

void
error(char *fmt, ...)
{
	Fmt f;
	char buf[64];
	va_list arg;

	fmtfdinit(&f, 2, buf, sizeof buf);
	fmtprint(&f, "Mail: ");
	va_start(arg, fmt);
	fmtvprint(&f, fmt, arg);
	va_end(arg);
	fmtprint(&f, "\n");
	fmtfdflush(&f);
	threadexitsall(fmt);
}

void
ctlprint(CFid *fd, char *fmt, ...)
{
	int n;
	va_list arg;

	va_start(arg, fmt);
	n = fsvprint(fd, fmt, arg);
	va_end(arg);
	if(n <= 0)
		error("control file write error: %r");
}
