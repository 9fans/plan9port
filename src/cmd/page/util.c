#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <bio.h>
#include <cursor.h>
#include "page.h"

void*
emalloc(int sz)
{
	void *v;
	v = malloc(sz);
	if(v == nil) {
		fprint(2, "out of memory allocating %d\n", sz);
		wexits("mem");
	}
	memset(v, 0, sz);
	return v;
}

void*
erealloc(void *v, int sz)
{
	v = realloc(v, sz);
	if(v == nil) {
		fprint(2, "out of memory allocating %d\n", sz);
		wexits("mem");
	}
	return v;
}

char*
estrdup(char *s)
{
	char *t;
	if((t = strdup(s)) == nil) {
		fprint(2, "out of memory in strdup(%.10s)\n", s);
		wexits("mem");
	}
	return t;
}

/*
 * spool standard input to /tmp.
 * we've already read the initial in bytes into ibuf.
 */
int
spooltodisk(uchar *ibuf, int in, char **name)
{
	uchar buf[8192];
	int fd, n;

	strcpy(tempfile, "/tmp/pagespoolXXXXXXXXX");
	fd = opentemp(tempfile, ORDWR);
	if(name)
		*name = estrdup(tempfile);

	if(write(fd, ibuf, in) != in){
		fprint(2, "error writing temporary file\n");
		wexits("write temp");
	}

	while((n = read(stdinfd, buf, sizeof buf)) > 0){
		if(write(fd, buf, n) != n){
			fprint(2, "error writing temporary file\n");
			wexits("write temp0");
		}
	}
	seek(fd, 0, 0);
	return fd;
}

typedef struct StdinArg StdinArg;

struct StdinArg {
	Channel *cp;
	uchar	*ibuf;
	int	in;
};

/*
 * spool standard input into a pipe.
 * we've already ready the first in bytes into ibuf
 */
static void
_stdinpipe(void *a)
{
	uchar buf[8192];
	StdinArg *arg;
	int p[2];
	int n;

	arg = a;

	if(pipe(p) < 0){
		fprint(2, "pipe fails: %r\n");
		wexits("pipe");
	}

	send(arg->cp, &p[0]);

	write(p[1], arg->ibuf, arg->in);
	while((n = read(stdinfd, buf, sizeof buf)) > 0)
		write(p[1], buf, n);

	close(p[1]);
	threadexits(0);
}

int
stdinpipe(uchar *ibuf, int in) {
	StdinArg arg;
	int fd;

	arg.ibuf = ibuf;
	arg.in = in;
	arg.cp = chancreate(sizeof(int), 0);
	proccreate(_stdinpipe, &arg, mainstacksize);
	recv(arg.cp, &fd);
	chanfree(arg.cp);

	return fd;
}
