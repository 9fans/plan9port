#include <u.h>
#include <libc.h>
#include "libString.h"

/* append a char array to a String */
String *
s_append(String *to, char *from)
{
	if (to == 0)
		to = s_new();
	if (from == 0)
		return to;
	for(; *from; from++)
		s_putc(to, *from);
	s_terminate(to);
	return to;
}
