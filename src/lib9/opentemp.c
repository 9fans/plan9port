#include <u.h>
#include <libc.h>

int
opentemp(char *template, int mode)
{
	int fd, fd1;

	fd = mkstemp(template);
	if(fd < 0)
		return -1;
	/* reopen for mode */
	fd1 = open(template, mode);
	if(fd1 < 0){
		close(fd);
		remove(template);
		return -1;
	}
	close(fd);
	return fd1;
}
