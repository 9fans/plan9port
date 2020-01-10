#include <u.h>
#include <libc.h>
#include <libsec.h>

#define Maxrand	((1UL<<31)-1)

ulong
nfastrand(ulong n)
{
	ulong m, r;

	/*
	 * set m to the maximum multiple of n <= 2^31-1
	 * so we want a random number < m.
	 */
	if(n > Maxrand)
		sysfatal("nfastrand: n too large");

	m = Maxrand - Maxrand % n;
	while((r = fastrand()) >= m)
		;
	return r%n;
}
