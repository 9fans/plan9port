#include "../threadimpl.h"
#undef exits


ucontext_t c0, c1;
char stack[65536];

void
go(void *v)
{
	print("hello, world\n");
	setcontext(&c0);
}

void
main(void)
{
//	print("in main\n");
	getcontext(&c1);
	c1.uc_stack.ss_sp = stack;
	c1.uc_stack.ss_size = sizeof stack;
	makecontext(&c1, go, 1, 0);
	if(getcontext(&c0) == 0)
		setcontext(&c1);
	print("back in main\n");
	exits(0);
}
