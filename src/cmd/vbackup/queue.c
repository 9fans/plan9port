#include <u.h>
#include <libc.h>
#include <thread.h>
#include <venti.h>
#include <diskfs.h>
#include "queue.h"

Queue*
qalloc(void)
{
	Queue *q;

	q = vtmallocz(sizeof(Queue));
	q->r.l = &q->lk;
	return q;
}

Block*
qread(Queue *q, u32int *pbno)
{
	Block *db;
	u32int bno;

	qlock(&q->lk);
	while(q->nel == 0 && !q->closed)
		rsleep(&q->r);
	if(q->nel == 0 && q->closed){
		qunlock(&q->lk);
		return nil;
	}
	db = q->el[q->ri].db;
	bno = q->el[q->ri].bno;
	if(++q->ri == MAXQ)
		q->ri = 0;
	if(q->nel-- == MAXQ/2)
		rwakeup(&q->r);
	qunlock(&q->lk);
	*pbno = bno;
	return db;
}

void
qwrite(Queue *q, Block *db, u32int bno)
{
	qlock(&q->lk);
	while(q->nel == MAXQ)
		rsleep(&q->r);
	q->el[q->wi].db = db;
	q->el[q->wi].bno = bno;
	if(++q->wi == MAXQ)
		q->wi = 0;
	if(q->nel++ == MAXQ/2)
		rwakeup(&q->r);
	qunlock(&q->lk);
}

void
qclose(Queue *q)
{
	qlock(&q->lk);
	q->closed = 1;
	rwakeup(&q->r);
	qunlock(&q->lk);
}

void
qfree(Queue *q)
{
	vtfree(q);
}
