#include "threadimpl.h"

static long
_iodial(va_list *arg)
{
	char *addr, *local, *dir;
	int *cdfp, fd;

	addr = va_arg(*arg, char*);
	local = va_arg(*arg, char*);
	dir = va_arg(*arg, char*);
	cdfp = va_arg(*arg, int*);

fprint(2, "before dial\n");
	fd = dial(addr, local, dir, cdfp);
fprint(2, "after dial\n");
	return fd;
}

int
iodial(Ioproc *io, char *addr, char *local, char *dir, int *cdfp)
{
	return iocall(io, _iodial, addr, local, dir, cdfp);
}
