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
