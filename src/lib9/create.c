#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>
#include <sys/stat.h>

extern char *_p9translate(char*);

int
p9create(char *xpath, int mode, ulong perm)
{
	int fd, cexec, umode, rclose;
	char *path;

	if((path = _p9translate(xpath)) == nil)
		return -1;

	cexec = mode&OCEXEC;
	rclose = mode&ORCLOSE;
	mode &= ~(ORCLOSE|OCEXEC);

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
		if(cexec)
			fcntl(fd, F_SETFL, FD_CLOEXEC);
		if(rclose)
			remove(path);
	}
	if(path != xpath)
		free(path);
	return fd;
}
