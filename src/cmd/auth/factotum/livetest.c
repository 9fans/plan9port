#include <u.h>
#include <libc.h>
#include <fcall.h>

enum
{
	ARok = 0,
	ARdone,
	ARerror,
	ARneedkey,
	ARbadkey,
	ARwritenext,
	ARtoosmall,
	ARtoobig,
	ARrpcfailure,
	ARphase,

	AuthRpcMax = 4096
};

typedef struct Rpc Rpc;

struct Rpc
{
	int fd;
	char ibuf[AuthRpcMax+1];
	char obuf[AuthRpcMax+1];
	char *arg;
	int narg;
};

static struct {
	char *verb;
	int val;
} tab[] = {
	"ok", ARok,
	"done", ARdone,
	"error", ARerror,
	"needkey", ARneedkey,
	"badkey", ARbadkey,
	"phase", ARphase,
	"toosmall", ARtoosmall,
};

static int
classify(Rpc *rpc, int n)
{
	int i, len;

	rpc->ibuf[n] = 0;
	for(i = 0; i < nelem(tab); i++){
		len = strlen(tab[i].verb);
		if(n >= len && memcmp(rpc->ibuf, tab[i].verb, len) == 0
		&& (n == len || rpc->ibuf[len] == ' ')){
			if(n == len){
				rpc->narg = 0;
				rpc->arg = "";
			}else{
				rpc->narg = n - (len+1);
				rpc->arg = rpc->ibuf + len + 1;
			}
			return tab[i].val;
		}
	}
	werrstr("malformed rpc response");
	return ARrpcfailure;
}

static int
rpccall(Rpc *rpc, char *verb, void *a, int na)
{
	int l, n;

	l = strlen(verb);
	if(l + 1 + na > AuthRpcMax){
		werrstr("rpc too big");
		return ARtoobig;
	}

	memmove(rpc->obuf, verb, l);
	rpc->obuf[l] = ' ';
	if(na > 0)
		memmove(rpc->obuf+l+1, a, na);
	if(write(rpc->fd, rpc->obuf, l+1+na) != l+1+na){
		werrstr("rpc write: %r");
		return ARrpcfailure;
	}

	n = read(rpc->fd, rpc->ibuf, AuthRpcMax);
	if(n < 0){
		werrstr("rpc read: %r");
		return ARrpcfailure;
	}
	return classify(rpc, n);
}

static uchar*
skipstring(uchar *p, uchar *ep)
{
	uint n;

	if(p == nil || p + BIT16SZ > ep)
		return nil;
	n = GBIT16(p);
	p += BIT16SZ;
	if(p + n > ep)
		return nil;
	return p + n;
}

static int
authsecretlen(Rpc *rpc)
{
	uchar *p, *ep;
	uint n;

	if(rpccall(rpc, "authinfo", nil, 0) != ARok)
		return -1;

	p = (uchar*)rpc->arg;
	ep = p + rpc->narg;
	p = skipstring(p, ep);
	p = skipstring(p, ep);
	p = skipstring(p, ep);
	if(p == nil || p + BIT16SZ > ep){
		werrstr("bad authinfo");
		return -1;
	}
	n = GBIT16(p);
	p += BIT16SZ;
	if(p + n > ep){
		werrstr("bad authinfo");
		return -1;
	}
	return n;
}

static int
runproxy(Rpc *rpc, char *params, int datafd)
{
	char buf[AuthRpcMax];
	int m, n, ret;

	ret = rpccall(rpc, "start", params, strlen(params));
	if(ret != ARok){
		werrstr("start: %s", rpc->arg);
		return -1;
	}

	for(;;){
		switch(rpccall(rpc, "read", nil, 0)){
		case ARdone:
			return 0;
		case ARok:
			if(write(datafd, rpc->arg, rpc->narg) != rpc->narg){
				werrstr("write peer: %r");
				return -1;
			}
			break;
		case ARphase:
			n = 0;
			memset(buf, 0, sizeof buf);
			while((ret = rpccall(rpc, "write", buf, n)) == ARtoosmall){
				m = atoi(rpc->arg);
				if(m <= n || m > sizeof buf){
					werrstr("bad toosmall: %s", rpc->arg);
					return -1;
				}
				m -= n;
				m = read(datafd, buf+n, m);
				if(m <= 0){
					werrstr("read peer: %r");
					return -1;
				}
				n += m;
			}
			if(ret != ARok){
				werrstr("write: %s", rpc->arg);
				return -1;
			}
			break;
		default:
			werrstr("%s", rpc->arg);
			return -1;
		}
	}
}

static void
usage(void)
{
	fprint(2, "usage: livetest proto expect-secret [serverattrs] [clientattrs]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	char *clientattrs, *mnt, *proto, *rpcpath, *serverattrs;
	char cparams[1024], sparams[1024];
	int cfd, expect, p[2], secretlen, sfd;
	Rpc crpc, srpc;
	Waitmsg *w;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	if(argc < 2 || argc > 4)
		usage();

	proto = argv[0];
	expect = atoi(argv[1]);
	serverattrs = argc > 2 ? argv[2] : "";
	clientattrs = argc > 3 ? argv[3] : "";
	mnt = getenv("FACTOTUM_MNT");
	if(mnt == nil)
		mnt = "/mnt/factotum";

	rpcpath = smprint("%s/rpc", mnt);
	if(rpcpath == nil)
		sysfatal("smprint: %r");
	if(pipe(p) < 0)
		sysfatal("pipe: %r");
	sfd = open(rpcpath, ORDWR);
	if(sfd < 0)
		sysfatal("open %s: %r", rpcpath);
	cfd = open(rpcpath, ORDWR);
	if(cfd < 0)
		sysfatal("open %s: %r", rpcpath);
	free(rpcpath);

	memset(&srpc, 0, sizeof srpc);
	memset(&crpc, 0, sizeof crpc);
	srpc.fd = sfd;
	crpc.fd = cfd;

	snprint(sparams, sizeof sparams, "proto=%s role=server %s", proto, serverattrs);
	snprint(cparams, sizeof cparams, "proto=%s role=client %s", proto, clientattrs);

	switch(fork()){
	case -1:
		sysfatal("fork: %r");
	case 0:
		close(p[0]);
		if(runproxy(&srpc, sparams, p[1]) < 0)
			sysfatal("server: %r");
		close(p[1]);
		close(sfd);
		close(cfd);
		exits(nil);
	default:
		close(p[1]);
		if(runproxy(&crpc, cparams, p[0]) < 0)
			sysfatal("client: %r");
		secretlen = authsecretlen(&crpc);
		if(secretlen < 0)
			sysfatal("authinfo: %r");
		if(secretlen != expect)
			sysfatal("secret length %d != %d", secretlen, expect);
		close(p[0]);
		w = wait();
		if(w != nil && w->msg[0] != 0)
			sysfatal("wait: %s", w->msg);
		free(w);
		print("ok secret=%d\n", secretlen);
		break;
	}

	close(sfd);
	close(cfd);
	exits(nil);
}
