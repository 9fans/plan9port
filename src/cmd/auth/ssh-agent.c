/*
 * Present factotum in ssh agent clothing.
 */
#include <u.h>
#include <libc.h>
#include <mp.h>
#include <libsec.h>
#include <auth.h>
#include <thread.h>
#include <9pclient.h>

enum
{
	STACK = 65536
};
enum		/* agent protocol packet types */
{
	SSH_AGENTC_NONE = 0,
	SSH_AGENTC_REQUEST_RSA_IDENTITIES,
	SSH_AGENT_RSA_IDENTITIES_ANSWER,
	SSH_AGENTC_RSA_CHALLENGE,
	SSH_AGENT_RSA_RESPONSE,
	SSH_AGENT_FAILURE,
	SSH_AGENT_SUCCESS,
	SSH_AGENTC_ADD_RSA_IDENTITY,
	SSH_AGENTC_REMOVE_RSA_IDENTITY,
	SSH_AGENTC_REMOVE_ALL_RSA_IDENTITIES,

	SSH2_AGENTC_REQUEST_IDENTITIES = 11,
	SSH2_AGENT_IDENTITIES_ANSWER,
	SSH2_AGENTC_SIGN_REQUEST,
	SSH2_AGENT_SIGN_RESPONSE,

	SSH2_AGENTC_ADD_IDENTITY = 17,
	SSH2_AGENTC_REMOVE_IDENTITY,
	SSH2_AGENTC_REMOVE_ALL_IDENTITIES,
	SSH2_AGENTC_ADD_SMARTCARD_KEY,
	SSH2_AGENTC_REMOVE_SMARTCARD_KEY,

	SSH_AGENTC_LOCK,
	SSH_AGENTC_UNLOCK,
	SSH_AGENTC_ADD_RSA_ID_CONSTRAINED,
	SSH2_AGENTC_ADD_ID_CONSTRAINED,
	SSH_AGENTC_ADD_SMARTCARD_KEY_CONSTRAINED,

	SSH_AGENT_CONSTRAIN_LIFETIME = 1,
	SSH_AGENT_CONSTRAIN_CONFIRM = 2,

	SSH2_AGENT_FAILURE = 30,

	SSH_COM_AGENT2_FAILURE = 102,
	SSH_AGENT_OLD_SIGNATURE = 0x01
};

typedef struct Aconn Aconn;
struct Aconn
{
	uchar *data;
	uint ndata;
	int ctl;
	int fd;
	char dir[40];
};

typedef struct Msg Msg;
struct Msg
{
	uchar *bp;
	uchar *p;
	uchar *ep;
	int bpalloc;
};

char adir[40];
int afd;
int chatty;
char *factotum = "factotum";

void		agentproc(void *v);
void*	emalloc(int n);
void*	erealloc(void *v, int n);
void		listenproc(void *v);
int		runmsg(Aconn *a);
void		listkeystext(void);

void
usage(void)
{
	fprint(2, "usage: 9 ssh-agent [-D] [factotum]\n");
	threadexitsall("usage");
}

int
threadmaybackground(void)
{
	return 1;
}

void
threadmain(int argc, char **argv)
{
	int fd, pid, export, dotextlist;
	char dir[100], *ns;
	char sock[200], addr[200];
	uvlong x;

	export = 0;
	dotextlist = 0;
	pid = getpid();
	fmtinstall('B', mpfmt);
	fmtinstall('H', encodefmt);
	fmtinstall('[', encodefmt);

	ARGBEGIN{
	case '9':
		chatty9pclient++;
		break;
	case 'D':
		chatty++;
		break;
	case 'e':
		export = 1;
		break;
	case 'l':
		dotextlist = 1;
		break;
	default:
		usage();
	}ARGEND

	if(argc > 1)
		usage();
	if(argc == 1)
		factotum = argv[0];

	if(dotextlist)
		listkeystext();

	ns = getns();
	snprint(sock, sizeof sock, "%s/ssh-agent.socket", ns);
	if(0){
		x = ((uvlong)fastrand()<<32) | fastrand();
		x ^= ((uvlong)fastrand()<<32) | fastrand();
		snprint(dir, sizeof dir, "/tmp/ssh-%llux", x);
		if((fd = create(dir, OREAD, DMDIR|0700)) < 0)
			sysfatal("mkdir %s: %r", dir);
		close(fd);
		snprint(sock, sizeof sock, "%s/agent.%d", dir, pid);
	}
	snprint(addr, sizeof addr, "unix!%s", sock);

	if((afd = announce(addr, adir)) < 0)
		sysfatal("announce %s: %r", addr);

	print("SSH_AUTH_SOCK=%s;\n", sock);
	if(export)
		print("export SSH_AUTH_SOCK;\n");
	print("SSH_AGENT_PID=%d;\n", pid);
	if(export)
		print("export SSH_AGENT_PID;\n");
	close(1);
	rfork(RFNOTEG);
	proccreate(listenproc, nil, STACK);
	threadexits(0);
}

void
listenproc(void *v)
{
	Aconn *a;

	USED(v);
	for(;;){
		a = emalloc(sizeof *a);
		a->ctl = listen(adir, a->dir);
		if(a->ctl < 0)
			sysfatal("listen: %r");
		proccreate(agentproc, a, STACK);
	}
}

void
agentproc(void *v)
{
	Aconn *a;
	int n;

	a = v;
	a->fd = accept(a->ctl, a->dir);
	close(a->ctl);
	a->ctl = -1;
	for(;;){
		a->data = erealloc(a->data, a->ndata+1024);
		n = read(a->fd, a->data+a->ndata, 1024);
		if(n <= 0)
			break;
		a->ndata += n;
		while(runmsg(a))
			;
	}
	close(a->fd);
	free(a);
	threadexits(nil);
}

int
get1(Msg *m)
{
	if(m->p >= m->ep)
		return 0;
	return *m->p++;
}

int
get2(Msg *m)
{
	uint x;

	if(m->p+2 > m->ep)
		return 0;
	x = (m->p[0]<<8)|m->p[1];
	m->p += 2;
	return x;
}

int
get4(Msg *m)
{
	uint x;
	if(m->p+4 > m->ep)
		return 0;
	x = (m->p[0]<<24)|(m->p[1]<<16)|(m->p[2]<<8)|m->p[3];
	m->p += 4;
	return x;
}

uchar*
getn(Msg *m, uint n)
{
	uchar *p;

	if(m->p+n > m->ep)
		return nil;
	p = m->p;
	m->p += n;
	return p;
}

char*
getstr(Msg *m)
{
	uint n;
	uchar *p;

	n = get4(m);
	p = getn(m, n);
	if(p == nil)
		return nil;
	p--;
	memmove(p, p+1, n);
	p[n] = 0;
	return (char*)p;
}

mpint*
getmp(Msg *m)
{
	int n;
	uchar *p;

	n = (get2(m)+7)/8;
	if((p=getn(m, n)) == nil)
		return nil;
	return betomp(p, n, nil);
}

mpint*
getmp2(Msg *m)
{
	int n;
	uchar *p;

	n = get4(m);
	if((p = getn(m, n)) == nil)
		return nil;
	return betomp(p, n, nil);
}

void
newmsg(Msg *m)
{
	memset(m, 0, sizeof *m);
}

void
mreset(Msg *m)
{
	if(m->bpalloc){
		memset(m->bp, 0, m->ep-m->bp);
		free(m->bp);
	}
	memset(m, 0, sizeof *m);
}

Msg*
getm(Msg *m, Msg *mm)
{
	uint n;
	uchar *p;

	n = get4(m);
	if((p = getn(m, n)) == nil)
		return nil;
	mm->bp = p;
	mm->p = p;
	mm->ep = p+n;
	mm->bpalloc = 0;
	return mm;
}

uchar*
ensure(Msg *m, int n)
{
	int len;
	uchar *p;
	uchar *obp;

	if(m->bp == nil)
		m->bpalloc = 1;
	if(!m->bpalloc){
		p = emalloc(m->ep - m->bp);
		memmove(p, m->bp, m->ep - m->bp);
		obp = m->bp;
		m->bp = p;
		m->ep += m->bp - obp;
		m->p += m->bp - obp;
		m->bpalloc = 1;
	}
	len = m->ep - m->bp;
	if(m->p+n > m->ep){
		obp = m->bp;
		m->bp = erealloc(m->bp, len+n+1024);
		m->p += m->bp - obp;
		m->ep += m->bp - obp;
		m->ep += n+1024;
	}
	p = m->p;
	m->p += n;
	return p;
}

void
put4(Msg *m, uint n)
{
	uchar *p;

	p = ensure(m, 4);
	p[0] = (n>>24)&0xFF;
	p[1] = (n>>16)&0xFF;
	p[2] = (n>>8)&0xFF;
	p[3] = n&0xFF;
}

void
put2(Msg *m, uint n)
{
	uchar *p;

	p = ensure(m, 2);
	p[0] = (n>>8)&0xFF;
	p[1] = n&0xFF;
}

void
put1(Msg *m, uint n)
{
	uchar *p;

	p = ensure(m, 1);
	p[0] = n&0xFF;
}

void
putn(Msg *m, void *a, uint n)
{
	uchar *p;

	p = ensure(m, n);
	memmove(p, a, n);
}

void
putmp(Msg *m, mpint *b)
{
	int bits, n;
	uchar *p;

	bits = mpsignif(b);
	put2(m, bits);
	n = (bits+7)/8;
	p = ensure(m, n);
	mptobe(b, p, n, nil);
}

void
putmp2(Msg *m, mpint *b)
{
	int bits, n;
	uchar *p;

	if(mpcmp(b, mpzero) == 0){
		put4(m, 0);
		return;
	}
	bits = mpsignif(b);
	n = (bits+7)/8;
	if(bits%8 == 0){
		put4(m, n+1);
		put1(m, 0);
	}else
		put4(m, n);
	p = ensure(m, n);
	mptobe(b, p, n, nil);
}

void
putstr(Msg *m, char *s)
{
	int n;

	n = strlen(s);
	put4(m, n);
	putn(m, s, n);
}

void
putm(Msg *m, Msg *mm)
{
	uint n;

	n = mm->p - mm->bp;
	put4(m, n);
	putn(m, mm->bp, n);
}

void
newreply(Msg *m, int type)
{
	memset(m, 0, sizeof *m);
	put4(m, 0);
	put1(m, type);
}

void
reply(Aconn *a, Msg *m)
{
	uint n;
	uchar *p;

	n = (m->p - m->bp) - 4;
	p = m->bp;
	p[0] = (n>>24)&0xFF;
	p[1] = (n>>16)&0xFF;
	p[2] = (n>>8)&0xFF;
	p[3] = n&0xFF;
	if(chatty)
		fprint(2, "respond %d t=%d: %.*H\n", n, p[4], n, m->bp+4);
	write(a->fd, p, n+4);
	mreset(m);
}

typedef struct Key Key;
struct Key
{
	mpint *mod;
	mpint *ek;
	char *comment;
};

static char*
find(char **f, int nf, char *k)
{
	int i, len;

	len = strlen(k);
	for(i=1; i<nf; i++)	/* i=1: f[0] is "key" */
		if(strncmp(f[i], k, len) == 0 && f[i][len] == '=')
			return f[i]+len+1;
	return nil;
}

static int
putrsa1(Msg *m, char **f, int nf)
{
	char *p;
	mpint *mod, *ek;

	p = find(f, nf, "n");
	if(p == nil || (mod = strtomp(p, nil, 16, nil)) == nil)
		return -1;
	p = find(f, nf, "ek");
	if(p == nil || (ek = strtomp(p, nil, 16, nil)) == nil){
		mpfree(mod);
		return -1;
	}
	p = find(f, nf, "comment");
	if(p == nil)
		p = "";
	put4(m, mpsignif(mod));
	putmp(m, ek);
	putmp(m, mod);
	putstr(m, p);
	mpfree(mod);
	mpfree(ek);
	return 0;
}

void
printattr(char **f, int nf)
{
	int i;

	print("#");
	for(i=0; i<nf; i++)
		print(" %s", f[i]);
	print("\n");
}

void
printrsa1(char **f, int nf)
{
	char *p;
	mpint *mod, *ek;

	p = find(f, nf, "n");
	if(p == nil || (mod = strtomp(p, nil, 16, nil)) == nil)
		return;
	p = find(f, nf, "ek");
	if(p == nil || (ek = strtomp(p, nil, 16, nil)) == nil){
		mpfree(mod);
		return;
	}
	p = find(f, nf, "comment");
	if(p == nil)
		p = "";

	if(chatty)
		printattr(f, nf);
	print("%d %.10B %.10B %s\n", mpsignif(mod), ek, mod, p);
	mpfree(ek);
	mpfree(mod);
}

static int
putrsa(Msg *m, char **f, int nf)
{
	char *p;
	mpint *mod, *ek;

	p = find(f, nf, "n");
	if(p == nil || (mod = strtomp(p, nil, 16, nil)) == nil)
		return -1;
	p = find(f, nf, "ek");
	if(p == nil || (ek = strtomp(p, nil, 16, nil)) == nil){
		mpfree(mod);
		return -1;
	}
	putstr(m, "ssh-rsa");
	putmp2(m, ek);
	putmp2(m, mod);
	mpfree(ek);
	mpfree(mod);
	return 0;
}

RSApub*
getrsapub(Msg *m)
{
	RSApub *k;

	k = rsapuballoc();
	if(k == nil)
		return nil;
	k->ek = getmp2(m);
	k->n = getmp2(m);
	if(k->ek == nil || k->n == nil){
		rsapubfree(k);
		return nil;
	}
	return k;
}

static int
putdsa(Msg *m, char **f, int nf)
{
	char *p;
	int ret;
	mpint *dp, *dq, *dalpha, *dkey;

	ret = -1;
	dp = dq = dalpha = dkey = nil;
	p = find(f, nf, "p");
	if(p == nil || (dp = strtomp(p, nil, 16, nil)) == nil)
		goto out;
	p = find(f, nf, "q");
	if(p == nil || (dq = strtomp(p, nil, 16, nil)) == nil)
		goto out;
	p = find(f, nf, "alpha");
	if(p == nil || (dalpha = strtomp(p, nil, 16, nil)) == nil)
		goto out;
	p = find(f, nf, "key");
	if(p == nil || (dkey = strtomp(p, nil, 16, nil)) == nil)
		goto out;
	putstr(m, "ssh-dss");
	putmp2(m, dp);
	putmp2(m, dq);
	putmp2(m, dalpha);
	putmp2(m, dkey);
	ret = 0;
out:
	mpfree(dp);
	mpfree(dq);
	mpfree(dalpha);
	mpfree(dkey);
	return ret;
}

static int
putkey2(Msg *m, int (*put)(Msg*,char**,int), char **f, int nf)
{
	char *p;
	Msg mm;

	newmsg(&mm);
	if(put(&mm, f, nf) < 0)
		return -1;
	putm(m, &mm);
	mreset(&mm);
	p = find(f, nf, "comment");
	if(p == nil)
		p = "";
	putstr(m, p);
	return 0;
}

static int
printkey(char *type, int (*put)(Msg*,char**,int), char **f, int nf)
{
	Msg m;
	char *p;

	newmsg(&m);
	if(put(&m, f, nf) < 0)
		return -1;
	p = find(f, nf, "comment");
	if(p == nil)
		p = "";
	if(chatty)
		printattr(f, nf);
	print("%s %.*[ %s\n", type, m.p-m.bp, m.bp, p);
	mreset(&m);
	return 0;
}

DSApub*
getdsapub(Msg *m)
{
	DSApub *k;

	k = dsapuballoc();
	if(k == nil)
		return nil;
	k->p = getmp2(m);
	k->q = getmp2(m);
	k->alpha = getmp2(m);
	k->key = getmp2(m);
	if(!k->p || !k->q || !k->alpha || !k->key){
		dsapubfree(k);
		return nil;
	}
	return k;
}

static int
listkeys(Msg *m, int version)
{
	char buf[8192+1], *line[100], *f[20], *p, *s;
	int pnk;
	int i, n, nl, nf, nk;
	CFid *fid;

	nk = 0;
	pnk = m->p - m->bp;
	put4(m, 0);
	if((fid = nsopen(factotum, nil, "ctl", OREAD)) == nil){
		fprint(2, "ssh-agent: open factotum: %r\n");
		return -1;
	}
	for(;;){
		if((n = fsread(fid, buf, sizeof buf-1)) <= 0)
			break;
		buf[n] = 0;
		nl = getfields(buf, line, nelem(line), 1, "\n");
		for(i=0; i<nl; i++){
			nf = tokenize(line[i], f, nelem(f));
			if(nf == 0 || strcmp(f[0], "key") != 0)
				continue;
			p = find(f, nf, "proto");
			if(p == nil)
				continue;
			s = find(f, nf, "service");
			if(s == nil)
				continue;

			if(version == 1 && strcmp(p, "rsa") == 0 && strcmp(s, "ssh") == 0)
				if(putrsa1(m, f, nf) >= 0)
					nk++;
			if(version == 2 && strcmp(p, "rsa") == 0 && strcmp(s, "ssh-rsa") == 0)
				if(putkey2(m, putrsa, f, nf) >= 0)
					nk++;
			if(version == 2 && strcmp(p, "dsa") == 0 && strcmp(s, "ssh-dss") == 0)
				if(putkey2(m, putdsa, f, nf) >= 0)
					nk++;
		}
	}
	if(chatty)
		fprint(2, "sending %d keys\n", nk);
	fsclose(fid);
	m->bp[pnk+0] = (nk>>24)&0xFF;
	m->bp[pnk+1] = (nk>>16)&0xFF;
	m->bp[pnk+2] = (nk>>8)&0xFF;
	m->bp[pnk+3] = nk&0xFF;
	return nk;
}

void
listkeystext(void)
{
	char buf[8192+1], *line[100], *f[20], *p, *s;
	int i, n, nl, nf;
	CFid *fid;

	if((fid = nsopen(factotum, nil, "ctl", OREAD)) == nil){
		fprint(2, "ssh-agent: open factotum: %r\n");
		return;
	}
	for(;;){
		if((n = fsread(fid, buf, sizeof buf-1)) <= 0)
			break;
		buf[n] = 0;
		nl = getfields(buf, line, nelem(line), 1, "\n");
		for(i=0; i<nl; i++){
			nf = tokenize(line[i], f, nelem(f));
			if(nf == 0 || strcmp(f[0], "key") != 0)
				continue;
			p = find(f, nf, "proto");
			if(p == nil)
				continue;
			s = find(f, nf, "service");
			if(s == nil)
				continue;

			if(strcmp(p, "rsa") == 0 && strcmp(s, "ssh") == 0)
				printrsa1(f, nf);
			if(strcmp(p, "rsa") == 0 && strcmp(s, "ssh-rsa") == 0)
				printkey("ssh-rsa", putrsa, f, nf);
			if(strcmp(p, "dsa") == 0 && strcmp(s, "ssh-dss") == 0)
				printkey("ssh-dss", putdsa, f, nf);
		}
	}
	fsclose(fid);
	threadexitsall(nil);
}

mpint*
rsaunpad(mpint *b)
{
	int i, n;
	uchar buf[2560];

	n = (mpsignif(b)+7)/8;
	if(n > sizeof buf){
		werrstr("rsaunpad: too big");
		return nil;
	}
	mptobe(b, buf, n, nil);

	/* the initial zero has been eaten by the betomp -> mptobe sequence */
	if(buf[0] != 2){
		werrstr("rsaunpad: expected leading 2");
		return nil;
	}
	for(i=1; i<n; i++)
		if(buf[i]==0)
			break;
	return betomp(buf+i, n-i, nil);
}

void
mptoberjust(mpint *b, uchar *buf, int len)
{
	int n;

	n = mptobe(b, buf, len, nil);
	assert(n >= 0);
	if(n < len){
		len -= n;
		memmove(buf+len, buf, n);
		memset(buf, 0, len);
	}
}

static int
dorsa(Aconn *a, mpint *mod, mpint *exp, mpint *chal, uchar chalbuf[32])
{
	AuthRpc *rpc;
	char buf[4096], *p;
	mpint *decr, *unpad;

	USED(exp);
	if((rpc = auth_allocrpc()) == nil){
		fprint(2, "ssh-agent: auth_allocrpc: %r\n");
		return -1;
	}
	snprint(buf, sizeof buf, "proto=rsa service=ssh role=decrypt n=%lB ek=%lB", mod, exp);
	if(chatty)
		fprint(2, "ssh-agent: start %s\n", buf);
	if(auth_rpc(rpc, "start", buf, strlen(buf)) != ARok){
		fprint(2, "ssh-agent: auth 'start' failed: %r\n");
	Die:
		auth_freerpc(rpc);
		return -1;
	}

	p = mptoa(chal, 16, nil, 0);
	if(p == nil){
		fprint(2, "ssh-agent: dorsa: mptoa: %r\n");
		goto Die;
	}
	if(chatty)
		fprint(2, "ssh-agent: challenge %B => %s\n", chal, p);
	if(auth_rpc(rpc, "writehex", p, strlen(p)) != ARok){
		fprint(2, "ssh-agent: dorsa: auth 'write': %r\n");
		free(p);
		goto Die;
	}
	free(p);
	if(auth_rpc(rpc, "readhex", nil, 0) != ARok){
		fprint(2, "ssh-agent: dorsa: auth 'read': %r\n");
		goto Die;
	}
	decr = strtomp(rpc->arg, nil, 16, nil);
	if(chatty)
		fprint(2, "ssh-agent: response %s => %B\n", rpc->arg, decr);
	if(decr == nil){
		fprint(2, "ssh-agent: dorsa: strtomp: %r\n");
		goto Die;
	}
	unpad = rsaunpad(decr);
	if(chatty)
		fprint(2, "ssh-agent: unpad %B => %B\n", decr, unpad);
	if(unpad == nil){
		fprint(2, "ssh-agent: dorsa: rsaunpad: %r\n");
		mpfree(decr);
		goto Die;
	}
	mpfree(decr);
	mptoberjust(unpad, chalbuf, 32);
	mpfree(unpad);
	auth_freerpc(rpc);
	return 0;
}

int
keysign(Msg *mkey, Msg *mdata, Msg *msig)
{
	char *s;
	AuthRpc *rpc;
	RSApub *rsa;
	DSApub *dsa;
	char buf[4096];
	uchar digest[SHA1dlen];

	s = getstr(mkey);
	if(strcmp(s, "ssh-rsa") == 0){
		rsa = getrsapub(mkey);
		if(rsa == nil)
			return -1;
		snprint(buf, sizeof buf, "proto=rsa service=ssh-rsa role=sign n=%lB ek=%lB",
			rsa->n, rsa->ek);
		rsapubfree(rsa);
	}else if(strcmp(s, "ssh-dss") == 0){
		dsa = getdsapub(mkey);
		if(dsa == nil)
			return -1;
		snprint(buf, sizeof buf, "proto=dsa service=ssh-dss role=sign p=%lB q=%lB alpha=%lB key=%lB",
			dsa->p, dsa->q, dsa->alpha, dsa->key);
		dsapubfree(dsa);
	}else{
		fprint(2, "ssh-agent: cannot sign key type %s\n", s);
		werrstr("unknown key type %s", s);
		return -1;
	}

	if((rpc = auth_allocrpc()) == nil){
		fprint(2, "ssh-agent: auth_allocrpc: %r\n");
		return -1;
	}
	if(chatty)
		fprint(2, "ssh-agent: start %s\n", buf);
	if(auth_rpc(rpc, "start", buf, strlen(buf)) != ARok){
		fprint(2, "ssh-agent: auth 'start' failed: %r\n");
	Die:
		auth_freerpc(rpc);
		return -1;
	}
	sha1(mdata->bp, mdata->ep-mdata->bp, digest, nil);
	if(auth_rpc(rpc, "write", digest, SHA1dlen) != ARok){
		fprint(2, "ssh-agent: auth 'write in sign failed: %r\n");
		goto Die;
	}
	if(auth_rpc(rpc, "read", nil, 0) != ARok){
		fprint(2, "ssh-agent: auth 'read' failed: %r\n");
		goto Die;
	}
	newmsg(msig);
	putstr(msig, s);
	put4(msig, rpc->narg);
	putn(msig, rpc->arg, rpc->narg);
	auth_freerpc(rpc);
	return 0;
}

int
runmsg(Aconn *a)
{
	char *p;
	int n, nk, type, rt, vers;
	mpint *ek, *mod, *chal;
	uchar sessid[16], chalbuf[32], digest[MD5dlen];
	uint len, flags;
	DigestState *s;
	Msg m, mkey, mdata, msig;

	if(a->ndata < 4)
		return 0;
	len = (a->data[0]<<24)|(a->data[1]<<16)|(a->data[2]<<8)|a->data[3];
	if(a->ndata < 4+len)
		return 0;
	m.p = a->data+4;
	m.ep = m.p+len;
	type = get1(&m);
	if(chatty)
		fprint(2, "msg %d: %.*H\n", type, len, m.p);
	switch(type){
	default:
	Failure:
		newreply(&m, SSH_AGENT_FAILURE);
		reply(a, &m);
		break;

	case SSH_AGENTC_REQUEST_RSA_IDENTITIES:
		vers = 1;
		newreply(&m, SSH_AGENT_RSA_IDENTITIES_ANSWER);
		goto Identities;
	case SSH2_AGENTC_REQUEST_IDENTITIES:
		vers = 2;
		newreply(&m, SSH2_AGENT_IDENTITIES_ANSWER);
	Identities:
		nk = listkeys(&m, vers);
		if(nk < 0){
			mreset(&m);
			goto Failure;
		}
		if(chatty)
			fprint(2, "request identities\n", nk);
		reply(a, &m);
		break;

	case SSH_AGENTC_RSA_CHALLENGE:
		n = get4(&m);
		USED(n);
		ek = getmp(&m);
		mod = getmp(&m);
		chal = getmp(&m);
		if((p = (char*)getn(&m, 16)) == nil){
		Failchal:
			mpfree(ek);
			mpfree(mod);
			mpfree(chal);
			goto Failure;
		}
		memmove(sessid, p, 16);
		rt = get4(&m);
		if(rt != 1 || dorsa(a, mod, ek, chal, chalbuf) < 0)
			goto Failchal;
		s = md5(chalbuf, 32, nil, nil);
		if(s == nil)
			goto Failchal;
		md5(sessid, 16, digest, s);
		print("md5 %.*H %.*H => %.*H\n", 32, chalbuf, 16, sessid, MD5dlen, digest);

		newreply(&m, SSH_AGENT_RSA_RESPONSE);
		putn(&m, digest, 16);
		reply(a, &m);

		mpfree(ek);
		mpfree(mod);
		mpfree(chal);
		break;

	case SSH2_AGENTC_SIGN_REQUEST:
		if(getm(&m, &mkey) == nil
		|| getm(&m, &mdata) == nil)
			goto Failure;
		flags = get4(&m);
		if(flags & SSH_AGENT_OLD_SIGNATURE)
			goto Failure;
		if(keysign(&mkey, &mdata, &msig) < 0)
			goto Failure;
		if(chatty)
			fprint(2, "signature: %.*H\n",
				msig.p-msig.bp, msig.bp);
		newreply(&m, SSH2_AGENT_SIGN_RESPONSE);
		putm(&m, &msig);
		mreset(&msig);
		reply(a, &m);
		break;

	case SSH_AGENTC_ADD_RSA_IDENTITY:
		/*
			msg: n[4] mod[mp] pubexp[exp] privexp[mp]
				p^-1 mod q[mp] p[mp] q[mp] comment[str]
		 */
		goto Failure;

	case SSH_AGENTC_REMOVE_RSA_IDENTITY:
		/*
			msg: n[4] mod[mp] pubexp[mp]
		 */
		goto Failure;

	}

	a->ndata -= 4+len;
	memmove(a->data, a->data+4+len, a->ndata);
	return 1;
}

void*
emalloc(int n)
{
	void *v;

	v = mallocz(n, 1);
	if(v == nil){
		abort();
		sysfatal("out of memory allocating %d", n);
	}
	return v;
}

void*
erealloc(void *v, int n)
{
	v = realloc(v, n);
	if(v == nil){
		abort();
		sysfatal("out of memory reallocating %d", n);
	}
	return v;
}
