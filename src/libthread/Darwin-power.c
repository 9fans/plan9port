#include "threadimpl.h"

void
makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...)
{
	ulong *sp, *tos;
	va_list arg;

	tos = (ulong*)ucp->uc_stack.ss_sp+ucp->uc_stack.ss_size/sizeof(ulong);
	sp = tos - 16;	
	ucp->label.pc = (long)func;
	ucp->label.sp = (long)sp;
	va_start(arg, argc);
	ucp->label.r3 = va_arg(arg, long);
	va_end(arg);
}

void
getcontext(ucontext_t *uc)
{
	return __setlabel(uc);
}

int
setcontext(ucontext_t *uc)
{
	return __gotolabel(uc);
}

int
swapcontext(ucontext_t *oucp, ucontext_t *ucp)
{
	if(getcontext(oucp) == 0)
		setcontext(ucp);
	return 0;
}

