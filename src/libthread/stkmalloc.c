#include "threadimpl.h"

void*
_threadstkalloc(int n)
{
	return malloc(n);
}

void
_threadstkfree(void *v, int n)
{
	free(v);
}
