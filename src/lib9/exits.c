#include <u.h>
#include <libc.h>

extern void _privdie(void);

void
exits(char *s)
{
	_privdie();
	if(s && *s)
		exit(1);
	exit(0);
}

void
_exits(char *s)
{
	_privdie();
	if(s && *s)
		_exit(1);
	_exit(0);
}
