#include "u.h"
#include "libc.h"

/*
 * first argument (l) is in r3 at entry.
 * r3 contains return value upon return.
 */
int
_tas(int *x)
{
	int     v;
	int	tmp, tmp2, tmp3;

	/*
	 * this __asm__ works with gcc on linux
	 */
	__asm__("\n	sync\n"
	"	li	%1,0\n"
	"	mr	%2,%4		/* &x->val */\n"
	"	lis	%3,0xdead	/* assemble constant 0xdeaddead */\n"
	"	ori	%3,%3,0xdead	/* \" */\n"
	"tas1:\n"
	"	dcbf	%2,%1	/* cache flush; \"fix for 603x bug\" */\n"
	"	lwarx	%0,%2,%1	/* v = x->val with reservation */\n"
	"	cmp	cr0,0,%0,%1	/* v == 0 */\n"
	"	bne	tas0\n"
	"	stwcx.	%3,%2,%1   /* if (x->val same) x->val = 0xdeaddead */\n"
	"	bne	tas1\n"
	"tas0:\n"
	"	sync\n"
	"	isync\n"
	: "=r" (v), "=&r" (tmp), "=&r"(tmp2), "=&r"(tmp3)
	: "r"  (x)
	: "cr0", "memory"
	);
	switch(v) {
	case 0:		return 0;
	case 0xdeaddead: return 1;
	default:	fprint(2, "tas: corrupted 0x%lux\n", v);
	}
	return 0;
}
