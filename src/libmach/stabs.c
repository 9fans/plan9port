#include <u.h>
#include <libc.h>
#include <mach.h>
#include "stabs.h"

/*
http://sources.redhat.com/gdb/onlinedocs/stabs.html
*/

int
stabsym(Stab *stabs, int i, StabSym *sym)
{
	uchar *p;
	ulong x;

	if(stabs == nil){
		werrstr("no stabs");
		return -1;
	}
	if(stabs->e2==0 || stabs->e4==0){
		werrstr("no data extractors");
		return -1;
	}

	if(i >= stabs->stabsize/12){
		werrstr("stabs index out of range");
		return -1;
	}

	p = stabs->stabbase+i*12;
	x = stabs->e4(p);
	if(x == 0)
		sym->name = nil;
	else if(x < stabs->strsize)
		sym->name = stabs->strbase+x;
	else{
		werrstr("bad stabs string index");
		return -1;
	}

	/*
	 * In theory, if name ends with a backslash,
	 * it continues into the next entry.  We could
	 * rewrite these in place and then zero the next
	 * few entries, but let's wait until we run across
	 * some system that generates these.
	 */
	sym->type = p[4];
	sym->other = p[5];
	sym->desc = stabs->e2(p+6);
	sym->value = stabs->e4(p+8);
	return 0;
}
