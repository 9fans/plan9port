#include <u.h>
#include <libc.h>

int
needsrcquote(int c)
{
	if(c <= ' ' || c >= 0x80)
		return 1;
	if(strchr("`^#*[]=|\\?${}()'<>&;", c))
		return 1;
	return 0;
}
