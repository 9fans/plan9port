#include <u.h>
#include <libc.h>
#include <venti.h>

char*
vtstrdup(char *s)
{
	int n;
	char *ss;

	if(s == nil)
		return nil;
	n = strlen(s) + 1;
	ss = vtmalloc(n);
	memmove(ss, s, n);
	return ss;
}
