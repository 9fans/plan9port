#define getpts not_using_this_getpts
#include "bsdpty.c"
#undef getpts

#include <util.h>

int
getpts(int fd[], char *slave)
{
	if(openpty(&fd[1], &fd[0], NULL, NULL, NULL) >= 0){
		fchmod(fd[1], 0620);
		strcpy(slave, ttyname(fd[0]));
		return 0;
	}
	sysfatal("no ptys: %r");
	return 0;
}
