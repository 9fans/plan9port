#include "rc.h"
#include "io.h"
#include "fns.h"

void *
emalloc(long n)
{
	void *p = malloc(n);
	if(p==0)
		panic("Can't malloc %d bytes", n);
	return p;
}

void*
erealloc(void *p, long n)
{
	p = realloc(p, n);
	if(p==0 && n!=0)
		panic("Can't realloc %d bytes\n", n);
	return p;
}

char*
estrdup(char *s)
{
	int n = strlen(s)+1;
	char *d = emalloc(n);
	memmove(d, s, n);
	return d;
}

void
pfln(io *fd, char *file, int line)
{
	if(file && line)
		pfmt(fd, "%s:%d", file, line);
	else if(file)
		pstr(fd, file);
	else
		pstr(fd, argv0);
}

static char *bp;

static void
iacvt(int n)
{
	if(n<0){
		*bp++='-';
		n=-n;	/* doesn't work for n==-inf */
	}
	if(n/10)
		iacvt(n/10);
	*bp++=n%10+'0';
}

void
inttoascii(char *s, int n)
{
	bp = s;
	iacvt(n);
	*bp='\0';
}

void
panic(char *s, int n)
{
	pfmt(err, "%s: ", argv0);
	pfmt(err, s, n);
	pchr(err, '\n');
	flushio(err);

	Abort();
}
