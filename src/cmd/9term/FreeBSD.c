#include "9term.h"
#include <libutil.h>

int
getchildwd(int pid, char *wdir, int bufn)
{
	USED(pid);
	USED(wdir);
	USED(bufn);
	return -1;
}

int
getpts(int fd[], char *slave)
{
	return openpty(&fd[1], &fd[0], slave, 0, 0);
}
