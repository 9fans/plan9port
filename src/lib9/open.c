#define _GNU_SOURCE	/* for Linux O_DIRECT */
#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>
#ifndef O_DIRECT
#define O_DIRECT 0
#endif

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
	if(mode&ODIRECT){
		umode |= O_DIRECT;
		mode ^= ODIRECT;
	}
	if(mode){
		werrstr("mode 0x%x not supported", mode);
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
