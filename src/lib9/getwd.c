#include <u.h>
#include <libc.h>

#undef getwd

char*
p9getwd(char *s, int ns)
{
	return getcwd(s, ns);
}
