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
	uint tag;
	Muxrpc *r, *r2;
	void *p;

	/* must malloc because stack could be private */
	r = mallocz(sizeof(Muxrpc), 1);
	if(r == nil)
		return nil;
	r->r.l = &mux->lk;

	/* assign the tag */
	tag = gettag(mux, r);
	if(mux->settag(mux, tx, tag) < 0){
		puttag(mux, r);
		free(r);
		return nil;
	}

	/* send the packet */
	if(_muxsend(mux, tx) < 0){
		puttag(mux, r);
		free(r);
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
			rwakeup(&r2->r);
		}
		mux->muxer = 0;

		/* if there is anyone else sleeping, wake them to mux */
		if(mux->sleep.next != &mux->sleep)
			rwakeup(&mux->sleep.next->r);
	}
	p = r->p;
	puttag(mux, r);
	free(r);
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
	int i;

Again:
	while(mux->nwait == mux->mwait)
		rsleep(&mux->tagrend);
	i=mux->freetag;
	if(mux->wait[i] == 0)
		goto Found;
	for(i=0; i<mux->mwait; i++)
		if(mux->wait[i] == 0){
		Found:
			mux->nwait++;
			mux->wait[i] = r;
			r->tag = i;
			return i;
		}
	fprint(2, "libfs: nwait botch\n");
	goto Again;
}

static void
puttag(Mux *mux, Muxrpc *r)
{
	assert(mux->wait[r->tag] == r);
	mux->wait[r->tag] = nil;
	mux->nwait--;
	mux->freetag = r->tag;
	rwakeup(&mux->tagrend);
}
