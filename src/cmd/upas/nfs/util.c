#include "a.h"

void
warn(char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	fprint(2, "warning: ");
	vfprint(2, fmt, arg);
	fprint(2, "\n");
	va_end(arg);
}
