#include <lib9.h>

void (*_sysfatal)(char*, ...);

void
sysfatal(char *fmt, ...)
{
	char buf[256];
	va_list arg;

	va_start(arg, fmt);
	if(_sysfatal)
		(*_sysfatal)(fmt, arg);
	vseprint(buf, buf+sizeof buf, fmt, arg);
	va_end(arg);

	__fixargv0();
	fprint(2, "%s: %s\n", argv0 ? argv0 : "<prog>", buf);
	exits("fatal");
}

