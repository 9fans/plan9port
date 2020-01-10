#include <u.h>
#include <libc.h>
#include <venti.h>

void
vtdebug(VtConn *z, char *fmt, ...)
{
	va_list arg;

	if(z->debug == 0)
		return;

	va_start(arg, fmt);
	vfprint(2, fmt, arg);
	va_end(arg);
}
