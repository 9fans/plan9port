#include <u.h>
#include <libc.h>
#include <bio.h>
#include <libsec.h>

#include "iso9660.h"

Rune*
strtorune(Rune *r, char *s)
{
	Rune *or;

	if(s == nil)
		return nil;

	or = r;
	while(*s)
		s += chartorune(r++, s);
	*r = L'\0';
	return or;
}

Rune*
runechr(Rune *s, Rune c)
{
	for(; *s; s++)
		if(*s == c)
			return s;
	return nil;
}

int
runecmp(Rune *s, Rune *t)
{
	while(*s && *t && *s == *t)
		s++, t++;
	return *s - *t;
}
