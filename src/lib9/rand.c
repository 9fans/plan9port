#include	<lib9.h>

int
rand(void)
{
	return lrand() & 0x7fff;
}

