#include <u.h>
#include <libc.h>

int
atoi(char *s)
{
	return strtol(s, 0, 0);
}
