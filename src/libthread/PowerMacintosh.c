#include "threadimpl.h"

static void
launcherpower(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7,
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
	--stk;
	--stk;
	--stk;
	*--stk = (ulong)arg;
	*--stk = (ulong)f;
	t->sched.pc = (ulong)launcherpower+LABELDPC;
	t->sched.sp = (ulong)tos-80;
}

void
_threadinswitch(int enter)
{
	USED(enter);
}

void
_threadstacklimit(void *addr, void *addr2)
{
	USED(addr);
}

