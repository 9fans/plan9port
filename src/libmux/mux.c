/* Copyright (C) 2003 Russ Cox, Massachusetts Institute of Technology */
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
	mux->tagrend.l = &mux->lk;
	mux->sleep.next = &mux->sleep;
	mux->sleep.prev = &mux->sleep;
}

void*
muxrpc(Mux *mux, void *tx)
{
	int tag;
	Muxrpc *r, *r2;
	void *p;

	/* must malloc because stack could be private */
	r = mallocz(sizeof(Muxrpc), 1);
	if(r == nil)
		return nil;
	r->r.l = &mux->lk;

	/* assign the tag */
	qlock(&mux->lk);
	tag = gettag(mux, r);
	qunlock(&mux->lk);
	if(tag < 0 || mux->settag(mux, tx, tag) < 0 || _muxsend(mux, tx) < 0){
		qlock(&mux->lk);
		puttag(mux, r);
		qunlock(&mux->lk);
		return nil;
	}

	/* add ourselves to sleep queue */
	qlock(&mux->lk);
	enqueue(mux, r);

	/* wait for our packet */
	while(mux->muxer && !r->p)
		rsleep(&r->r);

	/* if not done, there's no muxer: start muxing */
	if(!r->p){
		if(mux->muxer)
			abort();
		mux->muxer = 1;
		while(!r->p){
			qunlock(&mux->lk);
			p = _muxrecv(mux);
			if(p)
				tag = mux->gettag(mux, p);
			else
				tag = ~0;
			qlock(&mux->lk);
			if(p == nil){	/* eof -- just give up and pass the buck */
				dequeue(mux, r);
				break;
			}
			/* hand packet to correct sleeper */
			if(tag < 0 || tag >= mux->mwait){
				fprint(2, "%s: bad rpc tag %ux\n", argv0, tag);
				/* must leak packet! don't know how to free it! */
				continue;
			}
			r2 = mux->wait[tag];
			r2->p = p;
			dequeue(mux, r2);
			rwakeup(&r2->r);
		}
		mux->muxer = 0;

		/* if there is anyone else sleeping, wake them to mux */
		if(mux->sleep.next != &mux->sleep)
			rwakeup(&mux->sleep.next->r);
	}
	p = r->p;
	puttag(mux, r);
	qunlock(&mux->lk);
	return p;
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
	return i;
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
fprint(2, "free %p\n", r);
	free(r);
}
