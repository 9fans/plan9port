#include "threadimpl.h"

static long
_ioclose(va_list *arg)
{
	int fd;

	fd = va_arg(*arg, int);
	return close(fd);
}

int
ioclose(Ioproc *io, int fd)
{
	return iocall(io, _ioclose, fd);
}
