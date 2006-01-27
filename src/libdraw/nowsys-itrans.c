#include <u.h>
#include <libc.h>
 
static int
bad(void)
{
    sysfatal("compiled with no window system support");
    return 0;
}

void
putsnarf(char *data)
{
	USED(data);
	bad();
}

char*
getsnarf(void)
{
	bad();
	return nil;
}
