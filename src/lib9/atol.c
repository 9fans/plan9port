#include <u.h>
#include <libc.h>

long
atol(char *s)
{
	return strtol(s, 0, 0);
}

