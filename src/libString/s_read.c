#include <u.h>
#include <libc.h>
#include <bio.h>
#include "libString.h"

enum
{
	Minread=	256
};

/* Append up to 'len' input bytes to the string 'to'.
 *
 * Returns the number of characters read.
 */
extern int
s_read(Biobuf *fp, String *to, int len)
{
	int rv;
	int n;

	if(to->ref > 1)
		sysfatal("can't s_read a shared string");
	for(rv = 0; rv < len; rv += n){
		n = to->end - to->ptr;
		if(n < Minread){
			s_grow(to, Minread);
			n = to->end - to->ptr;
		}
		if(n > len - rv)
			n = len - rv;
		n = Bread(fp, to->ptr, n);
		if(n <= 0)
			break;
		to->ptr += n;
	}
	s_terminate(to);
	return rv;
}
