#include <u.h>
#include <libc.h>

void *
emalloc(ulong n)
{
	void *p = malloc(n);
	if(p == nil)
		sysfatal("emalloc");
	memset(p, 0, n);
	return p;
}

void *
erealloc(void *p, ulong n)
{
	if ((p = realloc(p, n)) == nil)
		sysfatal("erealloc");
	return p;
}

char *
estrdup(char *s)
{
	if ((s = strdup(s)) == nil)
		sysfatal("estrdup");
	return s;
}
