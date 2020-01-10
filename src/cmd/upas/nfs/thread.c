#include "a.h"

typedef struct New New;
struct New
{
	void (*fn)(void*);
	void *arg;
};

Channel *mailthreadchan;

void
mailthread(void (*fn)(void*), void *arg)
{
	New n;

	n.fn = fn;
	n.arg = arg;
	send(mailthreadchan, &n);
}

void
mailproc(void *v)
{
	New n;

	USED(v);
	while(recv(mailthreadchan, &n) == 1)
		threadcreate(n.fn, n.arg, STACK);
}

void
mailthreadinit(void)
{
	mailthreadchan = chancreate(sizeof(New), 0);
	proccreate(mailproc, nil, STACK);
}
