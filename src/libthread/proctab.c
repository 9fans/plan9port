#include "threadimpl.h"

/* this will need work */
enum
{
	PTABHASH = 257,
};

static Lock ptablock;
Proc *ptab[PTABHASH];

void
_threadsetproc(Proc *p)
{
	int h;

	lock(&ptablock);
	h = ((unsigned)p->pid)%PTABHASH;
	p->link = ptab[h];
	unlock(&ptablock);
	ptab[h] = p;
}

static Proc*
__threadgetproc(int rm)
{
	Proc **l, *p;
	int h, pid;

	pid = _threadgetpid();

	lock(&ptablock);
	h = ((unsigned)pid)%PTABHASH;
	for(l=&ptab[h]; p=*l; l=&p->link){
		if(p->pid == pid){
			if(rm)
				*l = p->link;
			unlock(&ptablock);
			return p;
		}
	}
	unlock(&ptablock);
	return nil;
}

Proc*
_threadgetproc(void)
{
	return __threadgetproc(0);
}

Proc*
_threaddelproc(void)
{
	return __threadgetproc(1);
}
