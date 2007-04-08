#include <u.h>
#include <libc.h>
#include <venti.h>
#include "queue.h"

int chattyventi;

VtConn*
vtconn(int infd, int outfd)
{
	VtConn *z;
	NetConnInfo *nci;

	z = vtmallocz(sizeof(VtConn));
	z->tagrend.l = &z->lk;
	z->rpcfork.l = &z->lk;
	z->infd = infd;
	z->outfd = outfd;
	z->part = packetalloc();
	nci = getnetconninfo(nil, infd);
	if(nci == nil)
		snprint(z->addr, sizeof z->addr, "/dev/fd/%d", infd);
	else{
		strecpy(z->addr, z->addr+sizeof z->addr, nci->raddr);
		freenetconninfo(nci);
	}
	return z;
}

void
vtfreeconn(VtConn *z)
{
	vthangup(z);
	qlock(&z->lk);
	/*
	 * Wait for send and recv procs to notice
	 * the hangup and clear out the queues.
	 */
	while(z->readq || z->writeq){
		if(z->readq)
			_vtqhangup(z->readq);
		if(z->writeq)
			_vtqhangup(z->writeq);
		rsleep(&z->rpcfork);
	}
	packetfree(z->part);
	vtfree(z->version);
	vtfree(z->sid);
	qunlock(&z->lk);
	vtfree(z);
}
