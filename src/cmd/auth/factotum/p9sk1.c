/*
 * p9sk1, p9sk2, dp9ik - Plan 9 secret (private) key authentication.
 * p9sk2 is an incomplete flawed variant of p9sk1.
 * dp9ik uses AuthPAK to protect the password-derived key from offline
 * dictionary attacks.
 *
 * Client protocol:
 *	write challenge[challen]	(p9sk1, dp9ik)
 *	read pakreq[tickreqlen+pakylen]	(dp9ik only)
 *	write paky[pakylen]		(dp9ik only)
 *	read tickreq[tickreqlen]	(p9sk1, p9sk2)
 *	write ticket+authenticator
 *	read authenticator
 *
 * Server protocol:
 *	read challenge[challen]		(p9sk1, dp9ik)
 *	write pakreq[tickreqlen+pakylen]	(dp9ik only)
 *	read paky[pakylen]		(dp9ik only)
 *	write tickreq[tickreqlen]	(p9sk1, p9sk2)
 *	read ticket+authenticator
 *	write authenticator
 */

#include "std.h"
#include "dat.h"

extern Proto dp9ik, p9sk1, p9sk2;
static int gettickets(Ticketreq*, uchar*, char*, Key*);

#define max(a, b) ((a) > (b) ? (a) : (b))
enum
{
	MAXAUTH = 2*MAXTICKETLEN + MAXAUTHENTLEN,
	DP9SECRETLEN = 256
};

static char*
keyproto(Conv *c)
{
	if(c->proto == &p9sk2)
		return "p9sk1";
	return c->proto->name;
}

static int
ispak(Conv *c)
{
	return c->proto == &dp9ik;
}

static int
ticketlen(Conv *c)
{
	if(ispak(c))
		return MAXTICKETLEN;
	return TICKETLEN;
}

static int
authlen(Conv *c)
{
	if(ispak(c))
		return MAXAUTHENTLEN;
	return AUTHENTLEN;
}

static void
addsecret(Conv *c, Ticket *t, uchar crand[NONCELEN], uchar srand[NONCELEN])
{
	uchar rand[2*NONCELEN];
	uchar secret[DP9SECRETLEN];
	static uchar info[] = "Plan 9 session secret";

	if(!ispak(c)){
		des56to64((uchar*)t->key, secret);
		c->attr = addattr(c->attr, "secret=%.8H", secret);
		memset(secret, 0, 8);
		return;
	}

	memmove(rand, crand, NONCELEN);
	memmove(rand+NONCELEN, srand, NONCELEN);
	hkdf_x(rand, sizeof rand,
		info, sizeof(info)-1,
		t->key, NONCELEN,
		secret, sizeof secret,
		hmac_sha2_256, SHA2_256dlen);
	c->attr = addattr(c->attr, "secret=%.*H", sizeof secret, secret);
	memset(rand, 0, sizeof rand);
	memset(secret, 0, sizeof secret);
}

static int
p9skclient(Conv *c)
{
	char *user;
	char cchal[CHALLEN];
	uchar crand[NONCELEN], paky[PAKYLEN], srand[NONCELEN];
	char buf[MAXAUTH];
	int alen, msglen, speakfor, tlen, ret;
	Attr *a;
	Authenticator au;
	Key *k;
	Ticket t;
	Ticketreq tr;

	ret = -1;
	a = nil;
	k = nil;
	memset(crand, 0, sizeof crand);
	memset(paky, 0, sizeof paky);
	memset(srand, 0, sizeof srand);
	tlen = ticketlen(c);
	alen = authlen(c);
	msglen = tlen + alen;

	/* p9sk1 and dp9ik: send client challenge */
	if(c->proto != &p9sk2){
		c->state = "write challenge";
		memrandom(cchal, CHALLEN);
		if(convwrite(c, cchal, CHALLEN) < 0)
			goto out;
	}

	/* read ticket request or pak request */
	if(ispak(c)){
		c->state = "read pakreq";
		if(convread(c, buf, TICKREQLEN+PAKYLEN) < 0)
			goto out;
		memmove(paky, buf+TICKREQLEN, PAKYLEN);
	}else{
		c->state = "read tickreq";
		if(convread(c, buf, TICKREQLEN) < 0)
			goto out;
	}
	convM2TR(buf, &tr);
	if(tr.type != (ispak(c) ? AuthPAK : AuthTreq)){
		werrstr("bad ticket request");
		goto out;
	}

	/* p9sk2: use server challenge as client challenge */
	if(c->proto == &p9sk2)
		memmove(cchal, tr.chal, CHALLEN);

	/*
	 * find a key.
	 *
	 * if the user is the factotum owner, any key will do.
	 * if not, then if we have a speakfor key,
	 * we will only vouch for the user's local identity.
	 *
	 * this logic is duplicated in p9any.c
	 */
	user = strfindattr(c->attr, "user");
	a = delattr(copyattr(c->attr), "role");
	a = addattr(a, "proto=%q", keyproto(c));

	if(strcmp(c->sysuser, owner) == 0){
		speakfor = 0;
		a = addattr(a, "proto=%q user? dom=%q", keyproto(c), tr.authdom);
	}else if(user==nil || strcmp(c->sysuser, user)==0){
		speakfor = 1;
		a = delattr(a, "user");
		a = addattr(a, "proto=%q user? dom=%q role=speakfor", keyproto(c), tr.authdom);
	}else{
		werrstr("will not authenticate for %q as %q", c->sysuser, user);
		goto out;
	}

	for(;;){
		c->state = "find key";
		k = keyfetch(c, "%A", a);
		if(k == nil)
			goto out;

		/* relay ticket request to auth server, get tickets */
		strcpy(tr.hostid, strfindattr(k->attr, "user"));
		if(speakfor)
			strcpy(tr.uid, c->sysuser);
		else
			strcpy(tr.uid, tr.hostid);

		c->state = "get tickets";
		if(gettickets(&tr, ispak(c) ? paky : nil, buf, k) < 0)
			goto out;

		convM2T(buf, &t, k->priv);
		if(t.num == AuthTc && (!ispak(c) || t.form == 1))
			break;

		/* we don't agree with the auth server about the key; try again */
		c->state = "replace key";
		if((k = keyreplace(c, k, "key mismatch with auth server")) == nil){
			werrstr("key mismatch with auth server");
			goto out;
		}
	}

	if(ispak(c)){
		c->state = "write paky";
		if(convwrite(c, paky, PAKYLEN) < 0)
			goto out;
	}

	/* send second ticket and authenticator to server */
	c->state = "write ticket+auth";
	memmove(buf, buf+tlen, tlen);
	memset(&au, 0, sizeof au);
	au.num = AuthAc;
	memmove(au.chal, tr.chal, CHALLEN);
	if(ispak(c)){
		memrandom(crand, NONCELEN);
		memmove(au.rand, crand, NONCELEN);
	}else
		au.id = 0;
	convA2M(&au, buf+tlen, &t);
	if(convwrite(c, buf, msglen) < 0)
		goto out;

	/* read authenticator from server */
	c->state = "read auth";
	if(convread(c, buf, alen) < 0)
		goto out;
	convM2A(buf, &au, &t);
	if(au.num != AuthAs || memcmp(au.chal, cchal, CHALLEN) != 0
	|| (!ispak(c) && au.id != 0)){
		werrstr("server lies through his teeth");
		goto out;
	}
	if(ispak(c))
		memmove(srand, au.rand, NONCELEN);

	/* success */
	c->attr = addcap(c->attr, c->sysuser, &t);
	flog("p9skclient success %A", c->attr);	/* before adding secret! */
	addsecret(c, &t, crand, srand);
	ret = 0;

out:
	if(ret < 0)
		flog("p9skclient: %r");
	freeattr(a);
	keyclose(k);
	return ret;
}

static int
p9skserver(Conv *c)
{
	char cchal[CHALLEN], buf[MAXAUTH];
	uchar crand[NONCELEN], paky[PAKYLEN], srand[NONCELEN];
	int alen, msglen, ret, tlen;
	Attr *a;
	Authenticator au;
	Key *k;
	PAKpriv p;
	Ticketreq tr;
	Ticket t;

	ret = -1;
	memset(&p, 0, sizeof p);
	memset(crand, 0, sizeof crand);
	memset(paky, 0, sizeof paky);
	memset(srand, 0, sizeof srand);
	tlen = ticketlen(c);
	alen = authlen(c);
	msglen = tlen + alen;

	a = addattr(copyattr(c->attr), "user? dom?");
	a = addattr(a, "user? dom? proto=%q", keyproto(c));
	if((k = keyfetch(c, "%A", a)) == nil)
		goto out;

	/* p9sk1 and dp9ik: read client challenge */
	if(c->proto != &p9sk2){
		if(convread(c, cchal, CHALLEN) < 0)
			goto out;
	}

	/* send ticket request */
	memset(&tr, 0, sizeof tr);
	tr.type = AuthTreq;
	strcpy(tr.authid, strfindattr(k->attr, "user"));
	strcpy(tr.authdom, strfindattr(k->attr, "dom"));
	memrandom(tr.chal, sizeof tr.chal);
	convTR2M(&tr, buf);
	if(ispak(c)){
		tr.type = AuthPAK;
		convTR2M(&tr, buf);
		authpak_new(&p, k->priv, paky, 1);
		memmove(buf+TICKREQLEN, paky, PAKYLEN);
		if(convwrite(c, buf, TICKREQLEN+PAKYLEN) < 0)
			goto out;
	}else{
		if(convwrite(c, buf, TICKREQLEN) < 0)
			goto out;
	}

	/* p9sk2: use server challenge as client challenge */
	if(c->proto == &p9sk2)
		memmove(cchal, tr.chal, sizeof tr.chal);

	if(ispak(c)){
		if(convread(c, paky, PAKYLEN) < 0)
			goto out;
		if(authpak_finish(&p, k->priv, paky) < 0){
			werrstr("bad AuthPAK exchange");
			goto out;
		}
	}

	/* read ticket+authenticator */
	if(convread(c, buf, msglen) < 0)
		goto out;

	convM2T(buf, &t, k->priv);
	if(t.num != AuthTs || memcmp(t.chal, tr.chal, CHALLEN) != 0
	|| (ispak(c) && t.form != 1)){
		/* BUG badkey */
		werrstr("key mismatch with auth server");
		goto out;
	}

	convM2A(buf+tlen, &au, &t);
	if(au.num != AuthAc || memcmp(au.chal, tr.chal, CHALLEN) != 0
	|| (!ispak(c) && au.id != 0)){
		werrstr("client lies through his teeth");
		goto out;
	}
	if(ispak(c))
		memmove(crand, au.rand, NONCELEN);

	/* send authenticator */
	memset(&au, 0, sizeof au);
	au.num = AuthAs;
	memmove(au.chal, cchal, CHALLEN);
	if(ispak(c)){
		memrandom(srand, NONCELEN);
		memmove(au.rand, srand, NONCELEN);
	}
	convA2M(&au, buf, &t);
	if(convwrite(c, buf, alen) < 0)
		goto out;

	/* success */
	c->attr = addcap(c->attr, c->sysuser, &t);
	flog("p9skserver success %A", c->attr);	/* before adding secret! */
	addsecret(c, &t, crand, srand);
	ret = 0;

out:
	if(ret < 0)
		flog("p9skserver: %r");
	memset(&p, 0, sizeof p);
	freeattr(a);
	keyclose(k);
	return ret;
}

static int
getastickets(Ticketreq *tr, uchar *y, char *buf, Key *k)
{
	int asfd;
	int ret;
	PAKpriv p;
	uchar pakbuf[2*PAKYLEN];

	if((asfd = xioauthdial(nil, tr->authdom)) < 0)
		return -1;
	memset(&p, 0, sizeof p);
	if(y != nil){
		tr->type = AuthPAK;
		convTR2M(tr, buf);
		if(xiowrite(asfd, buf, TICKREQLEN) < 0
		|| xiowrite(asfd, y, PAKYLEN) < 0){
			ret = -1;
			goto out;
		}
		authpak_new(&p, k->priv, (uchar*)buf, 1);
		if(xiowrite(asfd, buf, PAKYLEN) < 0){
			ret = -1;
			goto out;
		}
		if((ret = xioasrdresp(asfd, (char*)pakbuf, sizeof pakbuf)) != sizeof pakbuf){
			ret = -1;
			goto out;
		}
		memmove(y, pakbuf, PAKYLEN);
		if(authpak_finish(&p, k->priv, pakbuf+PAKYLEN) < 0){
			werrstr("bad AuthPAK reply");
			ret = -1;
			goto out;
		}
	}
	tr->type = AuthTreq;
	if(y != nil)
		ret = xioasgetticket(asfd, tr, buf, 2*MAXTICKETLEN);
	else
		ret = xioasgetticket(asfd, tr, buf, 2*TICKETLEN);

out:
	memset(&p, 0, sizeof p);
	xioclose(asfd);
	return ret;
}

static int
mktickets(Ticketreq *tr, uchar *y, char *buf, Key *k)
{
	uchar paky[PAKYLEN];
	Ticket t;
	PAKpriv p;
	int n;

	if(strcmp(tr->authid, tr->hostid) != 0)
		return -1;

	memset(&p, 0, sizeof p);
	memset(&t, 0, sizeof t);
	if(y != nil){
		memmove(paky, y, PAKYLEN);
		t.form = 1;
		authpak_new(&p, k->priv, y, 0);
		if(authpak_finish(&p, k->priv, paky) < 0){
			memset(&p, 0, sizeof p);
			return -1;
		}
	}
	memmove(t.chal, tr->chal, CHALLEN);
	strcpy(t.cuid, tr->uid);
	strcpy(t.suid, tr->uid);
	memrandom(t.key, y != nil ? NONCELEN : DESKEYLEN);
	t.num = AuthTc;
	n = convT2M(&t, buf, k->priv);
	t.num = AuthTs;
	n += convT2M(&t, buf+n, k->priv);
	memset(&p, 0, sizeof p);
	return n;
}

static int
gettickets(Ticketreq *tr, uchar *y, char *buf, Key *k)
{
	int ret;

	if((ret = getastickets(tr, y, buf, k)) > 0)
		return ret;
	if((ret = mktickets(tr, y, buf, k)) > 0)
		return ret;
	werrstr("gettickets: %r");
	return -1;
}

static int
p9skcheck(Key *k)
{
	Authkey *ak;
	int nkey;
	char *user, *dom, *pass;
	Ticketreq tr;

	user = strfindattr(k->attr, "user");
	dom = strfindattr(k->attr, "dom");
	if(user==nil || dom==nil){
		werrstr("need user and dom attributes");
		return -1;
	}
	if(strlen(user) >= sizeof tr.authid){
		werrstr("user name too long");
		return -1;
	}
	if(strlen(dom) >= sizeof tr.authdom){
		werrstr("auth dom name too long");
		return -1;
	}

	ak = emalloc(sizeof(*ak));
	memset(ak, 0, sizeof(*ak));
	nkey = k->proto == &dp9ik ? AESKEYLEN : DESKEYLEN;
	if(pass = strfindattr(k->privattr, "!password"))
		passtokey(ak, pass);
	else if(pass = strfindattr(k->privattr, "!hex")){
		if(hexparse(pass, k->proto == &dp9ik ? ak->aes : (uchar*)ak->des, nkey) < 0){
			memset(ak, 0, sizeof(*ak));
			free(ak);
			werrstr("malformed !hex key data");
			return -1;
		}
	}else{
		free(ak);
		werrstr("need !password or !hex attribute");
		return -1;
	}
	if(k->proto == &dp9ik)
		authpak_hash(ak, user);
	k->priv = ak;

	return 0;
}

static void
p9skclose(Key *k)
{
	if(k->priv)
		memset(k->priv, 0, sizeof(Authkey));
	free(k->priv);
	k->priv = nil;
}

static Role
p9sk1roles[] =
{
	"client",	p9skclient,
	"server",	p9skserver,
	0
};

static Role
p9sk2roles[] =
{
	"client",	p9skclient,
	"server",	p9skserver,
	0
};

Proto p9sk1 = {
	"p9sk1",
	p9sk1roles,
	"user? dom? !password?",
	p9skcheck,
	p9skclose
};

Proto dp9ik = {
	"dp9ik",
	p9sk1roles,
	"user? dom? !password?",
	p9skcheck,
	p9skclose
};

Proto p9sk2 = {
	"p9sk2",
	p9sk2roles
};
