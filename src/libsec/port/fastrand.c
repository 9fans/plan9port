#include	<u.h>
#include	<libc.h>
#include	<libsec.h>

/* 
 *  use the X917 random number generator to create random
 *  numbers (faster than truerand() but not as random).
 */
ulong
fastrand(void)
{
	ulong x;
	
	genrandom((uchar*)&x, sizeof x);
	return x;
}
