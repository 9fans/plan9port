#include "threadimpl.h"

static long
_ioread(va_list *arg)
{
	int fd;
	void *a;
	long n;

	fd = va_arg(*arg, int);
	a = va_arg(*arg, void*);
	n = va_arg(*arg, long);
	return read(fd, a, n);
}

long
ioread(Ioproc *io, int fd, void *a, long n)
{
	return iocall(io, _ioread, fd, a, n);
}
