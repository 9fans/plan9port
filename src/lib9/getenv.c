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
	char *t;

	t = smprint("%s=%s", s, v);
	if(t == nil)
		return -1;
	putenv(t);
	return 0;
}
