#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>
#include <sys/time.h>
#include <utime.h>

int
dirwstat(char *file, Dir *dir)
{
	struct utimbuf ub;

	/* BUG handle more */
	if(~dir->mtime == 0)
		return 0;

	ub.actime = dir->mtime;
	ub.modtime = dir->mtime;
	return utime(file, &ub);
}
