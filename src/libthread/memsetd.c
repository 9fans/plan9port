#include "threadimpl.h"
#include <string.h>

void
_threaddebugmemset(void *v, int c, int n)
{
	memset(v, c, n);
}
