#include <u.h>
#include <sys/mman.h>
#include "threadimpl.h"

#ifndef MAP_STACK
#define MAP_STACK 0
#endif

void*
_threadstkalloc(int n)
{
	void *p;

	p = mmap(nil, n, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON|MAP_STACK, -1, 0);
	if(p == (void*)-1)
		return nil;
	return p;
}

void
_threadstkfree(void *v, int n)
{
	if(n > 0)
		munmap(v, n);
}
