#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>
#include <plumb.h>
#include <9pclient.h>
#include "dat.h"

void*
emalloc(uint n)
{
	void *p;

	p = malloc(n);
	if(p == nil)
		error("can't malloc: %r");
	memset(p, 0, n);
	setmalloctag(p, getcallerpc(&n));
	return p;
}

void*
erealloc(void *p, uint n)
{
	p = realloc(p, n);
	if(p == nil)
		error("can't realloc: %r");
	setmalloctag(p, getcallerpc(&n));
	return p;
}

char*
estrdup(char *s)
{
	char *t;

	t = emalloc(strlen(s)+1);
	strcpy(t, s);
	return t;
}

char*
estrstrdup(char *s, char *t)
{
	char *u;

	u = emalloc(strlen(s)+strlen(t)+1);
	strcpy(u, s);
	strcat(u, t);
	return u;
}

char*
eappend(char *s, char *sep, char *t)
{
	char *u;

	if(t == nil)
		u = estrstrdup(s, sep);
	else{
		u = emalloc(strlen(s)+strlen(sep)+strlen(t)+1);
		strcpy(u, s);
		strcat(u, sep);
		strcat(u, t);
	}
	free(s);
	return u;
}

char*
egrow(char *s, char *sep, char *t)
{
	s = eappend(s, sep, t);
	free(t);
	return s;
}

void
error(char *fmt, ...)
{
	Fmt f;
	char buf[64];
	va_list arg;

	fmtfdinit(&f, 2, buf, sizeof buf);
	fmtprint(&f, "Mail: ");
	va_start(arg, fmt);
	fmtvprint(&f, fmt, arg);
	va_end(arg);
	fmtprint(&f, "\n");
	fmtfdflush(&f);
	threadexitsall(fmt);
}

#if 0 /* jpc */
void
ctlprint(int fd, char *fmt, ...)
{
	int n;
	va_list arg;

	va_start(arg, fmt);
	n = vfprint(fd, fmt, arg);
	va_end(arg);
	fsync(fd);
	if(n <= 0)
		error("control file write error: %r");
}
#endif

void
ctlprint(CFid* fd, char *fmt, ...)
{
	int n;
	va_list arg;
	char tmp[250];

	va_start(arg, fmt);
	n = vsnprint(tmp, 250, fmt, arg);
	va_end(arg);
	n = fswrite(fd, tmp, strlen(tmp));
	if(n <= 0)
		error("control file write error: %r");
}

int fsprint(CFid *fid, char* fmt, ...) {
	// example call this replaces:  Bprint(b, ">%s%s\n", lines[i][0]=='>'? "" : " ", lines[i]);
	char *tmp;
	va_list arg;
	int n, tlen;

	tmp = emalloc( tlen=(strlen(fmt)+250) );  // leave room for interpolated text
	va_start(arg, fmt);
	n = vsnprint(tmp, tlen, fmt, arg);
	va_end(arg);
	if(n == tlen)
		error("fsprint formatting error");
	n = fswrite(fid, tmp, strlen(tmp));
	if(n <= 0)
		error("fsprint write error: %r");
	free(tmp);

	return n;

}
#if 0 /* jpc */
/*
here's a read construct (from winselection) that may be useful in fsprint - think about it.
*/
	int m, n;
	char *buf;
	char tmp[256];
	CFid* fid;

	fid = winopenfid1(w, "rdsel", OREAD);
	if(fid == nil)
		error("can't open rdsel: %r");
	n = 0;
	buf = nil;

	for(;;){
		m = fsread(fid, tmp, sizeof tmp);
		if(m <= 0)
			break;
		buf = erealloc(buf, n+m+1);
		memmove(buf+n, tmp, m);
		n += m;
		buf[n] = '\0';
	}
#endif
