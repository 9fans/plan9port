#include <u.h>
#include <libc.h>
#include "libString.h"

String*
s_reset(String *s)
{
	if(s != nil){
		s = s_unique(s);
		s->ptr = s->base;
		*s->ptr = '\0';
	} else
		s = s_new();
	return s;
}

String*
s_restart(String *s)
{
	s = s_unique(s);
	s->ptr = s->base;
	return s;
}
