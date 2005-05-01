#include "threadimpl.h"

void
makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...)
{
	ulong *sp, *tos;
	va_list arg;

	tos = (ulong*)ucp->uc_stack.ss_sp+ucp->uc_stack.ss_size/sizeof(ulong);
	sp = (ulong*)((ulong)(tos-16) & ~15);
	ucp->mc.pc = (long)func;
	ucp->mc.sp = (long)sp;
	va_start(arg, argc);
	ucp->mc.r3 = va_arg(arg, long);
	va_end(arg);
}

int
getcontext(ucontext_t *uc)
{
	return _getmcontext(&uc->mc);
}

int
setcontext(ucontext_t *uc)
{
	_setmcontext(&uc->mc);
	return 0;
}

int
swapcontext(ucontext_t *oucp, ucontext_t *ucp)
{
	if(getcontext(oucp) == 0)
		setcontext(ucp);
	return 0;
}

