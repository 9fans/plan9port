#include <u.h>
#include <libc.h>
#include <bio.h>
#include "libString.h"

/* Append an input line to a String.
 *
 * Returns a pointer to the character string (or 0).
 * Trailing newline is left on.
 */
extern char *
s_read_line(Biobuf *fp, String *to)
{
	char *cp;
	int llen;

	if(to->ref > 1)
		sysfatal("can't s_read_line a shared string");
	s_terminate(to);
	cp = Brdline(fp, '\n');
	if(cp == 0)
		return 0;
	llen = Blinelen(fp);
	if(to->end - to->ptr < llen)
		s_grow(to, llen);
	memmove(to->ptr, cp, llen);
	cp = to->ptr;
	to->ptr += llen;
	s_terminate(to);
	return cp;
}
