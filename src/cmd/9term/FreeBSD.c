#include "9term.h"

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
	openpty(&fd[1], &fd[0], slave, 0, 0);
	return 0;
}
