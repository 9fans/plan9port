#include "a.h"

void*
emalloc(int n)
{
	void *v;

	v = mallocz(n, 1);
	if(v == nil)
		sysfatal("out of memory");
	return v;
}

void*
erealloc(void *v, int n)
{
	v = realloc(v, n);
	if(v == nil)
		sysfatal("out of memory");
	return v;
}

char*
estrdup(char *s)
{
	s = strdup(s);
	if(s == nil)
		sysfatal("out of memory");
	return s;
}

int
timefmt(Fmt *f)
{
	Tm tm;
	vlong ms;

	ms = nsec()/1000000;

	tm = *localtime(ms/1000);
	fmtprint(f, "%02d:%02d:%02d.%03d",
		tm.hour, tm.min, tm.sec,
		(int)(ms%1000));
	return 0;
}

int
writen(int fd, void *buf, int n)
{
	long m, tot;

	for(tot=0; tot<n; tot+=m){
		m = n - tot;
		if(m > 8192)
			m = 8192;
		if(write(fd, (uchar*)buf+tot, m) != m)
			break;
	}
	return tot;
}

int
urlencodefmt(Fmt *fmt)
{
	int x;
	char *s;

	s = va_arg(fmt->args, char*);
	for(; *s; s++){
		x = (uchar)*s;
		if(x == ' ')
			fmtrune(fmt, '+');
		else if(('a' <= x && x <= 'z') || ('A' <= x && x <= 'Z') || ('0' <= x && x <= '9')
			|| strchr("$-_.+!*'()", x)){
			fmtrune(fmt, x);
		}else
			fmtprint(fmt, "%%%02ux", x);
	}
	return 0;
}
