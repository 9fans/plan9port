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

int
p9putenv(char *s, char *v)
{
	return setenv(s, v, 1);
}
