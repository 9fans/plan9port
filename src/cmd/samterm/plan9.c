#include <u.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <cursor.h>
#include <keyboard.h>
#include <frame.h>
#define Tversion Tversion9p
#define Twrite Twrite9p
#include <fcall.h>
#undef Tversion
#undef Twrite
#include <9pclient.h>
#include <plumb.h>
#include "flayer.h"
#include "samterm.h"

static char *exname;

#define STACK 16384

void
usage(void)
{
	fprint(2, "usage: samterm -a -W winsize\n");
	threadexitsall("usage");
}

void
getscreen(int argc, char **argv)
{
	char *t;

	ARGBEGIN{
	case 'a':
		autoindent = 1;
		break;
	case 'W':
		winsize = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	if(initdraw(panic1, nil, "sam") < 0){
		fprint(2, "samterm: initdraw: %r\n");
		threadexitsall("init");
	}
	t = getenv("tabstop");
	if(t != nil){
		maxtab = strtoul(t, nil, 0);
		free(t);
	}
	draw(screen, screen->clipr, display->white, nil, ZP);
}

int
screensize(int *w, int *h)
{
	int fd, n;
	char buf[5*12+1];

	fd = open("/dev/screen", OREAD);
	if(fd < 0)
		return 0;
	n = read(fd, buf, sizeof(buf)-1);
	close(fd);
	if (n != sizeof(buf)-1)
		return 0;
	buf[n] = 0;
	if (h) {
		*h = atoi(buf+4*12)-atoi(buf+2*12);
		if (*h < 0)
			return 0;
	}
	if (w) {
		*w = atoi(buf+3*12)-atoi(buf+1*12);
		if (*w < 0)
			return 0;
	}
	return 1;
}

int
snarfswap(char *fromsam, int nc, char **tosam)
{
	char *s;

	s = getsnarf();
	putsnarf(fromsam);
	*tosam = s;
	return s ? strlen(s) : 0;
}

void
dumperrmsg(int count, int type, int count0, int c)
{
	fprint(2, "samterm: host mesg: count %d %ux %ux %ux %s...ignored\n",
		count, type, count0, c, rcvstring());
}

void
removeextern(void)
{
	remove(exname);
}

Readbuf	hostbuf[2];
Readbuf	plumbbuf[2];

void
extproc(void *argv)
{
	Channel *c;
	int i, n, which, fd;
	void **arg;

	arg = argv;
	c = arg[0];
	fd = (int)(uintptr)arg[1];

	i = 0;
	for(;;){
		i = 1-i;	/* toggle */
		n = read(fd, plumbbuf[i].data, sizeof plumbbuf[i].data);
if(0) fprint(2, "ext %d\n", n);
		if(n <= 0){
			fprint(2, "samterm: extern read error: %r\n");
			threadexits("extern");	/* not a fatal error */
		}
		plumbbuf[i].n = n;
		which = i;
		send(c, &which);
	}
}

void
extstart(void)
{
	char *user, *disp;
	int fd, flags;
	static void *arg[2];

	user = getenv("USER");
	if(user == nil)
		return;
	disp = getenv("DISPLAY");
	if(disp){
		exname = smprint("/tmp/.sam.%s.%s", user, disp);
		free(disp);
	}
	else
		exname = smprint("/tmp/.sam.%s", user);
	free(user);
	if(exname == nil){
		fprint(2, "not posting for B: out of memory\n");
		return;
	}

	if(mkfifo(exname, 0600) < 0){
		struct stat st;
		if(errno != EEXIST || stat(exname, &st) < 0)
			return;
		if(!S_ISFIFO(st.st_mode)){
			removeextern();
			if(mkfifo(exname, 0600) < 0)
				return;
		}
	}

	fd = open(exname, OREAD|ONONBLOCK);
	if(fd == -1){
		removeextern();
		return;
	}

	/*
	 * Turn off no-delay and provide ourselves as a lingering
	 * writer so as not to get end of file on read.
	 */
	flags = fcntl(fd, F_GETFL, 0);
	if(flags<0 || fcntl(fd, F_SETFL, flags&~O_NONBLOCK)<0
	||open(exname, OWRITE) < 0){
		close(fd);
		removeextern();
		return;
	}

	plumbc = chancreate(sizeof(int), 0);
	chansetname(plumbc, "plumbc");
	arg[0] = plumbc;
	arg[1] = (void*)(uintptr)fd;
	proccreate(extproc, arg, STACK);
	atexit(removeextern);
}

int
plumbformat(Plumbmsg *m, int i)
{
	char *addr, *data, *act;
	int n;

	data = (char*)plumbbuf[i].data;
	n = m->ndata;
	if(n == 0 || 2+n+2 >= READBUFSIZE){
		plumbfree(m);
		return 0;
	}
	act = plumblookup(m->attr, "action");
	if(act!=nil && strcmp(act, "showfile")!=0){
		/* can't handle other cases yet */
		plumbfree(m);
		return 0;
	}
	addr = plumblookup(m->attr, "addr");
	if(addr){
		if(addr[0] == '\0')
			addr = nil;
		else
			addr = strdup(addr);	/* copy to safe storage; we'll overwrite data */
	}
	memmove(data, "B ", 2);	/* we know there's enough room for this */
	memmove(data+2, m->data, n);
	n += 2;
	if(data[n-1] != '\n')
		data[n++] = '\n';
	if(addr != nil){
		if(n+strlen(addr)+1+1 <= READBUFSIZE)
			n += sprint(data+n, "%s\n", addr);
		free(addr);
	}
	plumbbuf[i].n = n;
	plumbfree(m);
	return 1;
}

void
plumbproc(void *arg)
{
	CFid *fid;
	int i;
	Plumbmsg *m;

	fid = arg;
	i = 0;
	for(;;){
		m = plumbrecvfid(fid);
		if(m == nil){
			fprint(2, "samterm: plumb read error: %r\n");
			threadexits("plumb");	/* not a fatal error */
		}
		if(plumbformat(m, i)){
			send(plumbc, &i);
			i = 1-i;	/* toggle */
		}
	}
}

int
plumbstart(void)
{
	CFid *fid;

	plumbfd = plumbopen("send", OWRITE|OCEXEC);	/* not open is ok */
	fid = plumbopenfid("edit", OREAD|OCEXEC);
	if(fid == nil)
		return -1;
	plumbc = chancreate(sizeof(int), 0);
	chansetname(plumbc, "plumbc");
	if(plumbc == nil){
		fsclose(fid);
		return -1;
	}
	threadcreate(plumbproc, fid, STACK);
	return 1;
}

void
hostproc(void *arg)
{
	Channel *c;
	int i, n, which;

	c = arg;

	i = 0;
	for(;;){
		i = 1-i;	/* toggle */
		n = read(hostfd[0], hostbuf[i].data, sizeof hostbuf[i].data);
if(0) fprint(2, "hostproc %d\n", n);
		if(n <= 0){
			if(n == 0){
				if(exiting)
					threadexits(nil);
				werrstr("unexpected eof");
			}
			fprint(2, "samterm: host read error: %r\n");
			threadexitsall("host");
		}
		hostbuf[i].n = n;
		which = i;
if(0) fprint(2, "hostproc send %d\n", which);
		send(c, &which);
	}
}

void
hoststart(void)
{
	hostc = chancreate(sizeof(int), 0);
	chansetname(hostc, "hostc");
	proccreate(hostproc, hostc, STACK);
}
