#include "threadimpl.h"

void
makecontext(ucontext_t *uc, void (*fn)(void), int argc, ...)
{
	uintptr *sp;
	va_list arg;

//fprint(2, "makecontext %d\n", argc);
	if(argc != 2)
		sysfatal("libthread: makecontext misused");
	va_start(arg, argc);
	uc->mc.di = va_arg(arg, uint);
	uc->mc.si = va_arg(arg, uint);
//fprint(2, "%ux %ux\n", uc->mc.di, uc->mc.si);
	va_end(arg);

	sp = (uintptr*)((char*)uc->uc_stack.ss_sp+uc->uc_stack.ss_size);
	*--sp = 0;  // fn's return address
	*--sp = (uintptr)fn;  // return address of setcontext
	uc->mc.sp = (uintptr)sp;
}

int
swapcontext(ucontext_t *oucp, ucontext_t *ucp)
{
	if(getcontext(oucp) == 0)
		setcontext(ucp);
	return 0;
}


