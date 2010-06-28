#define getpts not_using_this_getpts
#include "bsdpty.c"
#undef getpts
#include <signal.h>

int
getpts(int fd[], char *slave)
{
	void (*f)(int);

	f = signal(SIGCHLD, SIG_DFL);
	if(openpty(&fd[1], &fd[0], NULL, NULL, NULL) >= 0){
		fchmod(fd[1], 0620);
		strcpy(slave, ttyname(fd[0]));
		signal(SIGCHLD, f);
		return 0;
	}
	sysfatal("no ptys");
	return 0;
}
