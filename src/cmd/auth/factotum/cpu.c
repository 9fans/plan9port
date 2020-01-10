/*
 * cpu.c - Make a connection to a cpu server
 *
 *	   Invoked by listen as 'cpu -R | -N service net netdir'
 *	    	   by users  as 'cpu [-h system] [-c cmd args ...]'
 */

#include <u.h>
#include <libc.h>
#include <bio.h>
#include <auth.h>
#include <fcall.h>
#include <libsec.h>

#define	Maxfdata 8192

void	remoteside(int);
void	fatal(int, char*, ...);
void	lclnoteproc(int);
void	rmtnoteproc(void);
void	catcher(void*, char*);
void	usage(void);
void	writestr(int, char*, char*, int);
int	readstr(int, char*, int);
char	*rexcall(int*, char*, char*);
int	setamalg(char*);

int 	notechan;
char	system[32];
int	cflag;
int	hflag;
int	dbg;
char	*user;

char	*srvname = "ncpu";
char	*exportfs = "/bin/exportfs";
char	*ealgs = "rc4_256 sha1";

/* message size for exportfs; may be larger so we can do big graphics in CPU window */
int	msgsize = 8192+IOHDRSZ;

/* authentication mechanisms */
static int	netkeyauth(int);
static int	netkeysrvauth(int, char*);
static int	p9auth(int);
static int	srvp9auth(int, char*);
static int	noauth(int);
static int	srvnoauth(int, char*);

typedef struct AuthMethod AuthMethod;
struct AuthMethod {
	char	*name;			/* name of method */
	int	(*cf)(int);		/* client side authentication */
	int	(*sf)(int, char*);	/* server side authentication */
} authmethod[] =
{
	{ "p9",		p9auth,		srvp9auth,},
	{ "netkey",	netkeyauth,	netkeysrvauth,},
/*	{ "none",	noauth,		srvnoauth,}, */
	{ nil,	nil}
};
AuthMethod *am = authmethod;	/* default is p9 */

char *p9authproto = "p9any";

int setam(char*);

void
usage(void)
{
	fprint(2, "usage: cpu [-h system] [-a authmethod] [-e 'crypt hash'] [-c cmd args ...]\n");
	exits("usage");
}
int fdd;

void
main(int argc, char **argv)
{
	char dat[128], buf[128], cmd[128], *p, *err;
	int fd, ms, kms, data;

	/* see if we should use a larger message size */
	fd = open("/dev/draw", OREAD);
	if(fd > 0){
		ms = iounit(fd);
		if(ms != 0 && ms < ms+IOHDRSZ)
			msgsize = ms+IOHDRSZ;
		close(fd);
	}
	kms = kiounit();
	if(msgsize > kms-IOHDRSZ-100)	/* 100 for network packets, etc. */
		msgsize = kms-IOHDRSZ-100;

	user = getuser();
	if(user == nil)
		fatal(1, "can't read user name");
	ARGBEGIN{
	case 'a':
		p = EARGF(usage());
		if(setam(p) < 0)
			fatal(0, "unknown auth method %s", p);
		break;
	case 'e':
		ealgs = EARGF(usage());
		if(*ealgs == 0 || strcmp(ealgs, "clear") == 0)
			ealgs = nil;
		break;
	case 'd':
		dbg++;
		break;
	case 'f':
		/* ignored but accepted for compatibility */
		break;
	case 'O':
		p9authproto = "p9sk2";
		remoteside(1);				/* From listen */
		break;
	case 'R':				/* From listen */
		remoteside(0);
		break;
	case 'h':
		hflag++;
		p = EARGF(usage());
		strcpy(system, p);
		break;
	case 'c':
		cflag++;
		cmd[0] = '!';
		cmd[1] = '\0';
		while(p = ARGF()) {
			strcat(cmd, " ");
			strcat(cmd, p);
		}
		break;
	case 'o':
		p9authproto = "p9sk2";
		srvname = "cpu";
		break;
	case 'u':
		user = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND;


	if(argc != 0)
		usage();

	if(hflag == 0) {
		p = getenv("cpu");
		if(p == 0)
			fatal(0, "set $cpu");
		strcpy(system, p);
	}

	if(err = rexcall(&data, system, srvname))
		fatal(1, "%s: %s", err, system);

	/* Tell the remote side the command to execute and where our working directory is */
	if(cflag)
		writestr(data, cmd, "command", 0);
	if(getwd(dat, sizeof(dat)) == 0)
		writestr(data, "NO", "dir", 0);
	else
		writestr(data, dat, "dir", 0);

	/* start up a process to pass along notes */
	lclnoteproc(data);

	/*
	 *  Wait for the other end to execute and start our file service
	 *  of /mnt/term
	 */
	if(readstr(data, buf, sizeof(buf)) < 0)
		fatal(1, "waiting for FS");
	if(strncmp("FS", buf, 2) != 0) {
		print("remote cpu: %s", buf);
		exits(buf);
	}

	/* Begin serving the gnot namespace */
	close(0);
	dup(data, 0);
	close(data);
	sprint(buf, "%d", msgsize);
	if(dbg)
		execl(exportfs, exportfs, "-dm", buf, 0);
	else
		execl(exportfs, exportfs, "-m", buf, 0);
	fatal(1, "starting exportfs");
}

void
fatal(int syserr, char *fmt, ...)
{
	char buf[ERRMAX];
	va_list arg;

	va_start(arg, fmt);
	doprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	if(syserr)
		fprint(2, "cpu: %s: %r\n", buf);
	else
		fprint(2, "cpu: %s\n", buf);
	exits(buf);
}

char *negstr = "negotiating authentication method";

char bug[256];

int
old9p(int fd)
{
	int p[2];

	if(pipe(p) < 0)
		fatal(1, "pipe");

	switch(rfork(RFPROC|RFFDG|RFNAMEG)) {
	case -1:
		fatal(1, "rfork srvold9p");
	case 0:
		if(fd != 1){
			dup(fd, 1);
			close(fd);
		}
		if(p[0] != 0){
			dup(p[0], 0);
			close(p[0]);
		}
		close(p[1]);
		if(0){
			fd = open("/sys/log/cpu", OWRITE);
			if(fd != 2){
				dup(fd, 2);
				close(fd);
			}
			execl("/bin/srvold9p", "srvold9p", "-ds", 0);
		} else
			execl("/bin/srvold9p", "srvold9p", "-s", 0);
		fatal(1, "exec srvold9p");
	default:
		close(fd);
		close(p[0]);
	}
	return p[1];
}

/* Invoked with stdin, stdout and stderr connected to the network connection */
void
remoteside(int old)
{
	char user[128], home[128], buf[128], xdir[128], cmd[128];
	int i, n, fd, badchdir, gotcmd;

	fd = 0;

	/* negotiate authentication mechanism */
	n = readstr(fd, cmd, sizeof(cmd));
	if(n < 0)
		fatal(1, "authenticating");
	if(setamalg(cmd) < 0){
		writestr(fd, "unsupported auth method", nil, 0);
		fatal(1, "bad auth method %s", cmd);
	} else
		writestr(fd, "", "", 1);

	fd = (*am->sf)(fd, user);
	if(fd < 0)
		fatal(1, "srvauth");

	/* Set environment values for the user */
	putenv("user", user);
	sprint(home, "/usr/%s", user);
	putenv("home", home);

	/* Now collect invoking cpu's current directory or possibly a command */
	gotcmd = 0;
	if(readstr(fd, xdir, sizeof(xdir)) < 0)
		fatal(1, "dir/cmd");
	if(xdir[0] == '!') {
		strcpy(cmd, &xdir[1]);
		gotcmd = 1;
		if(readstr(fd, xdir, sizeof(xdir)) < 0)
			fatal(1, "dir");
	}

	/* Establish the new process at the current working directory of the
	 * gnot */
	badchdir = 0;
	if(strcmp(xdir, "NO") == 0)
		chdir(home);
	else if(chdir(xdir) < 0) {
		badchdir = 1;
		chdir(home);
	}

	/* Start the gnot serving its namespace */
	writestr(fd, "FS", "FS", 0);
	writestr(fd, "/", "exportfs dir", 0);

	n = read(fd, buf, sizeof(buf));
	if(n != 2 || buf[0] != 'O' || buf[1] != 'K')
		exits("remote tree");

	if(old)
		fd = old9p(fd);

	/* make sure buffers are big by doing fversion explicitly; pick a huge number; other side will trim */
	strcpy(buf, VERSION9P);
	if(fversion(fd, 64*1024, buf, sizeof buf) < 0)
		exits("fversion failed");
	if(mount(fd, -1, "/mnt/term", MCREATE|MREPL, "") < 0)
		exits("mount failed");

	close(fd);

	/* the remote noteproc uses the mount so it must follow it */
	rmtnoteproc();

	for(i = 0; i < 3; i++)
		close(i);

	if(open("/mnt/term/dev/cons", OREAD) != 0)
		exits("open stdin");
	if(open("/mnt/term/dev/cons", OWRITE) != 1)
		exits("open stdout");
	dup(1, 2);

	if(badchdir)
		print("cpu: failed to chdir to '%s'\n", xdir);

	if(gotcmd)
		execl("/bin/rc", "rc", "-lc", cmd, 0);
	else
		execl("/bin/rc", "rc", "-li", 0);
	fatal(1, "exec shell");
}

char*
rexcall(int *fd, char *host, char *service)
{
	char *na;
	char dir[128];
	char err[ERRMAX];
	char msg[128];
	int n;

	na = netmkaddr(host, 0, service);
	if((*fd = dial(na, 0, dir, 0)) < 0)
		return "can't dial";

	/* negotiate authentication mechanism */
	if(ealgs != nil)
		snprint(msg, sizeof(msg), "%s %s", am->name, ealgs);
	else
		snprint(msg, sizeof(msg), "%s", am->name);
	writestr(*fd, msg, negstr, 0);
	n = readstr(*fd, err, sizeof err);
	if(n < 0)
		return negstr;
	if(*err){
		werrstr(err);
		return negstr;
	}

	/* authenticate */
	*fd = (*am->cf)(*fd);
	if(*fd < 0)
		return "can't authenticate";
	return 0;
}

void
writestr(int fd, char *str, char *thing, int ignore)
{
	int l, n;

	l = strlen(str);
	n = write(fd, str, l+1);
	if(!ignore && n < 0)
		fatal(1, "writing network: %s", thing);
}

int
readstr(int fd, char *str, int len)
{
	int n;

	while(len) {
		n = read(fd, str, 1);
		if(n < 0)
			return -1;
		if(*str == '\0')
			return 0;
		str++;
		len--;
	}
	return -1;
}

static int
readln(char *buf, int n)
{
	char *p = buf;

	n--;
	while(n > 0){
		if(read(0, p, 1) != 1)
			break;
		if(*p == '\n' || *p == '\r'){
			*p = 0;
			return p-buf;
		}
		p++;
	}
	*p = 0;
	return p-buf;
}

/*
 *  user level challenge/response
 */
static int
netkeyauth(int fd)
{
	char chall[32];
	char resp[32];

	strcpy(chall, getuser());
	print("user[%s]: ", chall);
	if(readln(resp, sizeof(resp)) < 0)
		return -1;
	if(*resp != 0)
		strcpy(chall, resp);
	writestr(fd, chall, "challenge/response", 1);

	for(;;){
		if(readstr(fd, chall, sizeof chall) < 0)
			break;
		if(*chall == 0)
			return fd;
		print("challenge: %s\nresponse: ", chall);
		if(readln(resp, sizeof(resp)) < 0)
			break;
		writestr(fd, resp, "challenge/response", 1);
	}
	return -1;
}

static int
netkeysrvauth(int fd, char *user)
{
	char response[32];
	Chalstate *ch;
	int tries;
	AuthInfo *ai;

	if(readstr(fd, user, 32) < 0)
		return -1;

	ai = nil;
	ch = nil;
	for(tries = 0; tries < 10; tries++){
		if((ch = auth_challenge("p9cr", user, nil)) == nil)
			return -1;
		writestr(fd, ch->chal, "challenge", 1);
		if(readstr(fd, response, sizeof response) < 0)
			return -1;
		ch->resp = response;
		ch->nresp = strlen(response);
		if((ai = auth_response(ch)) != nil)
			break;
	}
	auth_freechal(ch);
	if(ai == nil)
		return -1;
	writestr(fd, "", "challenge", 1);
	if(auth_chuid(ai, 0) < 0)
		fatal(1, "newns");
	auth_freeAI(ai);
	return fd;
}

static void
mksecret(char *t, uchar *f)
{
	sprint(t, "%2.2ux%2.2ux%2.2ux%2.2ux%2.2ux%2.2ux%2.2ux%2.2ux%2.2ux%2.2ux",
		f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[9]);
}

/*
 *  plan9 authentication followed by rc4 encryption
 */
static int
p9auth(int fd)
{
	uchar key[16];
	uchar digest[SHA1dlen];
	char fromclientsecret[21];
	char fromserversecret[21];
	int i;
	AuthInfo *ai;

	ai = auth_proxy(fd, auth_getkey, "proto=%q user=%q role=client", p9authproto, user);
	if(ai == nil)
		return -1;
	memmove(key+4, ai->secret, ai->nsecret);
	if(ealgs == nil)
		return fd;

	/* exchange random numbers */
	srand(truerand());
	for(i = 0; i < 4; i++)
		key[i] = rand();
	if(write(fd, key, 4) != 4)
		return -1;
	if(readn(fd, key+12, 4) != 4)
		return -1;

	/* scramble into two secrets */
	sha1(key, sizeof(key), digest, nil);
	mksecret(fromclientsecret, digest);
	mksecret(fromserversecret, digest+10);

	/* set up encryption */
	i = pushssl(fd, ealgs, fromclientsecret, fromserversecret, nil);
	if(i < 0)
		werrstr("can't establish ssl connection: %r");
	return i;
}

static int
noauth(int fd)
{
	ealgs = nil;
	return fd;
}

static int
srvnoauth(int fd, char *user)
{
	strcpy(user, getuser());
	ealgs = nil;
	return fd;
}

void
loghex(uchar *p, int n)
{
	char buf[100];
	int i;

	for(i = 0; i < n; i++)
		sprint(buf+2*i, "%2.2ux", p[i]);
	syslog(0, "cpu", buf);
}

static int
srvp9auth(int fd, char *user)
{
	uchar key[16];
	uchar digest[SHA1dlen];
	char fromclientsecret[21];
	char fromserversecret[21];
	int i;
	AuthInfo *ai;

	ai = auth_proxy(0, nil, "proto=%q role=server", p9authproto);
	if(ai == nil)
		return -1;
	if(auth_chuid(ai, nil) < 0)
		return -1;
	strcpy(user, ai->cuid);
	memmove(key+4, ai->secret, ai->nsecret);

	if(ealgs == nil)
		return fd;

	/* exchange random numbers */
	srand(truerand());
	for(i = 0; i < 4; i++)
		key[i+12] = rand();
	if(readn(fd, key, 4) != 4)
		return -1;
	if(write(fd, key+12, 4) != 4)
		return -1;

	/* scramble into two secrets */
	sha1(key, sizeof(key), digest, nil);
	mksecret(fromclientsecret, digest);
	mksecret(fromserversecret, digest+10);

	/* set up encryption */
	i = pushssl(fd, ealgs, fromserversecret, fromclientsecret, nil);
	if(i < 0)
		werrstr("can't establish ssl connection: %r");
	return i;
}

/*
 *  set authentication mechanism
 */
int
setam(char *name)
{
	for(am = authmethod; am->name != nil; am++)
		if(strcmp(am->name, name) == 0)
			return 0;
	am = authmethod;
	return -1;
}

/*
 *  set authentication mechanism and encryption/hash algs
 */
int
setamalg(char *s)
{
	ealgs = strchr(s, ' ');
	if(ealgs != nil)
		*ealgs++ = 0;
	return setam(s);
}

char *rmtnotefile = "/mnt/term/dev/cpunote";

/*
 *  loop reading /mnt/term/dev/note looking for notes.
 *  The child returns to start the shell.
 */
void
rmtnoteproc(void)
{
	int n, fd, pid, notepid;
	char buf[256];

	/* new proc returns to start shell */
	pid = rfork(RFPROC|RFFDG|RFNOTEG|RFNAMEG|RFMEM);
	switch(pid){
	case -1:
		syslog(0, "cpu", "cpu -R: can't start noteproc: %r");
		return;
	case 0:
		return;
	}

	/* new proc reads notes from other side and posts them to shell */
	switch(notepid = rfork(RFPROC|RFFDG|RFMEM)){
	case -1:
		syslog(0, "cpu", "cpu -R: can't start wait proc: %r");
		_exits(0);
	case 0:
		fd = open(rmtnotefile, OREAD);
		if(fd < 0){
			syslog(0, "cpu", "cpu -R: can't open %s", rmtnotefile);
			_exits(0);
		}

		for(;;){
			n = read(fd, buf, sizeof(buf)-1);
			if(n <= 0){
				postnote(PNGROUP, pid, "hangup");
				_exits(0);
			}
			buf[n] = 0;
			postnote(PNGROUP, pid, buf);
		}
		break;
	}

	/* original proc waits for shell proc to die and kills note proc */
	for(;;){
		n = waitpid();
		if(n < 0 || n == pid)
			break;
	}
	postnote(PNPROC, notepid, "kill");
	_exits(0);
}

enum
{
	Qdir,
	Qcpunote,

	Nfid = 32
};

struct {
	char	*name;
	Qid	qid;
	ulong	perm;
} fstab[] =
{
	[Qdir]		{ ".",		{Qdir, 0, QTDIR},	DMDIR|0555	},
	[Qcpunote]	{ "cpunote",	{Qcpunote, 0},		0444		}
};

typedef struct Note Note;
struct Note
{
	Note *next;
	char msg[ERRMAX];
};

typedef struct Request Request;
struct Request
{
	Request *next;
	Fcall f;
};

typedef struct Fid Fid;
struct Fid
{
	int	fid;
	int	file;
};
Fid fids[Nfid];

struct {
	Lock;
	Note *nfirst, *nlast;
	Request *rfirst, *rlast;
} nfs;

int
fsreply(int fd, Fcall *f)
{
	uchar buf[IOHDRSZ+Maxfdata];
	int n;

	if(dbg)
		fprint(2, "<-%F\n", f);
	n = convS2M(f, buf, sizeof buf);
	if(n > 0){
		if(write(fd, buf, n) != n){
			close(fd);
			return -1;
		}
	}
	return 0;
}

/* match a note read request with a note, reply to the request */
int
kick(int fd)
{
	Request *rp;
	Note *np;
	int rv;

	for(;;){
		lock(&nfs);
		rp = nfs.rfirst;
		np = nfs.nfirst;
		if(rp == nil || np == nil){
			unlock(&nfs);
			break;
		}
		nfs.rfirst = rp->next;
		nfs.nfirst = np->next;
		unlock(&nfs);

		rp->f.type = Rread;
		rp->f.count = strlen(np->msg);
		rp->f.data = np->msg;
		rv = fsreply(fd, &rp->f);
		free(rp);
		free(np);
		if(rv < 0)
			return -1;
	}
	return 0;
}

void
flushreq(int tag)
{
	Request **l, *rp;

	lock(&nfs);
	for(l = &nfs.rfirst; *l != nil; l = &(*l)->next){
		rp = *l;
		if(rp->f.tag == tag){
			*l = rp->next;
			unlock(&nfs);
			free(rp);
			return;
		}
	}
	unlock(&nfs);
}

Fid*
getfid(int fid)
{
	int i, freefid;

	freefid = -1;
	for(i = 0; i < Nfid; i++){
		if(freefid < 0 && fids[i].file < 0)
			freefid = i;
		if(fids[i].fid == fid)
			return &fids[i];
	}
	if(freefid >= 0){
		fids[freefid].fid = fid;
		return &fids[freefid];
	}
	return nil;
}

int
fsstat(int fd, Fid *fid, Fcall *f)
{
	Dir d;
	uchar statbuf[256];

	memset(&d, 0, sizeof(d));
	d.name = fstab[fid->file].name;
	d.uid = user;
	d.gid = user;
	d.muid = user;
	d.qid = fstab[fid->file].qid;
	d.mode = fstab[fid->file].perm;
	d.atime = d.mtime = time(0);
	f->stat = statbuf;
	f->nstat = convD2M(&d, statbuf, sizeof statbuf);
	return fsreply(fd, f);
}

int
fsread(int fd, Fid *fid, Fcall *f)
{
	Dir d;
	uchar buf[256];
	Request *rp;

	switch(fid->file){
	default:
		return -1;
	case Qdir:
		if(f->offset == 0 && f->count >0){
			memset(&d, 0, sizeof(d));
			d.name = fstab[Qcpunote].name;
			d.uid = user;
			d.gid = user;
			d.muid = user;
			d.qid = fstab[Qcpunote].qid;
			d.mode = fstab[Qcpunote].perm;
			d.atime = d.mtime = time(0);
			f->count = convD2M(&d, buf, sizeof buf);
			f->data = (char*)buf;
		} else
			f->count = 0;
		return fsreply(fd, f);
	case Qcpunote:
		rp = mallocz(sizeof(*rp), 1);
		if(rp == nil)
			return -1;
		rp->f = *f;
		lock(&nfs);
		if(nfs.rfirst == nil)
			nfs.rfirst = rp;
		else
			nfs.rlast->next = rp;
		nfs.rlast = rp;
		unlock(&nfs);
		return kick(fd);;
	}
}

char Eperm[] = "permission denied";
char Enofile[] = "out of files";
char Enotdir[] = "not a directory";

void
notefs(int fd)
{
	uchar buf[IOHDRSZ+Maxfdata];
	int i, j, n;
	char err[ERRMAX];
	Fcall f;
	Fid *fid, *nfid;
	int doreply;

	rfork(RFNOTEG);
	fmtinstall('F', fcallconv);

	for(n = 0; n < Nfid; n++)
		fids[n].file = -1;

	for(;;){
		n = read9pmsg(fd, buf, sizeof(buf));
		if(n <= 0){
			if(dbg)
				fprint(2, "read9pmsg(%d) returns %d: %r\n", fd, n);
			break;
		}
		if(convM2S(buf, n, &f) < 0)
			break;
		if(dbg)
			fprint(2, "->%F\n", &f);
		doreply = 1;
		fid = getfid(f.fid);
		if(fid == nil){
nofids:
			f.type = Rerror;
			f.ename = Enofile;
			fsreply(fd, &f);
			continue;
		}
		switch(f.type++){
		default:
			f.type = Rerror;
			f.ename = "unknown type";
			break;
		case Tflush:
			flushreq(f.oldtag);
			break;
		case Tversion:
			if(f.msize > IOHDRSZ+Maxfdata)
				f.msize = IOHDRSZ+Maxfdata;
			break;
		case Tauth:
			f.type = Rerror;
			f.ename = "cpu: authentication not required";
			break;
		case Tattach:
			f.qid = fstab[Qdir].qid;
			fid->file = Qdir;
			break;
		case Twalk:
			nfid = nil;
			if(f.newfid != f.fid){
				nfid = getfid(f.newfid);
				if(nfid == nil)
					goto nofids;
				nfid->file = fid->file;
				fid = nfid;
			}

			f.ename = nil;
			for(i=0; i<f.nwname; i++){
				if(i > MAXWELEM){
					f.type = Rerror;
					f.ename = "too many name elements";
					break;
				}
				if(fid->file != Qdir){
					f.type = Rerror;
					f.ename = Enotdir;
					break;
				}
				if(strcmp(f.wname[i], "cpunote") == 0){
					fid->file = Qcpunote;
					f.wqid[i] = fstab[Qcpunote].qid;
					continue;
				}
				f.type = Rerror;
				f.ename = err;
				strcpy(err, "cpu: file \"");
				for(j=0; j<=i; j++){
					if(strlen(err)+1+strlen(f.wname[j])+32 > sizeof err)
						break;
					if(j != 0)
						strcat(err, "/");
					strcat(err, f.wname[j]);
				}
				strcat(err, "\" does not exist");
				break;
			}
			if(nfid != nil && (f.ename != nil || i < f.nwname))
				nfid ->file = -1;
			if(f.type != Rerror)
				f.nwqid = i;
			break;
		case Topen:
			if(f.mode != OREAD){
				f.type = Rerror;
				f.ename = Eperm;
			}
			f.qid = fstab[fid->file].qid;
			break;
		case Tcreate:
			f.type = Rerror;
			f.ename = Eperm;
			break;
		case Tread:
			if(fsread(fd, fid, &f) < 0)
				goto err;
			doreply = 0;
			break;
		case Twrite:
			f.type = Rerror;
			f.ename = Eperm;
			break;
		case Tclunk:
			fid->file = -1;
			break;
		case Tremove:
			f.type = Rerror;
			f.ename = Eperm;
			break;
		case Tstat:
			if(fsstat(fd, fid, &f) < 0)
				goto err;
			doreply = 0;
			break;
		case Twstat:
			f.type = Rerror;
			f.ename = Eperm;
			break;
		}
		if(doreply)
			if(fsreply(fd, &f) < 0)
				break;
	}
err:
	if(dbg)
		fprint(2, "notefs exiting: %r\n");
	close(fd);
}

char 	notebuf[ERRMAX];

void
catcher(void*, char *text)
{
	int n;

	n = strlen(text);
	if(n >= sizeof(notebuf))
		n = sizeof(notebuf)-1;
	memmove(notebuf, text, n);
	notebuf[n] = '\0';
	noted(NCONT);
}

/*
 *  mount in /dev a note file for the remote side to read.
 */
void
lclnoteproc(int netfd)
{
	int exportfspid;
	Waitmsg *w;
	Note *np;
	int pfd[2];

	if(pipe(pfd) < 0){
		fprint(2, "cpu: can't start note proc: pipe: %r\n");
		return;
	}

	/* new proc mounts and returns to start exportfs */
	switch(exportfspid = rfork(RFPROC|RFNAMEG|RFFDG|RFMEM)){
	case -1:
		fprint(2, "cpu: can't start note proc: rfork: %r\n");
		return;
	case 0:
		close(pfd[0]);
		if(mount(pfd[1], -1, "/dev", MBEFORE, "") < 0)
			fprint(2, "cpu: can't mount note proc: %r\n");
		close(pfd[1]);
		return;
	}

	close(netfd);
	close(pfd[1]);

	/* new proc listens for note file system rpc's */
	switch(rfork(RFPROC|RFNAMEG|RFMEM)){
	case -1:
		fprint(2, "cpu: can't start note proc: rfork1: %r\n");
		_exits(0);
	case 0:
		notefs(pfd[0]);
		_exits(0);
	}

	/* original proc waits for notes */
	notify(catcher);
	w = nil;
	for(;;) {
		*notebuf = 0;
		free(w);
		w = wait();
		if(w == nil) {
			if(*notebuf == 0)
				break;
			np = mallocz(sizeof(Note), 1);
			if(np != nil){
				strcpy(np->msg, notebuf);
				lock(&nfs);
				if(nfs.nfirst == nil)
					nfs.nfirst = np;
				else
					nfs.nlast->next = np;
				nfs.nlast = np;
				unlock(&nfs);
				kick(pfd[0]);
			}
			unlock(&nfs);
		} else if(w->pid == exportfspid)
			break;
	}

	if(w == nil)
		exits(nil);
	exits(w->msg);
}
