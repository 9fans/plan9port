#include <u.h>
#include <libc.h>

vlong
seek(int fd, vlong offset, int whence)
{
	return lseek(fd, offset, whence);
}
