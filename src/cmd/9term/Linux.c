#include "9term.h"

void
pdx(int pid, char *wdir, int bufn)
{
	char path[256];
	int n;

	snprint(path, sizeof path, "/proc/%d/cwd", pid);
	n = readlink(path, wdir, bufn);
	if(n < 0)
		n = 0;
	wdir[n] = '\0';
}

int
getpts(int fd[], char *slave)
{

	openpty(&fd[1], &fd[0], slave, 0, 0);
	return 0;
}
