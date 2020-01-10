/* Copyright (C) 2003 Russ Cox, Massachusetts Institute of Technology */
/* See COPYRIGHT */

#include <u.h>
#include <libc.h>
#include <mux.h>

/*
 * If you fork off two procs running muxrecvproc and muxsendproc,
 * then muxrecv/muxsend (and thus muxrpc) will never block except on
 * rendevouses, which is nice when it's running in one thread of many.
 */
void
_muxrecvproc(void *v)
{
	void *p;
	Mux *mux;
	Muxqueue *q;

	mux = v;
	q = _muxqalloc();

	qlock(&mux->lk);
	mux->readq = q;
	qlock(&mux->inlk);
	rwakeup(&mux->rpcfork);
	qunlock(&mux->lk);

	while((p = mux->recv(mux)) != nil)
		if(_muxqsend(q, p) < 0){
			free(p);
			break;
		}
	qunlock(&mux->inlk);
	qlock(&mux->lk);
	_muxqhangup(q);
	p = nil;
	while(_muxnbqrecv(q, &p) && p != nil){
		free(p);
		p = nil;
	}
	free(q);
	mux->readq = nil;
	rwakeup(&mux->rpcfork);
	qunlock(&mux->lk);
}

void
_muxsendproc(void *v)
{
	Muxqueue *q;
	void *p;
	Mux *mux;

	mux = v;
	q = _muxqalloc();

	qlock(&mux->lk);
	mux->writeq = q;
	qlock(&mux->outlk);
	rwakeup(&mux->rpcfork);
	qunlock(&mux->lk);

	while((p = _muxqrecv(q)) != nil)
		if(mux->send(mux, p) < 0)
			break;
	qunlock(&mux->outlk);
	qlock(&mux->lk);
	_muxqhangup(q);
	while(_muxnbqrecv(q, &p))
		free(p);
	free(q);
	mux->writeq = nil;
	rwakeup(&mux->rpcfork);
	qunlock(&mux->lk);
	return;
}

int
_muxrecv(Mux *mux, int canblock, void **vp)
{
	void *p;
	int ret;

	qlock(&mux->lk);
	if(mux->readq){
		qunlock(&mux->lk);
		if(canblock){
			*vp = _muxqrecv(mux->readq);
			return 1;
		}
		return _muxnbqrecv(mux->readq, vp);
	}

	qlock(&mux->inlk);
	qunlock(&mux->lk);
	if(canblock){
		p = mux->recv(mux);
		ret = 1;
	}else{
		if(mux->nbrecv)
			ret = mux->nbrecv(mux, &p);
		else{
			/* send eof, not "no packet ready" */
			p = nil;
			ret = 1;
		}
	}
	qunlock(&mux->inlk);
	*vp = p;
	return ret;
}

int
_muxsend(Mux *mux, void *p)
{
	qlock(&mux->lk);
/*
	if(mux->state != VtStateConnected){
		packetfree(p);
		werrstr("not connected");
		qunlock(&mux->lk);
		return -1;
	}
*/
	if(mux->writeq){
		qunlock(&mux->lk);
		if(_muxqsend(mux->writeq, p) < 0){
			free(p);
			return -1;
		}
		return 0;
	}

	qlock(&mux->outlk);
	qunlock(&mux->lk);
	if(mux->send(mux, p) < 0){
		qunlock(&mux->outlk);
		/* vthangup(mux); */
		return -1;
	}
	qunlock(&mux->outlk);
	return 0;
}
