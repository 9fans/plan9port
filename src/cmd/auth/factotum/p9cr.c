/*
 * p9cr, vnc - one-sided challenge/response authentication
 *
 * Protocol:
 *
 *	C -> S: user
 *	S -> C: challenge
 *	C -> S: response
 *	S -> C: ok or bad
 *
 * Note that this is the protocol between factotum and the local
 * program, not between the two factotums.  The information 
 * exchanged here is wrapped in other protocols by the local
 * programs.
 */

#include "std.h"
#include "dat.h"

static int
p9crclient(Conv *c)
{
	char *chal, *pw, *res, *user;
	int astype, nchal, npw, ntry, ret;
	uchar resp[MD5dlen];
	Attr *attr;
	DigestState *ds;
	Key *k;
	
	chal = nil;
	k = nil;
	res = nil;
	ret = -1;
	attr = c->attr;

	if(c->proto == &p9cr){
		astype = AuthChal;
		challen = NETCHLEN;
	}else if(c->proto == &vnc){
		astype = AuthVnc;
		challen = MAXCHAL;
	}else{
		werrstr("bad proto");
		goto out;
	}

	c->state = "find key";
	k = keyfetch(c, "%A %s", attr, c->proto->keyprompt);
	if(k == nil)
		goto out;

	for(ntry=1;; ntry++){
		if(c->attr != attr)
			freeattr(c->attr);
		c->attr = addattrs(copyattr(attr), k->attr);
		if((pw = strfindattr(k->privattr, "!password")) == nil){
			werrstr("key has no !password (cannot happen)");
			goto out;
		}
		npw = strlen(pw);

		if((user = strfindattr(k->attr, "user")) == nil){
			werrstr("key has no user (cannot happen)");
			goto out;
		}

		if(convprint(c, "%s", user) < 0)
			goto out;

		if(convreadm(c, &chal) < 0)
			goto out;

		if((nresp = (*response)(chal, resp)) < 0)
			goto out;

		if(convwrite(c, resp, nresp) < 0)
			goto out;

		if(convreadm(c, &res) < 0)
			goto out;

		if(strcmp(res, "ok") == 0)
			break;

		if((k = keyreplace(c, k, "%s", res)) == nil){
			c->state = "auth failed";
			werrstr("%s", res);
			goto out;
		}
	}

	werrstr("succeeded");
	ret = 0;

out:
	keyclose(k);
	free(chal);
	if(c->attr != attr)
		freeattr(attr);
	return ret;
}

static int
p9crserver(Conv *c)
{
	char chal[MAXCHAL], *user, *resp;
	int astype, challen, asfd, fd, ret;
	Attr *a;
	Key *k;
	char *hostid, *dom;

	ret = -1;
	user = nil;
	resp = nil;
	memset(&s, 0, sizeof s);
	s.asfd = -1;

	if(c->proto == &p9cr){
		astype = AuthChal;
		challen = NETCHLEN;
	}else if(c->proto == &vnc){
		astype = AuthVnc;
		challen = MAXCHAL;
	}else{
		werrstr("bad proto");
		goto out;
	}

	c->state = "find key";
	if((k = plan9authkey(c->attr)) == nil)
		goto out;

/*
	a = copyattr(k->attr);
	a = delattr(a, "proto");
	c->attr = addattrs(c->attr, a);
	freeattr(a);
*/

	c->state = "authdial";
	hostid = strfindattr(s.k->attr, "user");
	dom = strfindattr(s.k->attr, "dom");
	if((asfd = xioauthdial(nil, s.dom)) < 0){
		werrstr("authdial %s: %r", s.dom);
		goto out;
	}

	c->state = "authchal";
	if(p9crchal(&s, astype, chal) < 0)
		goto out;

	c->state = "write challenge";
	if(convprint(c, "%s", chal) < 0)
		goto out;

	for(;;){
		c->state = "read user";
		if(convreadm(c, &user) < 0)
			goto out;

		c->state = "read response";
		if(convreadm(c, &resp) < 0)
			goto out;

		c->state = "authwrite";
		switch(apopresp(&s, user, resp)){
		case -1:
			goto out;
		case 0:
			c->state = "write status";
			if(convprint(c, "bad authentication failed") < 0)
				goto out;
			break;
		case 1:
			c->state = "write status";
			if(convprint(c, "ok") < 0)
				goto out;
			goto ok;
		}
		free(user);
		free(resp);
		user = nil;
		resp = nil;
	}

ok:
	ret = 0;
	c->attr = addcap(c->attr, c->sysuser, &s.t);

out:
	keyclose(s.k);
	free(user);
	free(resp);
	xioclose(s.asfd);
	return ret;
}

enum
{
	MAXCHAL = 64,
};

typedef struct State State;
struct State
{
	Key	*key;
	int	astype;
	int	asfd;
	Ticket	t;
	Ticketreq tr;
	char	chal[MAXCHAL];
	int	challen;
	char	resp[MAXCHAL];
	int	resplen;
};

enum
{
	CNeedChal,
	CHaveResp,

	SHaveChal,
	SNeedResp,

	Maxphase,
};

static char *phasenames[Maxphase] =
{
[CNeedChal]	"CNeedChal",
[CHaveResp]	"CHaveResp",

[SHaveChal]	"SHaveChal",
[SNeedResp]	"SNeedResp",
};

static void
p9crclose(Fsstate *fss)
{
	State *s;

	s = fss->ps;
	if(s->asfd >= 0){
		close(s->asfd);
		s->asfd = -1;
	}
	free(s);
}

static int getchal(State*, Fsstate*);

static int
p9crinit(Proto *p, Fsstate *fss)
{
	int iscli, ret;
	char *user;
	State *s;
	Attr *attr;

	if((iscli = isclient(_str_findattr(fss->attr, "role"))) < 0)
		return failure(fss, nil);
	
	s = emalloc(sizeof(*s));
	s->asfd = -1;
	if(p == &p9cr){
		s->astype = AuthChal;
		s->challen = NETCHLEN;
	}else if(p == &vnc){
		s->astype = AuthVNC;
		s->challen = Maxchal;
	}else
		abort();

	if(iscli){
		fss->phase = CNeedChal;
		if(p == &p9cr)
			attr = setattr(_copyattr(fss->attr), "proto=p9sk1");
		else
			attr = nil;
		ret = findkey(&s->key, fss, Kuser, 0, attr ? attr : fss->attr,
			"role=client %s", p->keyprompt);
		_freeattr(attr);
		if(ret != RpcOk){
			free(s);
			return ret;
		}
		fss->ps = s;
	}else{
		if((ret = findp9authkey(&s->key, fss)) != RpcOk){
			free(s);
			return ret;
		}
		if((user = _str_findattr(fss->attr, "user")) == nil){
			free(s);
			return failure(fss, "no user name specified in start msg");
		}
		if(strlen(user) >= sizeof s->tr.uid){
			free(s);
			return failure(fss, "user name too long");
		}
		fss->ps = s;
		strcpy(s->tr.uid, user);
		ret = getchal(s, fss);
		if(ret != RpcOk){
			p9crclose(fss);	/* frees s */
			fss->ps = nil;
		}
	}
	fss->phasename = phasenames;
	fss->maxphase = Maxphase;
	return ret;
}

static int
p9crread(Fsstate *fss, void *va, uint *n)
{
	int m;
	State *s;

	s = fss->ps;
	switch(fss->phase){
	default:
		return phaseerror(fss, "read");

	case CHaveResp:
		if(s->resplen < *n)
			*n = s->resplen;
		memmove(va, s->resp, *n);
		fss->phase = Established;
		return RpcOk;

	case SHaveChal:
		if(s->astype == AuthChal)
			m = strlen(s->chal);	/* ascii string */
		else
			m = s->challen;		/* fixed length binary */
		if(m > *n)
			return toosmall(fss, m);
		*n = m;
		memmove(va, s->chal, m);
		fss->phase = SNeedResp;
		return RpcOk;
	}
}

static int
p9response(Fsstate *fss, State *s)
{
	char key[DESKEYLEN];
	uchar buf[8];
	ulong chal;
	char *pw;

	pw = _str_findattr(s->key->privattr, "!password");
	if(pw == nil)
		return failure(fss, "vncresponse cannot happen");
	passtokey(key, pw);
	memset(buf, 0, 8);
	sprint((char*)buf, "%d", atoi(s->chal));
	if(encrypt(key, buf, 8) < 0)
		return failure(fss, "can't encrypt response");
	chal = (buf[0]<<24)+(buf[1]<<16)+(buf[2]<<8)+buf[3];
	s->resplen = snprint(s->resp, sizeof s->resp, "%.8lux", chal);
	return RpcOk;
}

static uchar tab[256];

/* VNC reverses the bits of each byte before using as a des key */
static void
mktab(void)
{
	int i, j, k;
	static int once;

	if(once)
		return;
	once = 1;

	for(i=0; i<256; i++) {
		j=i;
		tab[i] = 0;
		for(k=0; k<8; k++) {
			tab[i] = (tab[i]<<1) | (j&1);
			j >>= 1;
		}
	}
}

static int
vncaddkey(Key *k)
{
	uchar *p;
	char *s;

	k->priv = emalloc(8+1);
	if(s = _str_findattr(k->privattr, "!password")){
		mktab();
		memset(k->priv, 0, 8+1);
		strncpy((char*)k->priv, s, 8);
		for(p=k->priv; *p; p++)
			*p = tab[*p];
	}else{
		werrstr("no key data");
		return -1;
	}
	return replacekey(k);
}

static void
vncclosekey(Key *k)
{
	free(k->priv);
}

static int
vncresponse(Fsstate*, State *s)
{
	DESstate des;

	memmove(s->resp, s->chal, sizeof s->chal);
	setupDESstate(&des, s->key->priv, nil);
	desECBencrypt((uchar*)s->resp, s->challen, &des);
	s->resplen = s->challen;
	return RpcOk;
}

static int
p9crwrite(Fsstate *fss, void *va, uint n)
{
	char tbuf[TICKETLEN+AUTHENTLEN];
	State *s;
	char *data = va;
	Authenticator a;
	char resp[Maxchal];
	int ret;

	s = fss->ps;
	switch(fss->phase){
	default:
		return phaseerror(fss, "write");

	case CNeedChal:
		if(n >= sizeof(s->chal))
			return failure(fss, Ebadarg);
		memset(s->chal, 0, sizeof s->chal);
		memmove(s->chal, data, n);
		s->challen = n;

		if(s->astype == AuthChal)
			ret = p9response(fss, s);
		else
			ret = vncresponse(fss, s);
		if(ret != RpcOk)
			return ret;
		fss->phase = CHaveResp;
		return RpcOk;

	case SNeedResp:
		/* send response to auth server and get ticket */
		if(n > sizeof(resp))
			return failure(fss, Ebadarg);
		memset(resp, 0, sizeof resp);
		memmove(resp, data, n);
		if(write(s->asfd, resp, s->challen) != s->challen)
			return failure(fss, Easproto);

		/* get ticket plus authenticator from auth server */
		if(_asrdresp(s->asfd, tbuf, TICKETLEN+AUTHENTLEN) < 0)
			return failure(fss, nil);

		/* check ticket */
		convM2T(tbuf, &s->t, s->key->priv);
		if(s->t.num != AuthTs
		|| memcmp(s->t.chal, s->tr.chal, sizeof(s->t.chal)) != 0)
			return failure(fss, Easproto);
		convM2A(tbuf+TICKETLEN, &a, s->t.key);
		if(a.num != AuthAc
		|| memcmp(a.chal, s->tr.chal, sizeof(a.chal)) != 0
		|| a.id != 0)
			return failure(fss, Easproto);

		fss->haveai = 1;
		fss->ai.cuid = s->t.cuid;
		fss->ai.suid = s->t.suid;
		fss->ai.nsecret = 0;
		fss->ai.secret = nil;
		fss->phase = Established;
		return RpcOk;
	}
}

static int
getchal(State *s, Fsstate *fss)
{
	char trbuf[TICKREQLEN];
	int n;

	safecpy(s->tr.hostid, _str_findattr(s->key->attr, "user"), sizeof(s->tr.hostid));
	safecpy(s->tr.authdom, _str_findattr(s->key->attr, "dom"), sizeof(s->tr.authdom));
	s->tr.type = s->astype;
	convTR2M(&s->tr, trbuf);

	/* get challenge from auth server */
	s->asfd = _authdial(nil, _str_findattr(s->key->attr, "dom"));
	if(s->asfd < 0)
		return failure(fss, Easproto);
	if(write(s->asfd, trbuf, TICKREQLEN) != TICKREQLEN)
		return failure(fss, Easproto);
	n = _asrdresp(s->asfd, s->chal, s->challen);
	if(n <= 0){
		if(n == 0)
			werrstr("_asrdresp short read");
		return failure(fss, nil);
	}
	s->challen = n;
	fss->phase = SHaveChal;
	return RpcOk;
}

Proto p9cr =
{
.name=		"p9cr",
.init=		p9crinit,
.write=		p9crwrite,
.read=		p9crread,
.close=		p9crclose,
.keyprompt=	"user? !password?",
};

Proto vnc =
{
.name=		"vnc",
.init=		p9crinit,
.write=		p9crwrite,
.read=		p9crread,
.close=		p9crclose,
.keyprompt=	"!password?",
.addkey=	vncaddkey,
};
