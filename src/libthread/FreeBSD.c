#include "threadimpl.h"

#include "BSD.c"

/*
 * FreeBSD 4 and earlier needs the context functions.
 */
void
makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...)
{
	int *sp;

	sp = (int*)ucp->uc_stack.ss_sp+ucp->uc_stack.ss_size/4;
	sp -= argc;
	memmove(sp, &argc+1, argc*sizeof(int));
	*--sp = 0;		/* return address */
	ucp->uc_mcontext.mc_eip = (long)func;
	ucp->uc_mcontext.mc_esp = (int)sp;
}

int
swapcontext(ucontext_t *oucp, ucontext_t *ucp)
{
	if(getcontext(oucp) == 0)
		setcontext(ucp);
	return 0;
}

void
_pthreadinit(void)
{
	__isthreaded = 1;
	signal(SIGUSR2, sigusr2handler);
}
