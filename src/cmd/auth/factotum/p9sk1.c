/*
 * p9sk1, p9sk2 - Plan 9 secret (private) key authentication.
 * p9sk2 is an incomplete flawed variant of p9sk1.
 *
 * Client protocol:
 *	write challenge[challen]	(p9sk1 only)
 *	read tickreq[tickreqlen]
 *	write ticket[ticketlen]
 *	read authenticator[authentlen]
 *
 * Server protocol:
 * 	read challenge[challen]	(p9sk1 only)
 *	write tickreq[tickreqlen]
 *	read ticket[ticketlen]
 *	write authenticator[authentlen]
 */

#include "std.h"
#include "dat.h"

extern Proto p9sk1, p9sk2;
static int gettickets(Ticketreq*, char*, Key*);

#define max(a, b) ((a) > (b) ? (a) : (b))
enum
{
	MAXAUTH = max(TICKREQLEN, TICKETLEN+max(TICKETLEN, AUTHENTLEN))
};

static int
p9skclient(Conv *c)
{
	char *user;
	char cchal[CHALLEN];
	uchar secret[8];
	char buf[MAXAUTH];
	int speakfor, ret;
	Attr *a;
	Authenticator au;
	Key *k;
	Ticket t;
	Ticketreq tr;

	ret = -1;
	a = nil;
	k = nil;

	/* p9sk1: send client challenge */
	if(c->proto == &p9sk1){
		c->state = "write challenge";
		memrandom(cchal, CHALLEN);
		if(convwrite(c, cchal, CHALLEN) < 0)
			goto out;
	}

	/* read ticket request */
	c->state = "read tickreq";
	if(convread(c, buf, TICKREQLEN) < 0)
		goto out;
	convM2TR(buf, &tr);

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
	a = addattr(a, "proto=p9sk1");

	if(strcmp(c->sysuser, owner) == 0){
		speakfor = 0;
		a = addattr(a, "proto=p9sk1 user? dom=%q", tr.authdom);
	}else if(user==nil || strcmp(c->sysuser, user)==0){
		speakfor = 1;
		a = delattr(a, "user");
		a = addattr(a, "proto=p9sk1 user? dom=%q role=speakfor", tr.authdom);
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
		if(gettickets(&tr, buf, k) < 0)
			goto out;

		convM2T(buf, &t, k->priv);
		if(t.num == AuthTc)
			break;

		/* we don't agree with the auth server about the key; try again */
		c->state = "replace key";
		if((k = keyreplace(c, k, "key mismatch with auth server")) == nil){
			werrstr("key mismatch with auth server");
			goto out;
		}
	}

	/* send second ticket and authenticator to server */
	c->state = "write ticket+auth";
	memmove(buf, buf+TICKETLEN, TICKETLEN);
	au.num = AuthAc;
	memmove(au.chal, tr.chal, CHALLEN);
	au.id = 0;
	convA2M(&au, buf+TICKETLEN, t.key);
	if(convwrite(c, buf, TICKETLEN+AUTHENTLEN) < 0)
		goto out;

	/* read authenticator from server */
	c->state = "read auth";
	if(convread(c, buf, AUTHENTLEN) < 0)
		goto out;
	convM2A(buf, &au, t.key);
	if(au.num != AuthAs || memcmp(au.chal, cchal, CHALLEN) != 0 || au.id != 0){
		werrstr("server lies through his teeth");
		goto out;
	}

	/* success */
	c->attr = addcap(c->attr, c->sysuser, &t);
	flog("p9skclient success %A", c->attr);	/* before adding secret! */
	des56to64((uchar*)t.key, secret);
	c->attr = addattr(c->attr, "secret=%.8H", secret);
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
	uchar secret[8];
	int ret;
	Attr *a;
	Authenticator au;
	Key *k;
	Ticketreq tr;
	Ticket t;

	ret = -1;

	a = addattr(copyattr(c->attr), "user? dom?");
	a = addattr(a, "user? dom? proto=p9sk1");
	if((k = keyfetch(c, "%A", a)) == nil)
		goto out;

	/* p9sk1: read client challenge */
	if(c->proto == &p9sk1){
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
	if(convwrite(c, buf, TICKREQLEN) < 0)
		goto out;

	/* p9sk2: use server challenge as client challenge */
	if(c->proto == &p9sk2)
		memmove(cchal, tr.chal, sizeof tr.chal);

	/* read ticket+authenticator */
	if(convread(c, buf, TICKETLEN+AUTHENTLEN) < 0)
		goto out;

	convM2T(buf, &t, k->priv);
	if(t.num != AuthTs || memcmp(t.chal, tr.chal, CHALLEN) != 0){
		/* BUG badkey */
		werrstr("key mismatch with auth server");
		goto out;
	}

	convM2A(buf+TICKETLEN, &au, t.key);
	if(au.num != AuthAc || memcmp(au.chal, tr.chal, CHALLEN) != 0 || au.id != 0){
		werrstr("client lies through his teeth");
		goto out;
	}

	/* send authenticator */
	au.num = AuthAs;
	memmove(au.chal, cchal, CHALLEN);
	convA2M(&au, buf, t.key);
	if(convwrite(c, buf, AUTHENTLEN) < 0)
		goto out;

	/* success */
	c->attr = addcap(c->attr, c->sysuser, &t);
	flog("p9skserver success %A", c->attr);	/* before adding secret! */
	des56to64((uchar*)t.key, secret);
	c->attr = addattr(c->attr, "secret=%.8H", secret);
	ret = 0;

out:
	if(ret < 0)
		flog("p9skserver: %r");
	freeattr(a);
	keyclose(k);
	return ret;
}

int
_asgetticket(int fd, char *trbuf, char *tbuf)
{
	if(write(fd, trbuf, TICKREQLEN) < 0){
		close(fd);
		return -1;
	}
	return _asrdresp(fd, tbuf, 2*TICKETLEN);
}
static int
getastickets(Ticketreq *tr, char *buf)
{
	int asfd;
	int ret;

	if((asfd = xioauthdial(nil, tr->authdom)) < 0)
		return -1;
	convTR2M(tr, buf);
	ret = xioasgetticket(asfd, buf, buf);
	xioclose(asfd);
	return ret;
}

static int
mktickets(Ticketreq *tr, char *buf, Key *k)
{
	Ticket t;

	if(strcmp(tr->authid, tr->hostid) != 0)
		return -1;

	memset(&t, 0, sizeof t);
	memmove(t.chal, tr->chal, CHALLEN);
	strcpy(t.cuid, tr->uid);
	strcpy(t.suid, tr->uid);
	memrandom(t.key, DESKEYLEN);
	t.num = AuthTc;
	convT2M(&t, buf, k->priv);
	t.num = AuthTs;
	convT2M(&t, buf+TICKETLEN, k->priv);
	return 0;
}

static int
gettickets(Ticketreq *tr, char *buf, Key *k)
{
	if(getastickets(tr, buf) == 0)
		return 0;
	if(mktickets(tr, buf, k) == 0)
		return 0;
	werrstr("gettickets: %r");
	return -1;
}

static int
p9sk1check(Key *k)
{
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

	k->priv = emalloc(DESKEYLEN);
	if(pass = strfindattr(k->privattr, "!password"))
		passtokey(k->priv, pass);
	else if(pass = strfindattr(k->privattr, "!hex")){
		if(hexparse(pass, k->priv, 7) < 0){
			werrstr("malformed !hex key data");
			return -1;
		}
	}else{
		werrstr("need !password or !hex attribute");
		return -1;
	}

	return 0;
}

static void
p9sk1close(Key *k)
{
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
	p9sk1check,
	p9sk1close
};

Proto p9sk2 = {
	"p9sk2",
	p9sk2roles
};
