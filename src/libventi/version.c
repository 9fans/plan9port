#include <u.h>
#include <libc.h>
#include <venti.h>

static char *okvers[] = {
	"02",
	nil,
};

/*
static char EBigString[] = "string too long";
static char EBigPacket[] = "packet too long";
static char ENullString[] = "missing string";
*/
static char EBadVersion[] = "bad format in version string";

static int
vtreadversion(VtConn *z, char *q, char *v, int nv)
{
	int n;

	for(;;){
		if(nv <= 1){
			werrstr("version too long");
			return -1;
		}
		n = read(z->infd, v, 1);
		if(n <= 0){
			if(n == 0)
				werrstr("unexpected eof");
			return -1;
		}
		if(*v == '\n'){
			*v = 0;
			break;
		}
		if((uchar)*v < ' ' || (uchar)*v > 0x7f || (*q && *v != *q)){
			werrstr(EBadVersion);
			return -1;
		}
		v++;
		nv--;
		if(*q)
			q++;
	}
	return 0;
}

int
vtversion(VtConn *z)
{
	char buf[VtMaxStringSize], *p, *ep, *prefix, *pp;
	int i;

	qlock(&z->lk);
	if(z->state != VtStateAlloc){
		werrstr("bad session state");
		qunlock(&z->lk);
		return -1;
	}

	qlock(&z->inlk);
	qlock(&z->outlk);

	p = buf;
	ep = buf + sizeof buf;
	prefix = "venti-";
	p = seprint(p, ep, "%s", prefix);
	p += strlen(p);
	for(i=0; okvers[i]; i++)
		p = seprint(p, ep, "%s%s", i ? ":" : "", okvers[i]);
	p = seprint(p, ep, "-libventi\n");
	assert(p-buf < sizeof buf);

	if(write(z->outfd, buf, p-buf) != p-buf)
		goto Err;
	vtdebug(z, "version string out: %s", buf);

	if(vtreadversion(z, prefix, buf, sizeof buf) < 0)
		goto Err;
	vtdebug(z, "version string in: %s", buf);

	p = buf+strlen(prefix);
	for(; *p; p=pp){
		if(*p == ':' || *p == '-')
			p++;
		pp = strpbrk(p, ":-");
		if(pp == nil)
			pp = p+strlen(p);
		for(i=0; okvers[i]; i++)
			if(strlen(okvers[i]) == pp-p && memcmp(okvers[i], p, pp-p) == 0){
				*pp = 0;
				z->version = vtstrdup(p);
				goto Okay;
			}
	}
	werrstr("unable to negotiate version");
	goto Err;

Okay:
	z->state = VtStateConnected;
	qunlock(&z->inlk);
	qunlock(&z->outlk);
	qunlock(&z->lk);
	return 0;

Err:
	werrstr("vtversion: %r");
	if(z->infd >= 0)
		close(z->infd);
	if(z->outfd >= 0 && z->outfd != z->infd)
		close(z->outfd);
	z->infd = -1;
	z->outfd = -1;
	z->state = VtStateClosed;
	qunlock(&z->inlk);
	qunlock(&z->outlk);
	qunlock(&z->lk);
	return -1;
}
