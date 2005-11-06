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
	print("p9malloc %lud => %p; pc %lux\n", n, v, getcallerpc(&n));
	return v;
}

void
p9free(void *v)
{
	if(v == nil)
		return;
	lock(&malloclock);
	print("p9free %p; pc %lux\n", v, getcallerpc(&v));
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
	print("p9calloc %lud %lud => %p; pc %lux\n", a, b, v, getcallerpc(&a));
	return v;
}

void*
p9realloc(void *v, ulong n)
{
	void *vv;
	
	lock(&malloclock);
	vv = realloc(v, n);
	unlock(&malloclock);
	print("p9realloc %p %lud => %p; pc %lux\n", v, n, vv, getcallerpc(&v));
	return vv;
}

