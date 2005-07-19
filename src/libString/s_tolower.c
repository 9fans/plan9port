#include <u.h>
#include <libc.h>
#include <ctype.h>
#include "libString.h"


/* convert String to lower case */
void
s_tolower(String *sp)
{
	char *cp;

	for(cp=sp->ptr; *cp; cp++)
		*cp = tolower((uchar)*cp);
}
