#include "threadimpl.h"

static long
_ioreadn(va_list *arg)
{
	int fd;
	void *a;
	long n;

	fd = va_arg(*arg, int);
	a = va_arg(*arg, void*);
	n = va_arg(*arg, long);
	n = readn(fd, a, n);
	return n;
}

long
ioreadn(Ioproc *io, int fd, void *a, long n)
{
	return iocall(io, _ioreadn, fd, a, n);
}
