#include <u.h>
#include <signal.h>
#include "threadimpl.h"

typedef struct Mainarg Mainarg;
struct Mainarg
{
	int	argc;
	char	**argv;
};

int	mainstacksize;
int	_threadnotefd;
int	_threadpasserpid;
static void mainlauncher(void*);
extern void (*_sysfatal)(char*, va_list);

void
_threadstatus(int x)
{
	USED(x);
	threadstatus();
}

void
_threaddie(int x)
{
	extern char *_threadexitsallstatus;
	USED(x);

	if(_threadexitsallstatus)
		_exits(_threadexitsallstatus);
}

int
main(int argc, char **argv)
{
	Mainarg *a;
	Proc *p;

//_threaddebuglevel = (DBGSCHED|DBGCHAN|DBGREND)^~0;
	_systhreadinit();
	_qlockinit(_threadsleep, _threadwakeup);
	_sysfatal = _threadsysfatal;
	notify(_threadnote);
	if(mainstacksize == 0)
		mainstacksize = 32*1024;

	a = _threadmalloc(sizeof *a, 1);
	a->argc = argc;
	a->argv = argv;
malloc(10);
	p = _newproc(mainlauncher, a, mainstacksize, "threadmain", 0, 0);
malloc(10);
	_schedinit(p);
	abort();	/* not reached */
	return 0;
}

static void
mainlauncher(void *arg)
{
	Mainarg *a;

malloc(10);
	a = arg;
malloc(10);
	threadmain(a->argc, a->argv);
	threadexits("threadmain");
}

void
_threadsignal(void)
{
}

void
_threadsignalpasser(void)
{
}

int
_schedfork(Proc *p)
{
	int pid;
	lock(&p->lock);
	pid = ffork(RFMEM|RFNOWAIT, _schedinit, p);
	p->pid = pid;
	unlock(&p->lock);
	return pid;
	
}

void
_schedexit(Proc *p)
{
	char ex[ERRMAX];
	Proc **l;

	lock(&_threadpq.lock);
	for(l=&_threadpq.head; *l; l=&(*l)->next){
		if(*l == p){
			*l = p->next;
			if(*l == nil)
				_threadpq.tail = l;
			break;
		}
	}
	_threadprocs--;
	unlock(&_threadpq.lock);

	strncpy(ex, p->exitstr, sizeof ex);
	ex[sizeof ex-1] = '\0';
	free(p);
	_exits(ex);
}

int
nrand(int n)
{
	return random()%n;
}

void
_systhreadinit(void)
{
}

void
threadstats(void)
{
	extern int _threadnrendez, _threadhighnrendez,
		_threadnalt, _threadhighnentry;
	fprint(2, "*** THREAD LIBRARY STATS ***\n");
	fprint(2, "nrendez %d high simultaneous %d\n", 
		_threadnrendez, _threadhighnrendez);
	fprint(2, "nalt %d high simultaneous entry %d\n",
		_threadnalt, _threadhighnentry);
}
