#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

extern char* _p9translate(char*);

int
p9open(char *xname, int mode)
{
	char *name;
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
	if((name = _p9translate(xname)) == nil)
		return -1;
	fd = open(name, umode);
	if(fd >= 0){
		if(cexec)
			fcntl(fd, F_SETFL, FD_CLOEXEC);
		if(rclose)
			remove(name);
	}
	if(name != xname)
		free(name);
	return fd;
}
