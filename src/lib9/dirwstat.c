#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

#include <sys/time.h>

int
dirwstat(char *file, Dir *dir)
{
	struct timeval tv[2];

	/* BUG handle more */
	if(dir->mtime == ~0ULL)
		return 0;

	tv[0].tv_sec = dir->mtime;
	tv[0].tv_usec = 0;
	tv[1].tv_sec = dir->mtime;
	tv[1].tv_usec = 0;
	return utimes(file, tv);
}
