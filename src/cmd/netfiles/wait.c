#include <u.h>
#include <libc.h>
#include <thread.h>
#include <9pclient.h>
#include "acme.h"

extern int debug;

#define dprint if(debug>1)print

typedef struct Waitreq Waitreq;
struct Waitreq
{
	int pid;
	Channel *c;
};

/*
 * watch the exiting children
 */
Channel *twaitchan;	/* chan(Waitreq) */
void
waitthread(void *v)
{
	Alt a[3];
	Waitmsg *w, **wq;
	Waitreq *rq, r;
	int i, nrq, nwq;

	threadsetname("waitthread");
	a[0].c = threadwaitchan();
	a[0].v = &w;
	a[0].op = CHANRCV;
	a[1].c = twaitchan;
	a[1].v = &r;
	a[1].op = CHANRCV;
	a[2].op = CHANEND;

	nrq = 0;
	nwq = 0;
	rq = nil;
	wq = nil;
	dprint("wait: start\n");
	for(;;){
	cont2:;
		dprint("wait: alt\n");
		switch(alt(a)){
		case 0:
			dprint("wait: pid %d exited\n", w->pid);
			for(i=0; i<nrq; i++){
				if(rq[i].pid == w->pid){
					dprint("wait: match with rq chan %p\n", rq[i].c);
					sendp(rq[i].c, w);
					rq[i] = rq[--nrq];
					goto cont2;
				}
			}
			if(i == nrq){
				dprint("wait: queueing waitmsg\n");
				wq = erealloc(wq, (nwq+1)*sizeof(wq[0]));
				wq[nwq++] = w;
			}
			break;
		
		case 1:
			dprint("wait: req for pid %d chan %p\n", r.pid, r.c);
			for(i=0; i<nwq; i++){
				if(w->pid == r.pid){
					dprint("wait: match with waitmsg\n");
					sendp(r.c, w);
					wq[i] = wq[--nwq];
					goto cont2;
				}
			}
			if(i == nwq){
				dprint("wait: queueing req\n");
				rq = erealloc(rq, (nrq+1)*sizeof(rq[0]));
				rq[nrq] = r;
				dprint("wait: queueing req pid %d chan %p\n", rq[nrq].pid, rq[nrq].c);
				nrq++;
			}
			break;
		}
	}
}

Waitmsg*
twaitfor(int pid)
{
	Waitreq r;
	Waitmsg *w;
	
	r.pid = pid;
	r.c = chancreate(sizeof(Waitmsg*), 1);
	send(twaitchan, &r);
	w = recvp(r.c);
	chanfree(r.c);
	return w;
}

int
twait(int pid)
{
	int x;
	Waitmsg *w;
	
	w = twaitfor(pid);
	x = w->msg[0] != 0 ? -1 : 0;
	free(w);
	return x;
}

void
twaitinit(void)
{
	threadwaitchan();	/* allocate it before returning */
	twaitchan = chancreate(sizeof(Waitreq), 10);
	threadcreate(waitthread, nil, 128*1024);
}

