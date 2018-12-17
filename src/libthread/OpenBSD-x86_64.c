#include "threadimpl.h"
#include <sys/mman.h>

#define MAX(a, b) \
	((a) > (b) ? (a) : (b))

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

void*
stkalloc(ulong sz)
{
	void *p;
	ulong *m;

	p = mmap(NULL, sz,
	    PROT_READ|PROT_WRITE,
	    MAP_PRIVATE|MAP_ANON|MAP_STACK,
	    -1, 0);
	if (p == (void*)-1)
		return p;
	m = p;
	m[0] = sz;
	return (char*)p + MAX(sizeof(ulong), 16);
}

void
stkfree(void *p)
{
	ulong *m = (ulong*)((char*)p - MAX(sizeof(ulong), 16));
	ulong sz = m[0];
	munmap(p, sz);
}

