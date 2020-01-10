#include <u.h>
#include <libc.h>
#include <mach.h>
#include "macho.h"

#if 0
static int
machosyminit(Fhdr *fp)
{
	/* XXX should parse dynamic symbol table here */
	return 0;
}
#endif

int
symmacho(Fhdr *fp)
{
	int ret;
	Macho *m;

	m = fp->macho;
	if(m == nil){
		werrstr("not a macho");
		return -1;
	}

	ret = -1;

	if(machdebug)
		fprint(2, "macho symbols...\n");

/*
	if(machosyminit(fp) < 0)
		fprint(2, "initializing macho symbols: %r\n");
	else
		ret = 0;
*/

	if(fp->stabs.stabbase){
		if(machdebug)
			fprint(2, "stabs symbols...\n");

		if(symstabs(fp) < 0)
			fprint(2, "initializing stabs: %r");
		else
			ret = 0;
	}
	return ret;
}
