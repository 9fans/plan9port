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
static int caphashfd;

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
	int vanilla;

	vanilla = net==nil || strcmp(net, "/net")==0;

	if(!vanilla || bindnetcs()>=0)
		return authdial(net, authdom);

	/* use the auth sever passed to us as an arg */
	if(authaddr == nil)
		return -1;
	return dial(netmkaddr(authaddr, "tcp", "567"), 0, 0, 0);
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

/*
 *  prompt for a string with a possible default response
 */
char*
readcons(char *prompt, char *def, int raw)
{
	int fdin, fdout, ctl, n;
	char line[10];
	char *s;

	fdin = open("/dev/cons", OREAD);
	if(fdin < 0)
		fdin = 0;
	fdout = open("/dev/cons", OWRITE);
	if(fdout < 0)
		fdout = 1;
	if(def != nil)
		fprint(fdout, "%s[%s]: ", prompt, def);
	else
		fprint(fdout, "%s: ", prompt);
	if(raw){
		ctl = open("/dev/consctl", OWRITE);
		if(ctl >= 0)
			write(ctl, "rawon", 5);
	} else
		ctl = -1;
	s = estrdup("");
	for(;;){
		n = read(fdin, line, 1);
		if(n == 0){
		Error:
			close(fdin);
			close(fdout);
			if(ctl >= 0)
				close(ctl);
			free(s);
			return nil;
		}
		if(n < 0)
			goto Error;
		if(line[0] == 0x7f)
			goto Error;
		if(n == 0 || line[0] == '\n' || line[0] == '\r'){
			if(raw){
				write(ctl, "rawoff", 6);
				write(fdout, "\n", 1);
			}
			close(ctl);
			close(fdin);
			close(fdout);
			if(*s == 0 && def != nil)
				s = estrappend(s, "%s", def);
			return s;
		}
		if(line[0] == '\b'){
			if(strlen(s) > 0)
				s[strlen(s)-1] = 0;
		} else if(line[0] == 0x15) {	/* ^U: line kill */
			if(def != nil)
				fprint(fdout, "\n%s[%s]: ", prompt, def);
			else
				fprint(fdout, "\n%s: ", prompt);
			
			s[0] = 0;
		} else {
			s = estrappend(s, "%c", line[0]);
		}
	}
	return nil; /* not reached */
}
