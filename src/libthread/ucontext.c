#include "threadimpl.h"

void
_threadinitstack(Thread *t, void (*f)(void*), void *arg)
{
	sigset_t zero;

	/* do a reasonable initialization */
	memset(&t->sched.uc, 0, sizeof t->sched.uc);
	sigemptyset(&zero);
	sigprocmask(SIG_BLOCK, &zero, &t->sched.uc.uc_sigmask);

	/* call getcontext, because on Linux makecontext neglects floating point */
	getcontext(&t->sched.uc);

	/* call makecontext to do the real work. */
	t->sched.uc.uc_stack.ss_sp = t->stk;
	t->sched.uc.uc_stack.ss_size = t->stksize;
	makecontext(&t->sched.uc, (void(*)())f, 1, arg);
}

void
_threadinswitch(int enter)
{
	USED(enter);
}

void
_threadstacklimit(void *bottom, void *top)
{
	USED(bottom);
	USED(top);
}

void
_swaplabel(Label *old, Label *new)
{
	if(swapcontext(&old->uc, &new->uc) < 0)
		sysfatal("swapcontext: %r");
}

