#include <u.h>
#include <libc.h>

int
create(char *path, int mode, ulong perm)
{
	return open(path, mode|O_CREAT|O_TRUNC, perm);
}
