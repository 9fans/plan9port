#include <u.h>
#include <libc.h>
#include "libString.h"

void
s_putc(String *s, int c)
{
	if(s->ref > 1)
		sysfatal("can't s_putc a shared string");
	if (s->ptr >= s->end)
		s_grow(s, 2);
	*(s->ptr)++ = c;
}
