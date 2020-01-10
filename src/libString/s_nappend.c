#include <u.h>
#include <libc.h>
#include "libString.h"

/* append a char array ( of up to n characters) to a String */
String *
s_nappend(String *to, char *from, int n)
{
	if (to == 0)
		to = s_new();
	if (from == 0)
		return to;
	for(; n && *from; from++, n--)
		s_putc(to, *from);
	s_terminate(to);
	return to;
}
