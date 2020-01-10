#include "common.h"
#include <ctype.h>
#include <auth.h>
#include <libsec.h>

typedef struct Cmd Cmd;
struct Cmd
{
	char *name;
	int needauth;
	int (*f)(char*);
};

static void hello(void);
static int apopcmd(char*);
static int capacmd(char*);
static int delecmd(char*);
static int listcmd(char*);
static int noopcmd(char*);
static int passcmd(char*);
static int quitcmd(char*);
static int rsetcmd(char*);
static int retrcmd(char*);
static int statcmd(char*);
static int stlscmd(char*);
static int topcmd(char*);
static int synccmd(char*);
static int uidlcmd(char*);
static int usercmd(char*);
static char *nextarg(char*);
static int getcrnl(char*, int);
static int readmbox(char*);
static void sendcrnl(char*, ...);
static int senderr(char*, ...);
static int sendok(char*, ...);
#pragma varargck argpos sendcrnl 1
#pragma varargck argpos senderr 1
#pragma varargck argpos sendok 1

Cmd cmdtab[] =
{
	"apop", 0, apopcmd,
	"capa", 0, capacmd,
	"dele", 1, delecmd,
	"list", 1, listcmd,
	"noop", 0, noopcmd,
	"pass", 0, passcmd,
	"quit", 0, quitcmd,
	"rset", 0, rsetcmd,
	"retr", 1, retrcmd,
	"stat", 1, statcmd,
	"stls", 0, stlscmd,
	"sync", 1, synccmd,
	"top", 1, topcmd,
	"uidl", 1, uidlcmd,
	"user", 0, usercmd,
	0, 0, 0
};

static Biobuf in;
static Biobuf out;
static int passwordinclear;
static int didtls;

typedef struct Msg Msg;
struct Msg
{
	int upasnum;
	char digest[64];
	int bytes;
	int deleted;
};

static int totalbytes;
static int totalmsgs;
static Msg *msg;
static int nmsg;
static int loggedin;
static int debug;
static uchar *tlscert;
static int ntlscert;
static char *peeraddr;
static char tmpaddr[64];

void
usage(void)
{
	fprint(2, "usage: upas/pop3 [-a authmboxfile] [-d debugfile] [-p]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	int fd;
	char *arg, cmdbuf[1024];
	Cmd *c;

	rfork(RFNAMEG);
	Binit(&in, 0, OREAD);
	Binit(&out, 1, OWRITE);

	ARGBEGIN{
	case 'a':
		loggedin = 1;
		if(readmbox(EARGF(usage())) < 0)
			exits(nil);
		break;
	case 'd':
		debug++;
		if((fd = create(EARGF(usage()), OWRITE, 0666)) >= 0 && fd != 2){
			dup(fd, 2);
			close(fd);
		}
		break;
	case 'r':
		strecpy(tmpaddr, tmpaddr+sizeof tmpaddr, EARGF(usage()));
		if(arg = strchr(tmpaddr, '!'))
			*arg = '\0';
		peeraddr = tmpaddr;
		break;
	case 't':
		tlscert = readcert(EARGF(usage()), &ntlscert);
		if(tlscert == nil){
			senderr("cannot read TLS certificate: %r");
			exits(nil);
		}
		break;
	case 'p':
		passwordinclear = 1;
		break;
	}ARGEND

	/* do before TLS */
	if(peeraddr == nil)
		peeraddr = remoteaddr(0,0);

	hello();

	while(Bflush(&out), getcrnl(cmdbuf, sizeof cmdbuf) > 0){
		arg = nextarg(cmdbuf);
		for(c=cmdtab; c->name; c++)
			if(cistrcmp(c->name, cmdbuf) == 0)
				break;
		if(c->name == 0){
			senderr("unknown command %s", cmdbuf);
			continue;
		}
		if(c->needauth && !loggedin){
			senderr("%s requires authentication", cmdbuf);
			continue;
		}
		(*c->f)(arg);
	}
	exits(nil);
}

/* sort directories in increasing message number order */
static int
dircmp(void *a, void *b)
{
	return atoi(((Dir*)a)->name) - atoi(((Dir*)b)->name);
}

static int
readmbox(char *box)
{
	int fd, i, n, nd, lines, pid;
	char buf[100], err[ERRMAX];
	char *p;
	Biobuf *b;
	Dir *d, *draw;
	Msg *m;
	Waitmsg *w;

	unmount(nil, "/mail/fs");
	switch(pid = fork()){
	case -1:
		return senderr("can't fork to start upas/fs");

	case 0:
		close(0);
		close(1);
		open("/dev/null", OREAD);
		open("/dev/null", OWRITE);
		execl("/bin/upas/fs", "upas/fs", "-np", "-f", box, nil);
		snprint(err, sizeof err, "upas/fs: %r");
		_exits(err);
		break;

	default:
		break;
	}

	if((w = wait()) == nil || w->pid != pid || w->msg[0] != '\0'){
		if(w && w->pid==pid)
			return senderr("%s", w->msg);
		else
			return senderr("can't initialize upas/fs");
	}
	free(w);

	if(chdir("/mail/fs/mbox") < 0)
		return senderr("can't initialize upas/fs: %r");

	if((fd = open(".", OREAD)) < 0)
		return senderr("cannot open /mail/fs/mbox: %r");
	nd = dirreadall(fd, &d);
	close(fd);
	if(nd < 0)
		return senderr("cannot read from /mail/fs/mbox: %r");

	msg = mallocz(sizeof(Msg)*nd, 1);
	if(msg == nil)
		return senderr("out of memory");

	if(nd == 0)
		return 0;
	qsort(d, nd, sizeof(d[0]), dircmp);

	for(i=0; i<nd; i++){
		m = &msg[nmsg];
		m->upasnum = atoi(d[i].name);
		sprint(buf, "%d/digest", m->upasnum);
		if((fd = open(buf, OREAD)) < 0)
			continue;
		n = readn(fd, m->digest, sizeof m->digest - 1);
		close(fd);
		if(n < 0)
			continue;
		m->digest[n] = '\0';

		/*
		 * We need the number of message lines so that we
		 * can adjust the byte count to include \r's.
		 * Upas/fs gives us the number of lines in the raw body
		 * in the lines file, but we have to count rawheader ourselves.
		 * There is one blank line between raw header and raw body.
		 */
		sprint(buf, "%d/rawheader", m->upasnum);
		if((b = Bopen(buf, OREAD)) == nil)
			continue;
		lines = 0;
		for(;;){
			p = Brdline(b, '\n');
			if(p == nil){
				if((n = Blinelen(b)) == 0)
					break;
				Bseek(b, n, 1);
			}else
				lines++;
		}
		Bterm(b);
		lines++;
		sprint(buf, "%d/lines", m->upasnum);
		if((fd = open(buf, OREAD)) < 0)
			continue;
		n = readn(fd, buf, sizeof buf - 1);
		close(fd);
		if(n < 0)
			continue;
		buf[n] = '\0';
		lines += atoi(buf);

		sprint(buf, "%d/raw", m->upasnum);
		if((draw = dirstat(buf)) == nil)
			continue;
		m->bytes = lines+draw->length;
		free(draw);
		nmsg++;
		totalmsgs++;
		totalbytes += m->bytes;
	}
	return 0;
}

/*
 *  get a line that ends in crnl or cr, turn terminating crnl into a nl
 *
 *  return 0 on EOF
 */
static int
getcrnl(char *buf, int n)
{
	int c;
	char *ep;
	char *bp;
	Biobuf *fp = &in;

	Bflush(&out);

	bp = buf;
	ep = bp + n - 1;
	while(bp != ep){
		c = Bgetc(fp);
		if(debug) {
			seek(2, 0, 2);
			fprint(2, "%c", c);
		}
		switch(c){
		case -1:
			*bp = 0;
			if(bp==buf)
				return 0;
			else
				return bp-buf;
		case '\r':
			c = Bgetc(fp);
			if(c == '\n'){
				if(debug) {
					seek(2, 0, 2);
					fprint(2, "%c", c);
				}
				*bp = 0;
				return bp-buf;
			}
			Bungetc(fp);
			c = '\r';
			break;
		case '\n':
			*bp = 0;
			return bp-buf;
		}
		*bp++ = c;
	}
	*bp = 0;
	return bp-buf;
}

static void
sendcrnl(char *fmt, ...)
{
	char buf[1024];
	va_list arg;

	va_start(arg, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	if(debug)
		fprint(2, "-> %s\n", buf);
	Bprint(&out, "%s\r\n", buf);
}

static int
senderr(char *fmt, ...)
{
	char buf[1024];
	va_list arg;

	va_start(arg, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	if(debug)
		fprint(2, "-> -ERR %s\n", buf);
	Bprint(&out, "-ERR %s\r\n", buf);
	return -1;
}

static int
sendok(char *fmt, ...)
{
	char buf[1024];
	va_list arg;

	va_start(arg, fmt);
	vseprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	if(*buf){
		if(debug)
			fprint(2, "-> +OK %s\n", buf);
		Bprint(&out, "+OK %s\r\n", buf);
	} else {
		if(debug)
			fprint(2, "-> +OK\n");
		Bprint(&out, "+OK\r\n");
	}
	return 0;
}

static int
capacmd(char*)
{
	sendok("");
	sendcrnl("TOP");
	if(passwordinclear || didtls)
		sendcrnl("USER");
	sendcrnl("PIPELINING");
	sendcrnl("UIDL");
	sendcrnl("STLS");
	sendcrnl(".");
	return 0;
}

static int
delecmd(char *arg)
{
	int n;

	if(*arg==0)
		return senderr("DELE requires a message number");

	n = atoi(arg)-1;
	if(n < 0 || n >= nmsg || msg[n].deleted)
		return senderr("no such message");

	msg[n].deleted = 1;
	totalmsgs--;
	totalbytes -= msg[n].bytes;
	sendok("message %d deleted", n+1);
	return 0;
}

static int
listcmd(char *arg)
{
	int i, n;

	if(*arg == 0){
		sendok("+%d message%s (%d octets)", totalmsgs, totalmsgs==1 ? "":"s", totalbytes);
		for(i=0; i<nmsg; i++){
			if(msg[i].deleted)
				continue;
			sendcrnl("%d %d", i+1, msg[i].bytes);
		}
		sendcrnl(".");
	}else{
		n = atoi(arg)-1;
		if(n < 0 || n >= nmsg || msg[n].deleted)
			return senderr("no such message");
		sendok("%d %d", n+1, msg[n].bytes);
	}
	return 0;
}

static int
noopcmd(char *arg)
{
	USED(arg);
	sendok("");
	return 0;
}

static void
_synccmd(char*)
{
	int i, fd;
	char *s;
	Fmt f;

	if(!loggedin){
		sendok("");
		return;
	}

	fmtstrinit(&f);
	fmtprint(&f, "delete mbox");
	for(i=0; i<nmsg; i++)
		if(msg[i].deleted)
			fmtprint(&f, " %d", msg[i].upasnum);
	s = fmtstrflush(&f);
	if(strcmp(s, "delete mbox") != 0){	/* must have something to delete */
		if((fd = open("../ctl", OWRITE)) < 0){
			senderr("open ctl to delete messages: %r");
			return;
		}
		if(write(fd, s, strlen(s)) < 0){
			senderr("error deleting messages: %r");
			return;
		}
	}
	sendok("");
}

static int
synccmd(char*)
{
	_synccmd(nil);
	return 0;
}

static int
quitcmd(char*)
{
	synccmd(nil);
	exits(nil);
	return 0;
}

static int
retrcmd(char *arg)
{
	int n;
	Biobuf *b;
	char buf[40], *p;

	if(*arg == 0)
		return senderr("RETR requires a message number");
	n = atoi(arg)-1;
	if(n < 0 || n >= nmsg || msg[n].deleted)
		return senderr("no such message");
	snprint(buf, sizeof buf, "%d/raw", msg[n].upasnum);
	if((b = Bopen(buf, OREAD)) == nil)
		return senderr("message disappeared");
	sendok("");
	while((p = Brdstr(b, '\n', 1)) != nil){
		if(p[0]=='.')
			Bwrite(&out, ".", 1);
		Bwrite(&out, p, strlen(p));
		Bwrite(&out, "\r\n", 2);
		free(p);
	}
	Bterm(b);
	sendcrnl(".");
	return 0;
}

static int
rsetcmd(char*)
{
	int i;

	for(i=0; i<nmsg; i++){
		if(msg[i].deleted){
			msg[i].deleted = 0;
			totalmsgs++;
			totalbytes += msg[i].bytes;
		}
	}
	return sendok("");
}

static int
statcmd(char*)
{
	return sendok("%d %d", totalmsgs, totalbytes);
}

static int
trace(char *fmt, ...)
{
	va_list arg;
	int n;

	va_start(arg, fmt);
	n = vfprint(2, fmt, arg);
	va_end(arg);
	return n;
}

static int
stlscmd(char*)
{
	int fd;
	TLSconn conn;

	if(didtls)
		return senderr("tls already started");
	if(!tlscert)
		return senderr("don't have any tls credentials");
	sendok("");
	Bflush(&out);

	memset(&conn, 0, sizeof conn);
	conn.cert = tlscert;
	conn.certlen = ntlscert;
	if(debug)
		conn.trace = trace;
	fd = tlsServer(0, &conn);
	if(fd < 0)
		sysfatal("tlsServer: %r");
	dup(fd, 0);
	dup(fd, 1);
	close(fd);
	Binit(&in, 0, OREAD);
	Binit(&out, 1, OWRITE);
	didtls = 1;
	return 0;
}

static int
topcmd(char *arg)
{
	int done, i, lines, n;
	char buf[40], *p;
	Biobuf *b;

	if(*arg == 0)
		return senderr("TOP requires a message number");
	n = atoi(arg)-1;
	if(n < 0 || n >= nmsg || msg[n].deleted)
		return senderr("no such message");
	arg = nextarg(arg);
	if(*arg == 0)
		return senderr("TOP requires a line count");
	lines = atoi(arg);
	if(lines < 0)
		return senderr("bad args to TOP");
	snprint(buf, sizeof buf, "%d/raw", msg[n].upasnum);
	if((b = Bopen(buf, OREAD)) == nil)
		return senderr("message disappeared");
	sendok("");
	while(p = Brdstr(b, '\n', 1)){
		if(p[0]=='.')
			Bputc(&out, '.');
		Bwrite(&out, p, strlen(p));
		Bwrite(&out, "\r\n", 2);
		done = p[0]=='\0';
		free(p);
		if(done)
			break;
	}
	for(i=0; i<lines; i++){
		p = Brdstr(b, '\n', 1);
		if(p == nil)
			break;
		if(p[0]=='.')
			Bwrite(&out, ".", 1);
		Bwrite(&out, p, strlen(p));
		Bwrite(&out, "\r\n", 2);
		free(p);
	}
	sendcrnl(".");
	Bterm(b);
	return 0;
}

static int
uidlcmd(char *arg)
{
	int n;

	if(*arg==0){
		sendok("");
		for(n=0; n<nmsg; n++){
			if(msg[n].deleted)
				continue;
			sendcrnl("%d %s", n+1, msg[n].digest);
		}
		sendcrnl(".");
	}else{
		n = atoi(arg)-1;
		if(n < 0 || n >= nmsg || msg[n].deleted)
			return senderr("no such message");
		sendok("%d %s", n+1, msg[n].digest);
	}
	return 0;
}

static char*
nextarg(char *p)
{
	while(*p && *p != ' ' && *p != '\t')
		p++;
	while(*p == ' ' || *p == '\t')
		*p++ = 0;
	return p;
}

/*
 * authentication
 */
Chalstate *chs;
char user[256];
char box[256];
char cbox[256];

static void
hello(void)
{
	fmtinstall('H', encodefmt);
	if((chs = auth_challenge("proto=apop role=server")) == nil){
		senderr("auth server not responding, try later");
		exits(nil);
	}

	sendok("POP3 server ready %s", chs->chal);
}

static int
setuser(char *arg)
{
	char *p;

	strcpy(box, "/mail/box/");
	strecpy(box+strlen(box), box+sizeof box-7, arg);
	strcpy(cbox, box);
	cleanname(cbox);
	if(strcmp(cbox, box) != 0)
		return senderr("bad mailbox name");
	strcat(box, "/mbox");

	strecpy(user, user+sizeof user, arg);
	if(p = strchr(user, '/'))
		*p = '\0';
	return 0;
}

static int
usercmd(char *arg)
{
	if(loggedin)
		return senderr("already authenticated");
	if(*arg == 0)
		return senderr("USER requires argument");
	if(setuser(arg) < 0)
		return -1;
	return sendok("");
}

static void
enableaddr(void)
{
	int fd;
	char buf[64];

	/* hide the peer IP address under a rock in the ratifier FS */
	if(peeraddr == 0 || *peeraddr == 0)
		return;

	sprint(buf, "/mail/ratify/trusted/%s#32", peeraddr);

	/*
	 * if the address is already there and the user owns it,
	 * remove it and recreate it to give him a new time quanta.
	 */
	if(access(buf, 0) >= 0  && remove(buf) < 0)
		return;

	fd = create(buf, OREAD, 0666);
	if(fd >= 0){
		close(fd);
/*		syslog(0, "pop3", "ratified %s", peeraddr); */
	}
}

static int
dologin(char *response)
{
	AuthInfo *ai;
	static int tries;

	chs->user = user;
	chs->resp = response;
	chs->nresp = strlen(response);
	if((ai = auth_response(chs)) == nil){
		if(tries++ >= 5){
			senderr("authentication failed: %r; server exiting");
			exits(nil);
		}
		return senderr("authentication failed");
	}

	if(auth_chuid(ai, nil) < 0){
		senderr("chuid failed: %r; server exiting");
		exits(nil);
	}
	auth_freeAI(ai);
	auth_freechal(chs);
	chs = nil;

	loggedin = 1;
	if(newns(user, 0) < 0){
		senderr("newns failed: %r; server exiting");
		exits(nil);
	}

	enableaddr();
	if(readmbox(box) < 0)
		exits(nil);
	return sendok("mailbox is %s", box);
}

static int
passcmd(char *arg)
{
	DigestState *s;
	uchar digest[MD5dlen];
	char response[2*MD5dlen+1];

	if(passwordinclear==0 && didtls==0)
		return senderr("password in the clear disallowed");

	/* use password to encode challenge */
	if((chs = auth_challenge("proto=apop role=server")) == nil)
		return senderr("couldn't get apop challenge");

	/* hash challenge with secret and convert to ascii */
	s = md5((uchar*)chs->chal, chs->nchal, 0, 0);
	md5((uchar*)arg, strlen(arg), digest, s);
	snprint(response, sizeof response, "%.*H", MD5dlen, digest);
	return dologin(response);
}

static int
apopcmd(char *arg)
{
	char *resp;

	resp = nextarg(arg);
	if(setuser(arg) < 0)
		return -1;
	return dologin(resp);
}
