#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

int
p9open(char *name, int mode)
{
	int cexec, rclose;
	int fd, umode;

	umode = mode&3;
	cexec = mode&OCEXEC;
	rclose = mode&ORCLOSE;
	mode &= ~(3|OCEXEC|ORCLOSE);
	if(mode&OTRUNC){
		umode |= O_TRUNC;
		mode ^= OTRUNC;
	}
	if(mode){
		werrstr("mode not supported");
		return -1;
	}
	fd = open(name, umode);
	if(fd >= 0){
		if(cexec)
			fcntl(fd, F_SETFL, FD_CLOEXEC);
		if(rclose)
			remove(name);
	}
	return fd;
}
