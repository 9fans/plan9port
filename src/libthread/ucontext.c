#include "threadimpl.h"

void
_threadinitstack(Thread *t, void (*f)(void*), void *arg)
{
	sigset_t zero;

	/* do a reasonable initialization */
	memset(&t->context.uc, 0, sizeof t->context.uc);
	sigemptyset(&zero);
	sigprocmask(SIG_BLOCK, &zero, &t->context.uc.uc_sigmask);

	/* call getcontext, because on Linux makecontext neglects floating point */
	getcontext(&t->context.uc);

	/* call makecontext to do the real work. */
	/* leave a few words open on both ends */
	t->context.uc.uc_stack.ss_sp = t->stk+8;
	t->context.uc.uc_stack.ss_size = t->stksize-16;
	makecontext(&t->context.uc, (void(*)())f, 1, arg);
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

