#include "stdinc.h"
#include "9.h"

/*
 * To do: This will become something else ('vprint'?).
 */
int
consVPrint(char* fmt, va_list args)
{
	int len, ret;
	char buf[256];

	len = vsnprint(buf, sizeof(buf), fmt, args);
	ret = consWrite(buf, len);

	while (len-- > 0 && buf[len] == '\n')
		buf[len] = '\0';
	/*
	 * if we do this, checking the root fossil (if /sys/log/fossil is there)
	 * will spew all over the console.
	 */
	if (0)
		syslog(0, "fossil", "%s", buf);
	return ret;
}

/*
 * To do: This will become 'print'.
 */
int
consPrint(char* fmt, ...)
{
	int ret;
	va_list args;

	va_start(args, fmt);
	ret = consVPrint(fmt, args);
	va_end(args);
	return ret;
}
