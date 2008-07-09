#include <u.h>
#include <libc.h>

int
opentemp(char *template)
{
	int fd;

	fd = mkstemp(template);
	if(fd < 0)
		return -1;
	remove(template);
	return fd;
}

