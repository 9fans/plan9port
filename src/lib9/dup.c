#include <u.h>
#include <libc.h>

#undef dup

int
p9dup(int old, int new)
{
	if(new == -1)
		return dup(old);
	return dup2(old, new);
}
