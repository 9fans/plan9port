#include <u.h>
#include <libc.h>
#include "libString.h"

void
s_terminate(String *s)
{
	if(s->ref > 1)
		sysfatal("can't s_terminate a shared string");
	if (s->ptr >= s->end)
		s_grow(s, 1);
	*s->ptr = 0;
}
