#include <u.h>
#include <sched.h>
#include <signal.h>
#include <libc.h>
#include "9proc.h"

int fforkstacksize = 16384;

static int
tramp(void *v)
{
	void (*fn)(void*), *arg;
	void **v2;

	_p9uproc(0);
	v2 = v;
	fn = v2[0];
	arg = v2[1];
	free(v2);
	fn(arg);
	return 0;
}
	
int
ffork(int flags, void (*fn)(void*), void *arg)
{
	void **v;
	char *p;
	int cloneflag, pid;

	_p9uproc(0);
	p = malloc(fforkstacksize);
	v = malloc(sizeof(void*)*2);
	if(p==nil || v==nil){
		free(p);
		free(v);
		return -1;
	}
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
		free(p);
		free(v);
		return -1;
	}
	v[0] = fn;
	v[1] = arg;
	pid = clone(tramp, p+fforkstacksize-16, cloneflag, v);
	if(pid < 0)
		free(p);
	return pid;
}

int
getfforkid(void)
{
	return getpid();
}

