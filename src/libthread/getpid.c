#include "threadimpl.h"
#include <unistd.h>

int
_threadgetpid(void)
{
	return getpid();
}
