#include "threadimpl.h"

static long
_iosleep(va_list *arg)
{
	long n;

	n = va_arg(*arg, long);
	return sleep(n);
}

int
iosleep(Ioproc *io, long n)
{
	return iocall(io, _iosleep, n);
}
