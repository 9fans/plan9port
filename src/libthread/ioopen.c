#include <unistd.h>
#include <fcntl.h>
#include "threadimpl.h"

static long
_ioopen(va_list *arg)
{
	char *path;
	int mode;

	path = va_arg(*arg, char*);
	mode = va_arg(*arg, int);
	return open(path, mode);
}

int
ioopen(Ioproc *io, char *path, int mode)
{
	return iocall(io, _ioopen, path, mode);
}
