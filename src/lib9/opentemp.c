#include <u.h>
#include <libc.h>

int
opentemp(char *template)
{
	return mkstemp(template);
}

