#if defined(__linux__)
#include "ucontext.c"
#else

#include "threadimpl.h"
/*
 * To use this you need some patches to Valgrind that
 * let it help out with detecting stack overflow. 
 */
#ifdef USEVALGRIND
#include <valgrind/memcheck.h>
#endif

static void
launcher386(void (*f)(void *arg), void *arg)
{
	Proc *p;
	Thread *t;

	p = _threadgetproc();
	t = p->thread;
	_threadstacklimit(t->stk, t->stk+t->stksize);

	(*f)(arg);
	threadexits(nil);
}

void
_threadinitstack(Thread *t, void (*f)(void*), void *arg)
{
	ulong *tos;

	tos = (ulong*)&t->stk[t->stksize&~7];
	*--tos = (ulong)arg;
	*--tos = (ulong)f;
	t->sched.pc = (ulong)launcher386;
	t->sched.sp = (ulong)tos - 8;		/* old PC and new PC */
}

void
_threadinswitch(int enter)
{
	USED(enter);
#ifdef USEVALGRIND
	if(enter)
		VALGRIND_SET_STACK_LIMIT(0, 0, 0);
	else
		VALGRIND_SET_STACK_LIMIT(0, 0, 1);
#endif
}

void
_threadstacklimit(void *bottom, void *top)
{
	USED(bottom);
	USED(top);

#ifdef USEVALGRIND
	VALGRIND_SET_STACK_LIMIT(1, bottom, top);
#endif
}
#endif
