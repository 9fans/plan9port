#include <u.h>
#include <libc.h>
#include <thread.h>

int
chanprint(Channel *c, char *fmt, ...)
{
	va_list arg;
	char *p;
	int n;

	va_start(arg, fmt);
	p = vsmprint(fmt, arg);
	va_end(arg);
	if(p == nil)
		sysfatal("vsmprint failed: %r");
	n = sendp(c, p);
	yield();	/* let recipient handle message immediately */
	return n;
}
