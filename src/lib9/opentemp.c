#include <u.h>
#include <libc.h>

int
opentemp(char *template, int mode)
{
	int fd, fd1;

	fd = mkstemp(template);
	if(fd < 0)
		return -1;
	if((fd1 = open(template, mode)) < 0){
		remove(template);
		close(fd);
		return -1;
	}
	close(fd);
	return fd1;
}

