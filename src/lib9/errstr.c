/*
 * We assume there's only one error buffer for the whole system.
 * If you use ffork, you need to provide a _syserrstr.  Since most
 * people will use libthread (which provides a _syserrstr), this is
 * okay.
 */

#include <u.h>
#include <errno.h>
#include <string.h>
#include <libc.h>

enum
{
	EPLAN9 = 0x19283745,
};

char *(*_syserrstr)(void);
static char xsyserr[ERRMAX];
static char*
getsyserr(void)
{
	char *s;

	s = nil;
	if(_syserrstr)
		s = (*_syserrstr)();
	if(s == nil)
		s = xsyserr;
	return s;
}

int
errstr(char *err, uint n)
{
	char tmp[ERRMAX];
	char *syserr;

	syserr = getsyserr();
	if(errno != EPLAN9)
		strcpy(syserr, strerror(errno));

	strecpy(tmp, tmp+ERRMAX, syserr);
	strecpy(syserr, syserr+ERRMAX, err);
	strecpy(err, err+n, tmp);
	errno = EPLAN9;
	return 0;
}

void
rerrstr(char *err, uint n)
{
	char *syserr;

	syserr = getsyserr();
	if(errno != EPLAN9)
		strcpy(syserr, strerror(errno));
	strecpy(err, err+n, syserr);
}

/* replaces __errfmt in libfmt */

int
__errfmt(Fmt *f)
{
	if(errno == EPLAN9)
		return fmtstrcpy(f, getsyserr());
	return fmtstrcpy(f, strerror(errno));
}

void
werrstr(char *fmt, ...)
{
	va_list arg;
	char buf[ERRMAX];

	va_start(arg, fmt);
	vseprint(buf, buf+ERRMAX, fmt, arg);
	va_end(arg);
	errstr(buf, ERRMAX);
}

char*
gerrstr(void)
{
	char *s;

	s = getsyserr();
	if(errno != EPLAN9)
		strcpy(s, strerror(errno));
	return s;
}


