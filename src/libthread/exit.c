#include <u.h>
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
	_threaddebug(DBGSCHED, "threadexitsall %s", exitstr);
	if(exitstr == nil)
		exitstr = "";
	_threadexitsallstatus = exitstr;
	_threaddebug(DBGSCHED, "_threadexitsallstatus set to %p", _threadexitsallstatus);
	/* leave */
	_kthreadexitallproc(exitstr);
}

Channel*
threadwaitchan(void)
{
	if(_threadwaitchan==nil)
		_threadwaitchan = chancreate(sizeof(Waitmsg*), 16);
	return _threadwaitchan;
}
