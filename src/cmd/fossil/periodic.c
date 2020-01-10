#include "stdinc.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

struct Periodic {
	QLock lk;
	int die;		/* flag: quit if set */
	void (*f)(void*);	/* call this each period */
	void *a;		/* argument to f */
	int msec;		/* period */
};

static void periodicThread(void *a);

Periodic *
periodicAlloc(void (*f)(void*), void *a, int msec)
{
	Periodic *p;

	p = vtmallocz(sizeof(Periodic));
	p->f = f;
	p->a = a;
	p->msec = msec;
	if(p->msec < 10)
		p->msec = 10;

	proccreate(periodicThread, p, STACK);
	return p;
}

void
periodicKill(Periodic *p)
{
	if(p == nil)
		return;
	qlock(&p->lk);
	p->die = 1;
	qunlock(&p->lk);
}

static void
periodicFree(Periodic *p)
{
	vtfree(p);
}

static void
periodicThread(void *a)
{
	Periodic *p = a;
	vlong t, ct, ts;		/* times in ms. */

	threadsetname("periodic");

	ct = nsec() / 1000000;
	t = ct + p->msec;		/* call p->f at or after this time */

	for(;;){
		ts = t - ct;		/* ms. to next cycle's start */
		if(ts > 1000)
			ts = 1000;	/* bound sleep duration */
		if(ts > 0)
			sleep(ts);	/* wait for cycle's start */

		qlock(&p->lk);
		if(p->die){
			qunlock(&p->lk);
			break;
		}
		ct = nsec() / 1000000;
		if(t <= ct){		/* due to call p->f? */
			p->f(p->a);
			ct = nsec() / 1000000;
			while(t <= ct)	/* advance t to future cycle start */
				t += p->msec;
		}
		qunlock(&p->lk);
	}
	periodicFree(p);
}
