/*
 * Implement threadexitsall by sending a signal to every proc.
 *
 * To be included from another C file (e.g., Linux-clone.c).
 */

void
_threadexitallproc(char *exitstr)
{
	Proc *p;
	int mypid;

	mypid = getpid();
	lock(&_threadpq.lock);
	for(p=_threadpq.head; p; p=p->next)
		if(p->pid > 1 && p->pid != mypid)
			kill(p->pid, SIGUSR2);
	exits(exitstr);
}

void
_threadexitproc(char *exitstr)
{
	_exits(exitstr);
}
