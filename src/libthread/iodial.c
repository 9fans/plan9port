#include "threadimpl.h"

static long
_iodial(va_list *arg)
{
	char *addr, *local, *dir;
	int *cdfp;

	addr = va_arg(*arg, char*);
	local = va_arg(*arg, char*);
	dir = va_arg(*arg, char*);
	cdfp = va_arg(*arg, int*);

	return dial(addr, local, dir, cdfp);
}

int
iodial(Ioproc *io, char *addr, char *local, char *dir, int *cdfp)
{
	return iocall(io, _iodial, addr, local, dir, cdfp);
}
