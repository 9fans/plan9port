#include "9term.h"

int
getchildwd(int pid, char *wdir, int bufn)
{
	char path[256];
	char cwd[256];

	if(getcwd(cwd, sizeof cwd) < 0)
		return -1;
	snprint(path, sizeof path, "/proc/%d/cwd", pid);
	if(chdir(path) < 0)
		return -1;
	if(getcwd(wdir, bufn) < 0)
		return -1;
	chdir(cwd);
	return 0;
}

int
getpts(int fd[], char *slave)
{
	fd[1] = open("/dev/ptmx", ORDWR);
	if ((grantpt(fd[1]) < 0) || (unlockpt(fd[1]) < 0))
		return -1;
	fchmod(fd[1], 0622);
	strcpy(slave, ptsname(fd[1]));
	fd[0] = open(slave, OREAD);
	return 0;
}
