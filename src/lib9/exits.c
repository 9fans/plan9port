#include <lib9.h>

void
exits(char *s)
{
	if(s && *s)
		exit(1);
	exit(0);
}

