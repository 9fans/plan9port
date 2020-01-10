#include <u.h>
#include <libc.h>
#include <thread.h>
#include <sunrpc.h>

void*
emalloc(ulong n)
{
	void *v;

	v = mallocz(n, 1);
	if(v == nil)
{
abort();
		sysfatal("out of memory");
}
	setmalloctag(v, getcallerpc(&n));
	return v;
}

void*
erealloc(void *v, ulong n)
{
	v = realloc(v, n);
	if(v == nil)
{
abort();
		sysfatal("out of memory");
}
	setrealloctag(v, getcallerpc(&n));
	return v;
}
