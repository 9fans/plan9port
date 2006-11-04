/* Copyright (C) 2003 Russ Cox, Massachusetts Institute of Technology */
/* See COPYRIGHT */

#include <u.h>
#include <libc.h>
#include <mux.h>

typedef struct Qel Qel;
struct Qel
{
	Qel *next;
	void *p;
};

struct Muxqueue
{
	int hungup;
	QLock lk;
	Rendez r;
	Qel *head;
	Qel *tail;
};

Muxqueue*
_muxqalloc(void)
{
	Muxqueue *q;

	q = mallocz(sizeof(Muxqueue), 1);
	if(q == nil)
		return nil;
	q->r.l = &q->lk;
	return q;
}

int
_muxqsend(Muxqueue *q, void *p)
{
	Qel *e;

	e = malloc(sizeof(Qel));
	if(e == nil)
		return -1;
	qlock(&q->lk);
	if(q->hungup){
		werrstr("hungup queue");
		qunlock(&q->lk);
		free(e);
		return -1;
	}
	e->p = p;
	e->next = nil;
	if(q->head == nil)
		q->head = e;
	else
		q->tail->next = e;
	q->tail = e;
	rwakeup(&q->r);
	qunlock(&q->lk);
	return 0;
}

void*
_muxqrecv(Muxqueue *q)
{
	void *p;
	Qel *e;

	qlock(&q->lk);
	while(q->head == nil && !q->hungup)
		rsleep(&q->r);
	if(q->hungup){
		qunlock(&q->lk);
		return nil;
	}
	e = q->head;
	q->head = e->next;
	qunlock(&q->lk);
	p = e->p;
	free(e);
	return p;
}

int
_muxnbqrecv(Muxqueue *q, void **vp)
{
	void *p;
	Qel *e;

	qlock(&q->lk);
	if(q->head == nil){
		qunlock(&q->lk);
		*vp = nil;
		return q->hungup;
	}
	e = q->head;
	q->head = e->next;
	qunlock(&q->lk);
	p = e->p;
	free(e);
	*vp = p;
	return 1;
}

void
_muxqhangup(Muxqueue *q)
{
	qlock(&q->lk);
	q->hungup = 1;
	rwakeupall(&q->r);
	qunlock(&q->lk);
}
