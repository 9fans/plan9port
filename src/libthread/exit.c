#include <signal.h>
#include "threadimpl.h"

char *_threadexitsallstatus;
Channel *_threadwaitchan;

void
threadexits(char *exitstr)
{
	Proc *p;
	Thread *t;

	p = _threadgetproc();
	t = p->thread;
	if(t == p->idle)
		p->idle = nil;
	t->moribund = 1;
	_threaddebug(DBGSCHED, "threadexits %s", exitstr);
	if(exitstr==nil)
		exitstr="";
	utfecpy(p->exitstr, p->exitstr+ERRMAX, exitstr);
	_sched();
}

void
threadexitsall(char *exitstr)
{
	Proc *p;
	int *pid;
	int i, npid, mypid;

	_threaddebug(DBGSCHED, "threadexitsall %s", exitstr);
	if(exitstr == nil)
		exitstr = "";
	_threadexitsallstatus = exitstr;
	_threaddebug(DBGSCHED, "_threadexitsallstatus set to %p", _threadexitsallstatus);
	mypid = _threadgetpid();

	/*
	 * signal others.
	 * copying all the pids first avoids other thread's
	 * teardown procedures getting in the way.
	 */
	lock(&_threadpq.lock);
	npid = 0;
	for(p=_threadpq.head; p; p=p->next)
		npid++;
	pid = _threadmalloc(npid*sizeof(pid[0]), 0);
	npid = 0;
	for(p = _threadpq.head; p; p=p->next)
		pid[npid++] = p->pid;
	unlock(&_threadpq.lock);
	for(i=0; i<npid; i++){
		_threaddebug(DBGSCHED, "threadexitsall kill %d", pid[i]);
		if(pid[i]==0 || pid[i]==-1)
			fprint(2, "bad pid in threadexitsall: %d\n", pid[i]);
		else if(pid[i] != mypid)
			kill(pid[i], SIGTERM);
	}

	/* leave */
	exit(0);
}

Channel*
threadwaitchan(void)
{
	if(_threadwaitchan==nil)
		_threadwaitchan = chancreate(sizeof(Waitmsg*), 16);
	return _threadwaitchan;
}
