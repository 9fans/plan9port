#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include <thread.h>

int post9p(int, char*);
int debug;
char *aname = "";
char *keypattern = "";
int fd;
int msize;
int doauth;
u32int afid = NOFID;
extern char *post9parg;	/* clumsy hack */
void xauth(void);
AuthInfo* xauth_proxy(AuthGetkey *getkey, char *fmt, ...);

void
usage(void)
{
	fprint(2, "usage: srv [-a] [-A aname] [-k keypattern] addr [srvname]\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char **argv)
{
	char *addr, *service;

	fmtinstall('F', fcallfmt);
	fmtinstall('M', dirmodefmt);

	ARGBEGIN{
	case 'D':
		debug = 1;
		break;
	case 'A':
		/* BUG: should be able to repeat this and establish multiple afids */
		aname = EARGF(usage());
		break;
	case 'a':
		doauth = 1;
		break;
	case 'n':
		doauth = -1;
		break;
	case 'k':
		keypattern = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	if(argc != 1 && argc != 2)
		usage();

	addr = netmkaddr(argv[0], "tcp", "9fs");
	if((fd = dial(addr, nil, nil, nil)) < 0)
		sysfatal("dial %s: %r", addr);

	if(doauth > 0)
		xauth();

	if(argc == 2)
		service = argv[1];
	else
		service = argv[0];

	rfork(RFNOTEG);
	if(post9p(fd, service) < 0)
		sysfatal("post9p: %r");

	threadexitsall(0);
}

void
do9p(Fcall *tx, Fcall *rx)
{
	static uchar buf[9000];
	static char ebuf[200];
	int n;

	n = convS2M(tx, buf, sizeof buf);
	if(n == BIT16SZ){
		werrstr("convS2M failed");
		goto err;
	}
	if(debug)
		fprint(2, "<- %F\n", tx);
	if(write(fd, buf, n) != n)
		goto err;
	if((n = read9pmsg(fd, buf, sizeof buf)) < 0)
		goto err;
	if(n == 0){
		werrstr("unexpected eof");
		goto err;
	}
	if(convM2S(buf, n, rx) != n){
		werrstr("convM2S failed");
		goto err;
	}
	if(debug)
		fprint(2, "-> %F\n", rx);
	if(rx->type != Rerror && rx->type != tx->type+1){
		werrstr("unexpected type");
		goto err;
	}
	if(rx->tag != tx->tag){
		werrstr("unexpected tag");
		goto err;
	}
	return;

err:
	rerrstr(ebuf, sizeof ebuf);
	rx->ename = ebuf;
	rx->type = Rerror;
	return;
}

void
xauth(void)
{
	Fcall tx, rx;

	afid = 0;
	tx.type = Tversion;
	tx.tag = NOTAG;
	tx.version = "9P2000";
	tx.msize = 8192;
	do9p(&tx, &rx);
	if(rx.type == Rerror)
		sysfatal("Tversion: %s", rx.ename);
	msize = rx.msize;

	tx.type = Tauth;
	tx.tag = 1;
	tx.afid = afid;
	tx.uname = getuser();
	tx.aname = aname;
	do9p(&tx, &rx);
	if(rx.type == Rerror){
		fprint(2, "rx: %s\n", rx.ename);
		afid = NOFID;
		return;
	}

	if(xauth_proxy(auth_getkey, "proto=p9any role=client %s", keypattern) == nil)
		sysfatal("authproxy: %r");
}

int
xread(void *buf, int n)
{
	Fcall tx, rx;

	tx.type = Tread;
	tx.tag = 1;
	tx.fid = 0;	/* afid above */
	tx.count = n;
	tx.offset = 0;
	do9p(&tx, &rx);
	if(rx.type == Rerror){
		werrstr("%s", rx.ename);
		return -1;
	}

	if(rx.count > n){
		werrstr("too much data returned");
		return -1;
	}
	memmove(buf, rx.data, rx.count);
	return rx.count;
}

int
xwrite(void *buf, int n)
{
	Fcall tx, rx;

	tx.type = Twrite;
	tx.tag = 1;
	tx.fid = 0;	/* afid above */
	tx.data = buf;
	tx.count = n;
	tx.offset = 0;
	do9p(&tx, &rx);
	if(rx.type == Rerror){
		werrstr("%s", rx.ename);
		return -1;
	}
	return n;
}


/*
 * changed to add -A below
 */
#undef _exits
int
post9p(int fd, char *name)
{
	int i;
	char *ns, *s;
	Waitmsg *w;

	if((ns = getns()) == nil)
		return -1;

	s = smprint("unix!%s/%s", ns, name);
	free(ns);
	if(s == nil)
		return -1;
	switch(fork()){
	case -1:
		return -1;
	case 0:
		dup(fd, 0);
		dup(fd, 1);
		for(i=3; i<20; i++)
			close(i);
		if(doauth > 0)
			execlp("9pserve", "9pserve", "-u",
				"-M",
					smprint("%d", msize),
				"-A",
					aname,
					smprint("%d", afid),
				s, (char*)0);
		else
			execlp("9pserve", "9pserve",
				doauth < 0 ? "-nu" : "-u", s, (char*)0);
		fprint(2, "exec 9pserve: %r\n");
		_exits("exec");
	default:
		w = wait();
		if(w == nil)
			return -1;
		close(fd);
		free(s);
		if(w->msg && w->msg[0]){
			free(w);
			werrstr("9pserve failed");
			return -1;
		}
		free(w);
		return 0;
	}
}

enum { ARgiveup = 100 };
static int
dorpc(AuthRpc *rpc, char *verb, char *val, int len, AuthGetkey *getkey)
{
	int ret;

	for(;;){
		if((ret = auth_rpc(rpc, verb, val, len)) != ARneedkey && ret != ARbadkey)
			return ret;
		if(getkey == nil)
			return ARgiveup;	/* don't know how */
		if((*getkey)(rpc->arg) < 0)
			return ARgiveup;	/* user punted */
	}
}


/*
 *  this just proxies what the factotum tells it to.
 */
AuthInfo*
xfauth_proxy(AuthRpc *rpc, AuthGetkey *getkey, char *params)
{
	char *buf;
	int m, n, ret;
	AuthInfo *a;
	char oerr[ERRMAX];

	rerrstr(oerr, sizeof oerr);
	werrstr("UNKNOWN AUTH ERROR");

	if(dorpc(rpc, "start", params, strlen(params), getkey) != ARok){
		werrstr("fauth_proxy start: %r");
		return nil;
	}

	buf = malloc(AuthRpcMax);
	if(buf == nil)
		return nil;
	for(;;){
		switch(dorpc(rpc, "read", nil, 0, getkey)){
		case ARdone:
			free(buf);
			a = auth_getinfo(rpc);
			errstr(oerr, sizeof oerr);	/* no error, restore whatever was there */
			return a;
		case ARok:
			if(xwrite(rpc->arg, rpc->narg) != rpc->narg){
				werrstr("auth_proxy write fid: %r");
				goto Error;
			}
			break;
		case ARphase:
			n = 0;
			memset(buf, 0, AuthRpcMax);
			while((ret = dorpc(rpc, "write", buf, n, getkey)) == ARtoosmall){
				if(atoi(rpc->arg) > AuthRpcMax)
					break;
				m = xread(buf+n, atoi(rpc->arg)-n);
				if(m <= 0){
					if(m == 0)
						werrstr("auth_proxy short read: %s", buf);
					goto Error;
				}
				n += m;
			}
			if(ret != ARok){
				werrstr("auth_proxy rpc write: %s: %r", buf);
				goto Error;
			}
			break;
		default:
			werrstr("auth_proxy rpc: %r");
			goto Error;
		}
	}
Error:
	free(buf);
	return nil;
}

AuthInfo*
xauth_proxy(AuthGetkey *getkey, char *fmt, ...)
{
	char *p;
	va_list arg;
	AuthInfo *ai;
	AuthRpc *rpc;

	quotefmtinstall();	/* just in case */
	va_start(arg, fmt);
	p = vsmprint(fmt, arg);
	va_end(arg);

	rpc = auth_allocrpc();
	if(rpc == nil){
		free(p);
		return nil;
	}

	ai = xfauth_proxy(rpc, getkey, p);
	free(p);
	auth_freerpc(rpc);
	return ai;
}
