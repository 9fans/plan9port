#include "threadimpl.h"

static long
_iowrite(va_list *arg)
{
	int fd;
	void *a;
	long n;

	fd = va_arg(*arg, int);
	a = va_arg(*arg, void*);
	n = va_arg(*arg, long);
	n = write(fd, a, n);
	return n;
}

long
iowrite(Ioproc *io, int fd, void *a, long n)
{
	return iocall(io, _iowrite, fd, a, n);
}
