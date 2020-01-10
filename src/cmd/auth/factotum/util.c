#include "std.h"
#include "dat.h"

static int
unhex(char c)
{
	if('0' <= c && c <= '9')
		return c-'0';
	if('a' <= c && c <= 'f')
		return c-'a'+10;
	if('A' <= c && c <= 'F')
		return c-'A'+10;
	abort();
	return -1;
}

int
hexparse(char *hex, uchar *dat, int ndat)
{
	int i, n;

	n = strlen(hex);
	if(n%2)
		return -1;
	n /= 2;
	if(n > ndat)
		return -1;
	if(hex[strspn(hex, "0123456789abcdefABCDEF")] != '\0')
		return -1;
	for(i=0; i<n; i++)
		dat[i] = (unhex(hex[2*i])<<4)|unhex(hex[2*i+1]);
	return n;
}

char*
estrappend(char *s, char *fmt, ...)
{
	char *t;
	int l;
	va_list arg;

	va_start(arg, fmt);
	t = vsmprint(fmt, arg);
	if(t == nil)
		sysfatal("out of memory");
	va_end(arg);
	l = s ? strlen(s) : 0;
	s = erealloc(s, l+strlen(t)+1);
	strcpy(s+l, t);
	free(t);
	return s;
}
