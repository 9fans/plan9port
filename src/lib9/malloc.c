/*
 * These are here mainly so that I can link against
 * debugmalloc.c instead and not recompile the world.
 */

#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

static Lock malloclock;

void*
p9malloc(ulong n)
{
	void *v;
	
	if(n == 0)
		n++;
	lock(&malloclock);
	v = malloc(n);
	unlock(&malloclock);
	return v;
}

void
p9free(void *v)
{
	if(v == nil)
		return;
	lock(&malloclock);
	free(v);
	unlock(&malloclock);
}

void*
p9calloc(ulong a, ulong b)
{
	void *v;
	
	if(a*b == 0)
		a = b = 1;

	lock(&malloclock);
	v = calloc(a*b, 1);
	unlock(&malloclock);
	return v;
}

void*
p9realloc(void *v, ulong n)
{
	lock(&malloclock);
	v = realloc(v, n);
	unlock(&malloclock);
	return v;
}
