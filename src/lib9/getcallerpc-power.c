#include <lib9.h>

ulong
getcallerpc(void *x)
{
	ulong *lp;

	lp = x;

	return lp[-1];
}
