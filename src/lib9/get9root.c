#include <u.h>
#include <libc.h>

char*
get9root(void)
{
	static char *s;

	if(s)
		return s;

	if((s = getenv("PLAN9")) != 0)
		return s;
	/* could do better - search $PATH */
	s = "/usr/local/plan9";
	return s;
}
