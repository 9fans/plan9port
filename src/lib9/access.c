#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

char *_p9translate(char*);

int
p9access(char *xname, int what)
{
	int ret;
	char *name;

	if((name = _p9translate(xname)) == nil)
		return -1;
	ret = access(name, what);
	if(name != xname)
		free(name);
	return ret;
}
