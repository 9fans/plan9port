#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

char*
p9getenv(char *s)
{
	char *t;

	t = getenv(s);
	if(t == 0)
		return 0;
	return strdup(t);
}

