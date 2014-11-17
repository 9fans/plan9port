#include <lib9.h>

ulong
getcallerpc(void *x)
{
	return ((ulong*)x)[-2];
}

