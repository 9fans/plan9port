#include <u.h>
#include <libc.h>
#include <regexp.h>
#include <thread.h>
#include <plumb.h>
#include <auth.h>
#include <fcall.h>
#include "plumber.h"

char	*plumbfile;
char *user;
char *home;
char *progname;
Ruleset **rules;
int	printerrors=1;
jmp_buf	parsejmp;
char	*lasterror;

void
makeports(Ruleset *rules[])
{
	int i;

	for(i=0; rules[i]; i++)
		addport(rules[i]->port);
}

void
mainproc(void *v)
{
	Channel *c;

	c = v;
	printerrors = 0;
	makeports(rules);
	startfsys();
	sendp(c, nil);
}

void
threadmain(int argc, char *argv[])
{
	char buf[512];
	int fd;
	Channel *c;

	progname = "plumber";

	ARGBEGIN{
	case 'p':
		plumbfile = ARGF();
		break;
	}ARGEND

	user = getenv("user");
	home = getenv("home");
	if(user==nil || home==nil)
		error("can't initialize $user or $home: %r");
	if(plumbfile == nil){
		sprint(buf, "%s/lib/plumbing", home);
		plumbfile = estrdup(buf);
	}

	fd = open(plumbfile, OREAD);
	if(fd < 0)
		error("can't open rules file %s: %r", plumbfile);
	if(setjmp(parsejmp))
		error("parse error");

	rules = readrules(plumbfile, fd);
	close(fd);

	/*
	 * Start all processes and threads from other proc
	 * so we (main pid) can return to user.
	 */
	c = chancreate(sizeof(void*), 0);
	proccreate(mainproc, c, 8192);
	recvp(c);
	chanfree(c);
	threadexits(nil);
}

void
error(char *fmt, ...)
{
	char buf[512];
	va_list args;

	va_start(args, fmt);
	vseprint(buf, buf+sizeof buf, fmt, args);
	va_end(args);

	fprint(2, "%s: %s\n", progname, buf);
	threadexitsall("error");
}

void
parseerror(char *fmt, ...)
{
	char buf[512];
	va_list args;

	va_start(args, fmt);
	vseprint(buf, buf+sizeof buf, fmt, args);
	va_end(args);

	if(printerrors){
		printinputstack();
		fprint(2, "%s\n", buf);
	}
	do; while(popinput());
	lasterror = estrdup(buf);
	longjmp(parsejmp, 1);
}

void*
emalloc(long n)
{
	void *p;

	p = malloc(n);
	if(p == nil)
		error("malloc failed: %r");
	memset(p, 0, n);
	return p;
}

void*
erealloc(void *p, long n)
{
	p = realloc(p, n);
	if(p == nil)
		error("realloc failed: %r");
	return p;
}

char*
estrdup(char *s)
{
	char *t;

	t = strdup(s);
	if(t == nil)
		error("estrdup failed: %r");
	return t;
}
