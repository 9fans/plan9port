#include <u.h>
#include <libc.h>
#include <diskfs.h>

void*
emalloc(ulong n)
{
	void *v;

	v = mallocz(n, 1);
	if(v == nil)
		abort();
	return v;
}

void*
erealloc(void *v, ulong n)
{
	v = realloc(v, n);
	if(v == nil)
		abort();
	return v;
}
