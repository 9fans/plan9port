#include <sys/stat.h>

#include <u.h>
#include <libc.h>
#include <draw.h>

vlong
flength(int fd)
{
	struct stat s;

	if(fstat(fd, &s) < 0)
		return -1;
	return s.st_size;
}

