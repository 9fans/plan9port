#include <u.h>
#include <unistd.h>
#include <string.h>
#include <libc.h>

void*
mallocz(unsigned long n, int clr)
{
	void *v;

	v = malloc(n);
	if(clr && v)
		memset(v, 0, n);
	return v;
}
