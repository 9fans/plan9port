#include "threadimpl.h"

static long
_iowrite(va_list *arg)
{
	int fd;
	void *a;
	long n, nn;

	fd = va_arg(*arg, int);
	a = va_arg(*arg, void*);
	n = va_arg(*arg, long);
	nn = write(fd, a, n);
fprint(2, "_iowrite %d %d %r\n", n, nn);
	return nn;
}

long
iowrite(Ioproc *io, int fd, void *a, long n)
{
	return iocall(io, _iowrite, fd, a, n);
}
