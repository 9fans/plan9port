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
	void *v;

	if(n == 0)
		n++;
	v = malloc(n);
	print("p9malloc %lud => %p; pc %lux\n", n, v, getcallerpc(&n));
	return v;
}

void
p9free(void *v)
{
	if(v == nil)
		return;
	print("p9free %p; pc %lux\n", v, getcallerpc(&v));
	free(v);
}

void*
p9calloc(ulong a, ulong b)
{
	void *v;

	if(a*b == 0)
		a = b = 1;

	v = calloc(a*b, 1);
	print("p9calloc %lud %lud => %p; pc %lux\n", a, b, v, getcallerpc(&a));
	return v;
}

void*
p9realloc(void *v, ulong n)
{
	void *vv;

	vv = realloc(v, n);
	print("p9realloc %p %lud => %p; pc %lux\n", v, n, vv, getcallerpc(&v));
	return vv;
}
