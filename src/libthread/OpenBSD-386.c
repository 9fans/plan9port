#include "threadimpl.h"
#include <sys/mman.h>

#define MAX(a, b) \
	((a) > (b) ? (a) : (b))

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
	ulong *m = (char*)p - MAX(sizeof(ulong), 16);
	ulong sz = m[0];
	munmap(p, sz);
}

