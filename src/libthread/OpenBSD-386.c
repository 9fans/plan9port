#include "threadimpl.h"

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

extern int getmcontext(mcontext_t*);
extern int setmcontext(mcontext_t*);

int
getcontext(ucontext_t *uc)
{
	return getmcontext(&uc->uc_mcontext);
}

void
setcontext(ucontext_t *uc)
{
	setmcontext(&uc->uc_mcontext);
}

int
swapcontext(ucontext_t *oucp, ucontext_t *ucp)
{
	if(getcontext(oucp) == 0)
		setcontext(ucp);
	return 0;
}

