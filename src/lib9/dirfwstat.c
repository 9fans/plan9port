#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

#include <sys/time.h>

#if !defined(_HAVEFUTIMES) && defined(_HAVEFUTIMESAT)
static int
futimes(int fd, struct timeval *tv)
{
	return futimesat(fd, 0, tv);
}
#elif !defined(_HAVEFUTIMES)
static int
futimes(int fd, struct timeval *tv)
{
	werrstr("futimes not available");
	return -1;
}
#endif

int
dirfwstat(int fd, Dir *dir)
{
	int ret;
	struct timeval tv[2];

	if(~dir->mode != 0){
		if(fchmod(fd, dir->mode) < 0)
			ret = -1;
	}
	if(~dir->mtime != 0){
		tv[0].tv_sec = dir->mtime;
		tv[0].tv_usec = 0;
		tv[1].tv_sec = dir->mtime;
		tv[1].tv_usec = 0;
		if(futimes(fd, tv) < 0)
			ret = -1;
	}
	return ret;
}

