#include <lib9.h>

#define	MASK	0x7fffffffL

int
nrand(int n)
{
	long slop, v;

	if(n < 0)
		return n;
	slop = MASK % n;
	do
		v = lrand();
	while(v <= slop);
	return v % n;
}
