#include <sched.h>
#include <signal.h>
#include <lib9.h>

int fforkstacksize = 16384;

int
ffork(int flags, void (*fn)(void*), void *arg)
{
	char *p;
	int cloneflag, pid;

	p = malloc(fforkstacksize);
	if(p == nil)
		return -1;
	cloneflag = 0;
	flags &= ~RFPROC;
	if(flags&RFMEM){
		cloneflag |= CLONE_VM;
		flags &= ~RFMEM;
	}
	if(!(flags&RFFDG))
		cloneflag |= CLONE_FILES;
	else
		flags &= ~RFFDG;
	if(!(flags&RFNOWAIT))
		cloneflag |= SIGCHLD;
	else
		flags &= ~RFNOWAIT;
	if(flags){
		fprint(2, "unknown rfork flags %x\n", flags);
		return -1;
	}
	pid = clone((int(*)(void*))fn, p+fforkstacksize-16, cloneflag, arg);
	if(pid < 0)
		free(p);
	return pid;
}

