/* Copyright (C) 2003-2006 Russ Cox, Massachusetts Institute of Technology */
/* See COPYRIGHT */

/*
 * Generic RPC packet multiplexor.  Inspired by but not derived from
 * Plan 9 kernel.  Originally developed as part of Tra, later used in
 * libnventi, and then finally split out into a generic library.
 */

#include <u.h>
#include <libc.h>
#include <mux.h>

static int gettag(Mux*, Muxrpc*);
static void puttag(Mux*, Muxrpc*);
static void enqueue(Mux*, Muxrpc*);
static void dequeue(Mux*, Muxrpc*);

void
muxinit(Mux *mux)
{
	memset(&mux->lk, 0, sizeof(Mux)-offsetof(Mux, lk));
	mux->tagrend.l = &mux->lk;
	mux->rpcfork.l = &mux->lk;
	mux->sleep.next = &mux->sleep;
	mux->sleep.prev = &mux->sleep;
}

static Muxrpc*
allocmuxrpc(Mux *mux)
{
	Muxrpc *r;

	/* must malloc because stack could be private */
	r = mallocz(sizeof(Muxrpc), 1);
	if(r == nil){
		werrstr("mallocz: %r");
		return nil;
	}
	r->mux = mux;
	r->r.l = &mux->lk;
	r->waiting = 1;

	return r;
}

static int
tagmuxrpc(Muxrpc *r, void *tx)
{
	int tag;
	Mux *mux;

	mux = r->mux;
	/* assign the tag, add selves to response queue */
	qlock(&mux->lk);
	tag = gettag(mux, r);
/*print("gettag %p %d\n", r, tag); */
	enqueue(mux, r);
	qunlock(&mux->lk);

	/* actually send the packet */
	if(tag < 0 || mux->settag(mux, tx, tag) < 0 || _muxsend(mux, tx) < 0){
		werrstr("settag/send tag %d: %r", tag);
		fprint(2, "%r\n");
		qlock(&mux->lk);
		dequeue(mux, r);
		puttag(mux, r);
		qunlock(&mux->lk);
		return -1;
	}
	return 0;
}

void
muxmsgandqlock(Mux *mux, void *p)
{
	int tag;
	Muxrpc *r2;

	tag = mux->gettag(mux, p) - mux->mintag;
/*print("mux tag %d\n", tag); */
	qlock(&mux->lk);
	/* hand packet to correct sleeper */
	if(tag < 0 || tag >= mux->mwait){
		fprint(2, "%s: bad rpc tag %ux\n", argv0, tag);
		/* must leak packet! don't know how to free it! */
		return;
	}
	r2 = mux->wait[tag];
	if(r2 == nil || r2->prev == nil){
		fprint(2, "%s: bad rpc tag %ux (no one waiting on that tag)\n", argv0, tag);
		/* must leak packet! don't know how to free it! */
		return;
	}
	r2->p = p;
	dequeue(mux, r2);
	rwakeup(&r2->r);
}

void
electmuxer(Mux *mux)
{
	Muxrpc *rpc;

	/* if there is anyone else sleeping, wake them to mux */
	for(rpc=mux->sleep.next; rpc != &mux->sleep; rpc = rpc->next){
		if(!rpc->async){
			mux->muxer = rpc;
			rwakeup(&rpc->r);
			return;
		}
	}
	mux->muxer = nil;
}

void*
muxrpc(Mux *mux, void *tx)
{
	int tag;
	Muxrpc *r;
	void *p;

	if((r = allocmuxrpc(mux)) == nil)
		return nil;

	if((tag = tagmuxrpc(r, tx)) < 0)
		return nil;

	qlock(&mux->lk);
	/* wait for our packet */
	while(mux->muxer && mux->muxer != r && !r->p)
		rsleep(&r->r);

	/* if not done, there's no muxer: start muxing */
	if(!r->p){
		if(mux->muxer != nil && mux->muxer != r)
			abort();
		mux->muxer = r;
		while(!r->p){
			qunlock(&mux->lk);
			_muxrecv(mux, 1, &p);
			if(p == nil){
				/* eof -- just give up and pass the buck */
				qlock(&mux->lk);
				dequeue(mux, r);
				break;
			}
			muxmsgandqlock(mux, p);
		}
		electmuxer(mux);
	}
	p = r->p;
	puttag(mux, r);
	qunlock(&mux->lk);
	if(p == nil)
		werrstr("unexpected eof");
	return p;
}

Muxrpc*
muxrpcstart(Mux *mux, void *tx)
{
	int tag;
	Muxrpc *r;

	if((r = allocmuxrpc(mux)) == nil)
		return nil;
	r->async = 1;
	if((tag = tagmuxrpc(r, tx)) < 0)
		return nil;
	return r;
}

int
muxrpccanfinish(Muxrpc *r, void **vp)
{
	void *p;
	Mux *mux;
	int ret;

	mux = r->mux;
	qlock(&mux->lk);
	ret = 1;
	if(!r->p && !mux->muxer){
		mux->muxer = r;
		while(!r->p){
			qunlock(&mux->lk);
			p = nil;
			if(!_muxrecv(mux, 0, &p))
				ret = 0;
			if(p == nil){
				qlock(&mux->lk);
				break;
			}
			muxmsgandqlock(mux, p);
		}
		electmuxer(mux);
	}
	p = r->p;
	if(p)
		puttag(mux, r);
	qunlock(&mux->lk);
	*vp = p;
	return ret;
}

static void
enqueue(Mux *mux, Muxrpc *r)
{
	r->next = mux->sleep.next;
	r->prev = &mux->sleep;
	r->next->prev = r;
	r->prev->next = r;
}

static void
dequeue(Mux *mux, Muxrpc *r)
{
	r->next->prev = r->prev;
	r->prev->next = r->next;
	r->prev = nil;
	r->next = nil;
}

static int
gettag(Mux *mux, Muxrpc *r)
{
	int i, mw;
	Muxrpc **w;

	for(;;){
		/* wait for a free tag */
		while(mux->nwait == mux->mwait){
			if(mux->mwait < mux->maxtag-mux->mintag){
				mw = mux->mwait;
				if(mw == 0)
					mw = 1;
				else
					mw <<= 1;
				w = realloc(mux->wait, mw*sizeof(w[0]));
				if(w == nil)
					return -1;
				memset(w+mux->mwait, 0, (mw-mux->mwait)*sizeof(w[0]));
				mux->wait = w;
				mux->freetag = mux->mwait;
				mux->mwait = mw;
				break;
			}
			rsleep(&mux->tagrend);
		}

		i=mux->freetag;
		if(mux->wait[i] == 0)
			goto Found;
		for(; i<mux->mwait; i++)
			if(mux->wait[i] == 0)
				goto Found;
		for(i=0; i<mux->freetag; i++)
			if(mux->wait[i] == 0)
				goto Found;
		/* should not fall out of while without free tag */
		fprint(2, "libfs: nwait botch\n");
		abort();
	}

Found:
	mux->nwait++;
	mux->wait[i] = r;
	r->tag = i+mux->mintag;
	return r->tag;
}

static void
puttag(Mux *mux, Muxrpc *r)
{
	int i;

	i = r->tag - mux->mintag;
	assert(mux->wait[i] == r);
	mux->wait[i] = nil;
	mux->nwait--;
	mux->freetag = i;
	rwakeup(&mux->tagrend);
	free(r);
}
