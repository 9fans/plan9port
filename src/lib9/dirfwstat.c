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
#endif

int
dirfwstat(int fd, Dir *dir)
{
	struct timeval tv[2];

	/* BUG handle more */
	if(dir->mtime == ~0ULL)
		return 0;

	tv[0].tv_sec = dir->mtime;
	tv[0].tv_usec = 0;
	tv[1].tv_sec = dir->mtime;
	tv[1].tv_usec = 0;
	return futimes(fd, tv);
}

