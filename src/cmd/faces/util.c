#include <u.h>
#include <libc.h>
#include <draw.h>
#include <plumb.h>
#include <9pclient.h>
#include "faces.h"

void*
emalloc(ulong sz)
{
	void *v;
	v = malloc(sz);
	if(v == nil) {
		fprint(2, "out of memory allocating %ld\n", sz);
		exits("mem");
	}
	memset(v, 0, sz);
	return v;
}

void*
erealloc(void *v, ulong sz)
{
	v = realloc(v, sz);
	if(v == nil) {
		fprint(2, "out of memory allocating %ld\n", sz);
		exits("mem");
	}
	return v;
}

char*
estrdup(char *s)
{
	char *t;
	if((t = strdup(s)) == nil) {
		fprint(2, "out of memory in strdup(%.10s)\n", s);
		exits("mem");
	}
	return t;
}
