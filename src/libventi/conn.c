#include <u.h>
#include <libc.h>
#include <venti.h>
#include "queue.h"

int chattyventi;

VtConn*
vtconn(int infd, int outfd)
{
	VtConn *z;

	z = vtmallocz(sizeof(VtConn));
	z->tagrend.l = &z->lk;
	z->rpcfork.l = &z->lk;
	z->infd = infd;
	z->outfd = outfd;
	z->part = packetalloc();
	return z;
}

void
vtfreeconn(VtConn *z)
{
	vthangup(z);
	qlock(&z->lk);
	for(;;){
		if(z->readq)
			_vtqhangup(z->readq);
		else if(z->writeq)
			_vtqhangup(z->writeq);
		else
			break;
		rsleep(&z->rpcfork);
	}
	packetfree(z->part);
	vtfree(z);
}
