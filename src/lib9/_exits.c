#include <lib9.h>

void
_exits(char *s)
{
	if(s && *s)
		_exit(1);
	_exit(0);
}
