#include <u.h>
#include <libc.h>
#include "9proc.h"

void
_exits(char *s)
{
	_p9uprocdie();

	if(s && *s)
		_exit(1);
	_exit(0);
}
