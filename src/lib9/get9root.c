#include <u.h>
#include <libc.h>

char*
get9root(void)
{
	char *s;

	if((s = getenv("PLAN9")) != 0)
		return s;
	return "/usr/local/plan9";
}

