#include "threadimpl.h"

static long totalmalloc;

void*
_threadmalloc(long size, int z)
{
	void *m;

	m = malloc(size);
	if (m == nil)
		sysfatal("Malloc of size %ld failed: %r\n", size);
	setmalloctag(m, getcallerpc(&size));
	totalmalloc += size;
	if (size > 1000000) {
		fprint(2, "Malloc of size %ld, total %ld\n", size, totalmalloc);
		abort();
	}
	if (z)
		_threadmemset(m, 0, size);
	return m;
}

void
_threadsysfatal(char *fmt, va_list arg)
{
	char buf[1024];	/* size doesn't matter; we're about to exit */

	vseprint(buf, buf+sizeof(buf), fmt, arg);
	if(argv0)
		fprint(2, "%s: %s\n", argv0, buf);
	else
		fprint(2, "%s\n", buf);
	threadexitsall(buf);
}
