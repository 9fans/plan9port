#include	<u.h>
#include	<libc.h>

long
lrand(void)
{
	return ((rand()<<16)^rand()) & 0x7FFFFFFF;
}
