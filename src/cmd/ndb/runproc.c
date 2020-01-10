#include <u.h>
#include <libc.h>
#include <thread.h>
#include <bio.h>
#include <ndb.h>
#include "dns.h"

Waitmsg*
runprocfd(char *file, char **v, int fd[3])
{
	int pid, i;

	threadwaitchan();
	pid = threadspawn(fd, file, v);
	for(i=0; i<3; i++)
		close(fd[i]);
	if(pid < 0)
		return nil;
	return procwait(pid);
}

Waitmsg*
runproc(char *file, char **v, int devnull)
{
	int fd[3], i;

	if(devnull){
		fd[0] = open("/dev/null", ORDWR);
		fd[1] = dup(1, fd[0]);
		fd[2] = dup(2, fd[0]);
	}else{
		for(i=0; i<3; i++)
			fd[i] = dup(i, -1);
	}
	return runprocfd(file, v, fd);
}
