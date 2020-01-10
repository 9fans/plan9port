#include "stdinc.h"
#include "dat.h"
#include "fns.h"

void
waitforkick(Round *r)
{
	int n;

	qlock(&r->lock);
	r->last = r->current;
	assert(r->current+1 == r->next);
	rwakeupall(&r->finish);
	while(!r->doanother)
		rsleep(&r->start);
	n = r->next++;
	r->current = n;
	r->doanother = 0;
	qunlock(&r->lock);
}

static void
_kickround(Round *r, int wait)
{
	int n;

	if(!r->doanother)
		trace(TraceProc, "kick %s", r->name);
	r->doanother = 1;
	rwakeup(&r->start);
	if(wait){
		n = r->next;
		while((int)(n - r->last) > 0){
			r->doanother = 1;
			rwakeup(&r->start);
			rsleep(&r->finish);
		}
	}
}

void
kickround(Round *r, int wait)
{
	qlock(&r->lock);
	_kickround(r, wait);
	qunlock(&r->lock);
}

void
initround(Round *r, char *name, int delay)
{
	memset(r, 0, sizeof *r);
	r->name = name;
	r->start.l = &r->lock;
	r->finish.l = &r->lock;
	r->delaywait.l = &r->lock;
	r->last = 0;
	r->current = 0;
	r->next = 1;
	r->doanother = 0;
	r->delaytime = delay;
}

void
delaykickround(Round *r)
{
	qlock(&r->lock);
	r->delaykick = 1;
	rwakeup(&r->delaywait);
	qunlock(&r->lock);
}

void
delaykickroundproc(void *v)
{
	Round *r = v;
	int n;

	threadsetname("delaykickproc %s", r->name);
	qlock(&r->lock);
	for(;;){
		while(r->delaykick == 0){
			trace(TraceProc, "sleep");
			rsleep(&r->delaywait);
		}

		n = r->next;
		qunlock(&r->lock);

		trace(TraceProc, "waitround 0x%ux", (uint)n);
		sleep(r->delaytime);

		qlock(&r->lock);
		if(n == r->next){
			trace(TraceProc, "kickround 0x%ux", (uint)n);
			_kickround(r, 1);
		}

		trace(TraceProc, "finishround 0x%ux", (uint)n);
	}
}
