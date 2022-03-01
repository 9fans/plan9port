#include <u.h>
#include <libc.h>
#include <regexp.h>
#include <thread.h>
#include <fcall.h>

int debug;
int dfd;
int srvfd;
int netfd[2];
int srv_to_net[2];
int net_to_srv[2];
char *srv;
char	*addr;
char *ns;
int export;

void	shuffle(void *arg);
int	post(char *srv);
void	remoteside(void*);
int	call(char *rsys, char *ns, char *srv);
void*	emalloc(int size);
void	localside(void*);

char *REXEXEC = "ssh";
char *prog = "import";

enum
{
	Stack= 32*1024
};

void
usage(void)
{
	fprint(2, "usage: %s [-df] [-s service] [-n remote-ns] [-p remote-prog] remote-system\n", argv0);
	threadexitsall("usage");
}

void
fatal(char *fmt, ...)
{
	char buf[256];
	va_list arg;

	va_start(arg, fmt);
	vseprint(buf, buf+sizeof buf, fmt, arg);
	va_end(arg);

	fprint(2, "%s: %s\n", argv0 ? argv0 : "<prog>", buf);
	threadexitsall("fatal");
}

int
threadmaybackground(void)
{
	return 1;
}

void
threadmain(int argc, char *argv[])
{
	int dofork;
	int rem;
	void (*fn)(void*);

	dofork = 1;
	rem = 0;
	ns = nil;
	srv = "plumb";

	ARGBEGIN{
	case 'd':
		debug = 1;
		break;
	case 'f':
		dofork = 0;
		break;
	case 'n':	/* name of remote namespace */
		ns = EARGF(usage());
		break;
	case 'p':
		prog = EARGF(usage());
		break;
	case 's':	/* name of service */
		srv = EARGF(usage());
		break;
	case 'R':
		rem = 1;
		break;
	case 'x':
		export = 1;
		break;
	}ARGEND

	if(debug){
		char *dbgfile;

		if(rem)
			dbgfile = smprint("/tmp/%s.export.debug", getuser());
		else
			dbgfile = smprint("/tmp/%s.import.debug", getuser());
		dfd = create(dbgfile, OWRITE, 0664);
		free(dbgfile);
		fmtinstall('F', fcallfmt);
	}


	if(rem){
		netfd[0] = 0;
		netfd[1] = 1;
		write(1, "OK", 2);
	}else{
		if(argc != 1)
			usage();
		addr = argv[0];
		/* connect to remote service */
		netfd[0] = netfd[1] = call(addr, ns, srv);
	}

	fn = localside;
	if(rem+export == 1)
		fn = remoteside;

	if(rem || !dofork)
		fn(nil);
	else
		proccreate(fn, nil, Stack);
}


void
localside(void *arg)
{
	USED(arg);

	/* start a loal service */
	srvfd = post(srv);

	/* threads to shuffle messages each way */
	srv_to_net[0] = srvfd;
	srv_to_net[1] = netfd[1];
	proccreate(shuffle, srv_to_net, Stack);
	net_to_srv[0] = netfd[0];
	net_to_srv[1] = srvfd;
	shuffle(net_to_srv);
}

/* post a local service */
int
post(char *srv)
{
	int p[2];

	if(pipe(p) < 0)
		fatal("can't create pipe: %r");

	/* 0 will be server end, 1 will be client end */
	if(post9pservice(p[1], srv, nil) < 0)
		fatal("post9pservice plumb: %r");
	close(p[1]);

	return p[0];
}

/* start a stub on the remote server */
int
call(char *rsys, char *ns, char *srv)
{
	int p[2];
	int ac;
	char *av[12];
	char buf[2];

	if(pipe(p) < 0)
		fatal("can't create pipe: %r");
	ac = 0;
	av[ac++] = REXEXEC;
	av[ac++] = rsys;
	av[ac++] = prog;
	if(debug)
		av[ac++] = "-d";
	av[ac++] = "-R";
	if(ns != nil){
		av[ac++] = "-n";
		av[ac++] = ns;
	}
	av[ac++] = "-s";
	av[ac++] = srv;
	if(export)
		av[ac++] = "-x";
	av[ac] = 0;

	if(debug){
		fprint(dfd, "execing ");
		for(ac = 0; av[ac]; ac++)
			fprint(dfd, " %s", av[ac]);
		fprint(dfd, "\n");
	}

	switch(fork()){
	case -1:
		fatal("%r");
	case 0:
		dup(p[1], 0);
		dup(p[1], 1);
		close(p[0]);
		close(p[1]);
		execvp(REXEXEC, av);
		fatal("can't exec %s", REXEXEC);
	default:
		break;
	}
	close(p[1]);

	/* ignore crap that might come out of the .profile */
	/* keep reading till we have an "OK" */
	if(read(p[0], &buf[0], 1) != 1)
		fatal("EOF");
	for(;;){
		if(read(p[0], &buf[1], 1) != 1)
			fatal("EOF");
		if(strncmp(buf, "OK", 2) == 0)
			break;
		buf[0] = buf[1];
	}
	if(debug)
		fprint(dfd, "got OK\n");

	return p[0];
}

enum
{
	BLEN=16*1024
};

void
shuffle(void *arg)
{
	int *fd;
	char *buf, *tbuf;
	int n;
	Fcall *t;

	fd = (int*)arg;
	buf = emalloc(BLEN+1);
	t = nil;
	tbuf = nil;
	for(;;){
		n = read9pmsg(fd[0], buf, BLEN);
		if(n <= 0){
			if(debug)
				fprint(dfd, "%d->%d read returns %d: %r\n", fd[0], fd[1], n);
			break;
		}
		if(debug){
			if(t == nil)
				t = emalloc(sizeof(Fcall));
			if(tbuf == nil)
				tbuf = emalloc(BLEN+1);
			memmove(tbuf, buf, n);	/* because convM2S is destructive */
			if(convM2S((uchar*)tbuf, n, t) != n)
				fprint(dfd, "%d->%d convert error in convM2S", fd[0], fd[1]);
			else
				fprint(dfd, "%d->%d %F\n", fd[0], fd[1], t);
		}
		if(write(fd[1], buf, n) != n)
			break;
	}
	threadexitsall(0);
}

void
remoteside(void *v)
{
	int srv_to_net[2];
	int net_to_srv[2];
	char *addr;
	int srvfd;

	if(ns == nil)
		ns = getns();

	addr = smprint("unix!%s/%s", ns, srv);
	if(addr == nil)
		fatal("%r");
	if(debug)
		fprint(dfd, "remoteside starting %s\n", addr);

	srvfd = dial(addr, 0, 0, 0);
	if(srvfd < 0)
		fatal("dial %s: %r", addr);
	if(debug)
		fprint(dfd, "remoteside dial %s succeeded\n", addr);
	fcntl(srvfd, F_SETFL, FD_CLOEXEC);

	/* threads to shuffle messages each way */
	srv_to_net[0] = srvfd;
	srv_to_net[1] = netfd[1];
	proccreate(shuffle, srv_to_net, Stack);
	net_to_srv[0] = netfd[0];
	net_to_srv[1] = srvfd;
	shuffle(net_to_srv);

	threadexitsall(0);
}

void*
emalloc(int size)
{
	void *x;

	x = malloc(size);
	if(x == nil)
		fatal("allocation fails: %r");
	return x;
}
