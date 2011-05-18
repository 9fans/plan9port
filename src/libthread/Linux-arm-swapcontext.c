#include "threadimpl.h"

void
makecontext(ucontext_t *uc, void (*fn)(void), int argc, ...)
{
	int i, *sp;
	va_list arg;

	sp = (int*)uc->uc_stack.ss_sp+uc->uc_stack.ss_size/4;
	va_start(arg, argc);
	for(i=0; i<4 && i<argc; i++)
		(&uc->uc_mcontext.arm_r0)[i] = va_arg(arg, uint);
	va_end(arg);
	uc->uc_mcontext.arm_sp = (uint)sp;
	uc->uc_mcontext.arm_lr = (uint)fn;
}

int
swapcontext(ucontext_t *oucp, const ucontext_t *ucp)
{
	if(getcontext(oucp) == 0)
		setcontext(ucp);
	return 0;
}


