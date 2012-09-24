#include <u.h>
#include <libc.h>
#include <auth.h>
#include <thread.h>

enum {
	Encnone,
	Encssl,
	Enctls,
};

static char *encprotos[] = {
	[Encnone] =	"clear",
	[Encssl] =	"ssl",
	[Enctls] = 	"tls",
			nil,
};

char		*keyspec = "";
char		*filterp;
char		*ealgs = "rc4_256 sha1";
int		encproto = Encnone;
AuthInfo 	*ai;
int		debug;
int		doauth = 1;
int		timedout;

int	connectez(char*, char*);
void	sysfatal(char*, ...);
void	usage(void);
int	filter(int, char *, char *);

int
catcher(void *v, char *msg)
{
	timedout = 1;
	if(strcmp(msg, "alarm") == 0)
		return 1;
	return 0;
}

static int
lookup(char *s, char *l[])
{
	int i;

	for (i = 0; l[i] != 0; i++)
		if (strcmp(l[i], s) == 0)
			return i;
	return -1;
}

static char*
srvname(char *addr)
{
	int i;

	for(i=0; i<strlen(addr); i++){
		if(addr[i] == '!')
			addr[i] = ':';
	}
	return addr;
}

void
threadmain(int argc, char **argv)
{
	char *mntpt, *srvpost, srvfile[64];
	int fd;

	quotefmtinstall();
	srvpost = nil;
	ARGBEGIN{
	case 'A':
		doauth = 0;
		break;
	case 'd':
		debug++;
		break;
	case 'E':
		if ((encproto = lookup(EARGF(usage()), encprotos)) < 0)
			usage();
		break;
	case 'e':
		ealgs = EARGF(usage());
		if(*ealgs == 0 || strcmp(ealgs, "clear") == 0)
			ealgs = nil;
		break;
	case 'k':
		keyspec = EARGF(usage());
		break;
	case 'p':
		filterp = unsharp("#9/bin/aan");
		break;
	case 's':
		srvpost = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND;

	mntpt = 0;		/* to shut up compiler */
	switch(argc) {
	case 2:
		mntpt = argv[1];
		break;
	case 3:
		mntpt = argv[2];
		break;
	default:
		usage();
	}

	if(encproto != Encnone)
		sysfatal("%s: tls and ssl have not yet been implemented", argv[0]);

	threadnotify(catcher, 1);
	alarm(60*1000);

	fd = connectez(argv[0], argv[1]);

	fprint(fd, "impo %s %s\n", filterp? "aan": "nofilter",
		encprotos[encproto]);

	if (filterp)
		fd = filter(fd, filterp, argv[0]);

	if(srvpost == nil)
		 srvpost = srvname(argv[0]);
	sprint(srvfile, "%s", srvpost);

	if(post9pservice(fd, srvfile, mntpt) < 0)
		sysfatal("can't post %s: %r", argv[1]);
	alarm(0);

	threadexitsall(0);
}

/* the name "connect" is special */
int
connectez(char *system, char *tree)
{
	char buf[ERRMAX], *na;
	int fd, n;
	char *authp;

	na = netmkaddr(system, "tcp", "exportfs");
	threadsetname("dial %s", na);
	if((fd = dial(na, nil, nil, nil)) < 0)
		sysfatal("can't dial %s: %r", system);

	if(doauth){
		authp = "p9any";
		threadsetname("auth_proxy auth_getkey proto=%q role=client %s",
			authp, keyspec);
		ai = auth_proxy(fd, auth_getkey, "proto=%q role=client %s",
			authp, keyspec);
		if(ai == nil)
			sysfatal("%r: %s", system);
	}

	threadsetname("writing tree name %s", tree);
	n = write(fd, tree, strlen(tree));
	if(n < 0)
		sysfatal("can't write tree: %r");

	strcpy(buf, "can't read tree");

	threadsetname("awaiting OK for %s", tree);
	n = read(fd, buf, sizeof buf - 1);
	if(n!=2 || buf[0]!='O' || buf[1]!='K'){
		if (timedout)
			sysfatal("timed out connecting to %s", na);
		buf[sizeof buf - 1] = '\0';
		sysfatal("bad remote tree: %s", buf);
	}

	return fd;
}

void
usage(void)
{
	fprint(2, "usage: 9import [-A] [-E clear|ssl|tls] "
"[-e 'crypt auth'|clear] [-k keypattern] [-p] [-s srv] host remotefs [mountpoint]\n");
	threadexitsall("usage");
}

/* Network on fd1, mount driver on fd0 */
int
filter(int fd, char *cmd, char *host)
{
	int p[2], len, argc;
	char newport[256], buf[256], *s;
	char *argv[16], *file, *pbuf;

	if ((len = read(fd, newport, sizeof newport - 1)) < 0)
		sysfatal("filter: cannot write port; %r");
	newport[len] = '\0';

	if ((s = strchr(newport, '!')) == nil)
		sysfatal("filter: illegally formatted port %s", newport);

	strecpy(buf, buf+sizeof buf, netmkaddr(host, "tcp", "0"));
	pbuf = strrchr(buf, '!');
	strecpy(pbuf, buf+sizeof buf, s);

	if(debug)
		fprint(2, "filter: remote port %s\n", newport);

	argc = tokenize(cmd, argv, nelem(argv)-2);
	if (argc == 0)
		sysfatal("filter: empty command");
	argv[argc++] = "-c";
	argv[argc++] = buf;
	argv[argc] = nil;
	file = argv[0];
	if (s = strrchr(argv[0], '/'))
		argv[0] = s+1;

	if(pipe(p) < 0)
		sysfatal("pipe: %r");

	switch(rfork(RFNOWAIT|RFPROC|RFFDG)) {
	case -1:
		sysfatal("rfork record module: %r");
	case 0:
		dup(p[0], 1);
		dup(p[0], 0);
		close(p[0]);
		close(p[1]);
		exec(file, argv);
		sysfatal("exec record module: %r");
	default:
		close(fd);
		close(p[0]);
	}
	return p[1];
}
