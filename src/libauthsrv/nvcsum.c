#include <u.h>
#include <libc.h>
#include <auth.h>

uchar
nvcsum(void *vmem, int n)
{
	uchar *mem, sum;
	int i;

	sum = 9;
	mem = vmem;
	for(i = 0; i < n; i++)
		sum += mem[i];
	return sum;
}
