#include "std.h"
#include "dat.h"
#include <bio.h>

int
memrandom(void *p, int n)
{
	uchar *cp;

	for(cp = (uchar*)p; n > 0; n--)
		*cp++ = fastrand();
	return 0;
}

/*
 *  create a change uid capability 
 */
static int caphashfd = -1;

static char*
mkcap(char *from, char *to)
{
	uchar rand[20];
	char *cap;
	char *key;
	int nfrom, nto;
	uchar hash[SHA1dlen];

	if(caphashfd < 0)
		return nil;

	/* create the capability */
	nto = strlen(to);
	nfrom = strlen(from);
	cap = emalloc(nfrom+1+nto+1+sizeof(rand)*3+1);
	sprint(cap, "%s@%s", from, to);
	memrandom(rand, sizeof(rand));
	key = cap+nfrom+1+nto+1;
	enc64(key, sizeof(rand)*3, rand, sizeof(rand));

	/* hash the capability */
	hmac_sha1((uchar*)cap, strlen(cap), (uchar*)key, strlen(key), hash, nil);

	/* give the kernel the hash */
	key[-1] = '@';
	if(write(caphashfd, hash, SHA1dlen) < 0){
		free(cap);
		return nil;
	}

	return cap;
}

Attr*
addcap(Attr *a, char *from, Ticket *t)
{
	char *cap;

	cap = mkcap(from, t->suid);
	return addattr(a, "cuid=%q suid=%q cap=%q", t->cuid, t->suid, cap);
}

/* bind in the default network and cs */
static int
bindnetcs(void)
{
	int srvfd;

	if(access("/net/tcp", AEXIST) < 0)
		bind("#I", "/net", MBEFORE);

	if(access("/net/cs", AEXIST) < 0){
		if((srvfd = open("#s/cs", ORDWR)) >= 0){
			/* mount closes srvfd on success */
			if(mount(srvfd, -1, "/net", MBEFORE, "") >= 0)
				return 0;
			close(srvfd);
		}
		return -1;
	}
	return 0;
}

int
_authdial(char *net, char *authdom)
{
	return authdial(net, authdom);
}

Key*
plan9authkey(Attr *a)
{
	char *dom;
	Key *k;

	/*
	 * The only important part of a is dom.
	 * We don't care, for example, about user name.
	 */
	dom = strfindattr(a, "dom");
	if(dom)
		k = keylookup("proto=p9sk1 role=server user? dom=%q", dom);
	else
		k = keylookup("proto=p9sk1 role=server user? dom?");
	if(k == nil)
		werrstr("could not find plan 9 auth key dom %q", dom);
	return k;
}
