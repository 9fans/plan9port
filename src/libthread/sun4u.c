#include "threadimpl.h"

static void
launchersparc(int o0, int o1, int o2, int o3, int o4,
	void (*f)(void *arg), void *arg)
{
	(*f)(arg);
	threadexits(nil);
}

void
_threadinitstack(Thread *t, void (*f)(void*), void *arg)
{
	ulong *tos, *stk;

	tos = (ulong*)&t->stk[t->stksize&~7];
	stk = tos;
	--stk;
	*--stk = (ulong)arg;
	*--stk = (ulong)f;
	t->sched.link = (ulong)launchersparc - 8;
	t->sched.input[6] = 0;
	t->sched.sp = (ulong)stk - 0x5c;
}

