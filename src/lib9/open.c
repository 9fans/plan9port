#define _GNU_SOURCE	/* for Linux O_DIRECT */
#include <u.h>
#define NOPLAN9DEFINES
#include <sys/file.h>
#include <libc.h>
#ifndef O_DIRECT
#define O_DIRECT 0
#endif

int
p9open(char *name, int mode)
{
	int cexec, rclose;
	int fd, umode, lock, rdwr;
	struct flock fl;

	rdwr = mode&3;
	umode = rdwr;
	cexec = mode&OCEXEC;
	rclose = mode&ORCLOSE;
	lock = mode&OLOCK;
	mode &= ~(3|OCEXEC|ORCLOSE|OLOCK);
	if(mode&OTRUNC){
		umode |= O_TRUNC;
		mode ^= OTRUNC;
	}
	if(mode&ODIRECT){
		umode |= O_DIRECT;
		mode ^= ODIRECT;
	}
	if(mode&ONONBLOCK){
		umode |= O_NONBLOCK;
		mode ^= ONONBLOCK;
	}
	if(mode&OAPPEND){
		umode |= O_APPEND;
		mode ^= OAPPEND;
	}
	if(mode){
		werrstr("mode 0x%x not supported", mode);
		return -1;
	}
	fd = open(name, umode);
	if(fd >= 0){
		if(lock){
			fl.l_type = (rdwr==OREAD) ? F_RDLCK : F_WRLCK;
			fl.l_whence = SEEK_SET;
			fl.l_start = 0;
			fl.l_len = 0;
			if(fcntl(fd, F_SETLK, &fl) < 0){
				close(fd);
				werrstr("lock: %r");
				return -1;
			}
		}
		if(cexec)
			fcntl(fd, F_SETFL, FD_CLOEXEC);
		if(rclose)
			remove(name);
	}
	return fd;
}
