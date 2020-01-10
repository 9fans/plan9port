#include <u.h>
#include <libc.h>
#include <venti.h>
#include "queue.h"

typedef struct Qel Qel;
struct Qel
{
	Qel *next;
	void *p;
};

struct Queue
{
	int ref;
	int hungup;
	QLock lk;
	Rendez r;
	Qel *head;
	Qel *tail;
};

Queue*
_vtqalloc(void)
{
	Queue *q;

	q = vtmallocz(sizeof(Queue));
	q->r.l = &q->lk;
	q->ref = 1;
	return q;
}

Queue*
_vtqincref(Queue *q)
{
	qlock(&q->lk);
	q->ref++;
	qunlock(&q->lk);
	return q;
}

void
_vtqdecref(Queue *q)
{
	Qel *e;

	qlock(&q->lk);
	if(--q->ref > 0){
		qunlock(&q->lk);
		return;
	}
	assert(q->ref == 0);
	qunlock(&q->lk);

	/* Leaks the pointers e->p! */
	while(q->head){
		e = q->head;
		q->head = e->next;
		free(e);
	}
	free(q);
}

int
_vtqsend(Queue *q, void *p)
{
	Qel *e;

	e = vtmalloc(sizeof(Qel));
	qlock(&q->lk);
	if(q->hungup){
		werrstr("hungup queue");
		qunlock(&q->lk);
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
_vtqrecv(Queue *q)
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
	vtfree(e);
	return p;
}

void*
_vtnbqrecv(Queue *q)
{
	void *p;
	Qel *e;

	qlock(&q->lk);
	if(q->head == nil){
		qunlock(&q->lk);
		return nil;
	}
	e = q->head;
	q->head = e->next;
	qunlock(&q->lk);
	p = e->p;
	vtfree(e);
	return p;
}

void
_vtqhangup(Queue *q)
{
	qlock(&q->lk);
	q->hungup = 1;
	rwakeupall(&q->r);
	qunlock(&q->lk);
}
