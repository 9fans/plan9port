#include "threadimpl.h"

void
makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...)
{
	ulong *sp, *tos;
	va_list arg;

	if(argc != 2)
		sysfatal("libthread: makecontext misused");
	sp = USPALIGN(ucp, 16);
	ucp->mc.pc = (long)func;
	ucp->mc.sp = (long)sp;
	va_start(arg, argc);
	ucp->mc.r3 = va_arg(arg, long);
	ucp->mc.r4 = va_arg(arg, long);
	va_end(arg);
}

int
swapcontext(ucontext_t *oucp, ucontext_t *ucp)
{
	if(getcontext(oucp) == 0)
		setcontext(ucp);
	return 0;
}
