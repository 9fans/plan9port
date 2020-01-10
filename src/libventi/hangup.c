#include <u.h>
#ifdef PLAN9PORT
#include <sys/socket.h>
#endif
#include <libc.h>
#include <venti.h>
#include "queue.h"

void
vthangup(VtConn *z)
{
	qlock(&z->lk);
	z->state = VtStateClosed;
#ifdef PLAN9PORT
	/* try to make the read in vtrecvproc fail */
	shutdown(SHUT_WR, z->infd);
	shutdown(SHUT_WR, z->outfd);
#endif
	if(z->infd >= 0)
		close(z->infd);
	if(z->outfd >= 0 && z->outfd != z->infd)
		close(z->outfd);
	z->infd = -1;
	z->outfd = -1;
	if(z->writeq)
		_vtqhangup(z->writeq);
	if(z->readq)
		_vtqhangup(z->readq);
	qunlock(&z->lk);
}
