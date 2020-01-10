#define NOPLAN9DEFINES
#include <u.h>
#include <libc.h>
#include <sys/time.h>
#include <sys/stat.h>

#if defined(__FreeBSD__) || defined(__APPLE__) || defined(__OpenBSD__) || defined(__linux__)
/* do nothing -- futimes exists and is fine */

#elif defined(__SunOS5_9__)
/* use futimesat */
static int
futimes(int fd, struct timeval *tv)
{
	return futimesat(fd, 0, tv);
}

#else
/* provide dummy */
/* rename just in case -- linux provides an unusable one */
#undef futimes
#define futimes myfutimes
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

	ret = 0;
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
	if(~dir->length != 0){
		if(ftruncate(fd, dir->length) < 0)
			ret = -1;
	}
	return ret;
}
