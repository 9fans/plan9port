#include "threadimpl.h"
#include <string.h>

void
_threadmemset(void *v, int c, int n)
{
	memset(v, c, n);
}
