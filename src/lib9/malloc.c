/*
 * These are here mainly so that I can link against
 * debugmalloc.c instead and not recompile the world.
 */

#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>


void*
p9malloc(ulong n)
{
	if(n == 0)
		n++;
	return malloc(n);
}

void
p9free(void *v)
{
	free(v);
}

void*
p9calloc(ulong a, ulong b)
{
	if(a*b == 0)
		a = b = 1;
	return calloc(a, b);
}

void*
p9realloc(void *v, ulong n)
{
	return realloc(v, n);
}
