#define _GNU_SOURCE	/* for Linux O_DIRECT */
#include <u.h>
#define NOPLAN9DEFINES
#include <sys/file.h>
#include <libc.h>
#include <sys/stat.h>
#ifndef O_DIRECT
#define O_DIRECT 0
#endif

int
p9create(char *path, int mode, ulong perm)
{
	int fd, cexec, umode, rclose, lock, rdwr;

	rdwr = mode&3;
	lock = mode&OLOCK;
	cexec = mode&OCEXEC;
	rclose = mode&ORCLOSE;
	mode &= ~(ORCLOSE|OCEXEC|OLOCK);

	/* XXX should get mode mask right? */
	fd = -1;
	if(perm&DMDIR){
		if(mode != OREAD){
			werrstr("bad mode in directory create");
			goto out;
		}
		if(mkdir(path, perm&0777) < 0)
			goto out;
		fd = open(path, O_RDONLY);
	}else{
		umode = (mode&3)|O_CREAT|O_TRUNC;
		mode &= ~(3|OTRUNC);
		if(mode&ODIRECT){
			umode |= O_DIRECT;
			mode &= ~ODIRECT;
		}
		if(mode&OEXCL){
			umode |= O_EXCL;
			mode &= ~OEXCL;
		}
		if(mode){
			werrstr("unsupported mode in create");
			goto out;
		}
		fd = open(path, umode, perm);
	}
out:
	if(fd >= 0){
		if(lock){
			if(flock(fd, (rdwr==OREAD) ? LOCK_SH : LOCK_EX) < 0){
				close(fd);
				return -1;
			}
		}
		if(cexec)
			fcntl(fd, F_SETFL, FD_CLOEXEC);
		if(rclose)
			remove(path);
	}
	return fd;
}
