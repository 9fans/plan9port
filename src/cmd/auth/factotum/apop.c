/*
 * APOP, CRAM - MD5 challenge/response authentication
 *
 * The client does not authenticate the server, hence no CAI.
 *
 * Protocol:
 *
 *	S -> C:	random@domain
 *	C -> S:	user hex-response
 *	S -> C:	ok
 *
 * Note that this is the protocol between factotum and the local
 * program, not between the two factotums.  The information
 * exchanged here is wrapped in the APOP protocol by the local
 * programs.
 *
 * If S sends "bad [msg]" instead of "ok", that is a hint that the key is bad.
 * The protocol goes back to "C -> S: user hex-response".
 */

#include "std.h"
#include "dat.h"

extern Proto apop, cram;

static int
apopcheck(Key *k)
{
	if(!strfindattr(k->attr, "user") || !strfindattr(k->privattr, "!password")){
		werrstr("need user and !password attributes");
		return -1;
	}
	return 0;
}

static int
apopclient(Conv *c)
{
	char *chal, *pw, *res;
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

	if(c->proto == &apop)
		astype = AuthApop;
	else if(c->proto == &cram)
		astype = AuthCram;
	else{
		werrstr("bad proto");
		goto out;
	}

	c->state = "find key";
	k = keyfetch(c, "%A %s", attr, c->proto->keyprompt);
	if(k == nil)
		goto out;

	c->state = "read challenge";
	if((nchal = convreadm(c, &chal)) < 0)
		goto out;

  	for(ntry=1;; ntry++){
		if(c->attr != attr)
			freeattr(c->attr);
		c->attr = addattrs(copyattr(attr), k->attr);
		if((pw = strfindattr(k->privattr, "!password")) == nil){
			werrstr("key has no password (cannot happen?)");
			goto out;
		}
		npw = strlen(pw);

		switch(astype){
		case AuthApop:
			ds = md5((uchar*)chal, nchal, nil, nil);
			md5((uchar*)pw, npw, resp, ds);
			break;
		case AuthCram:
			hmac_md5((uchar*)chal, nchal, (uchar*)pw, npw, resp, nil);
			break;
		}

		/* C->S: APOP user hex-response\n */
		if(ntry == 1)
			c->state = "write user";
		else{
			sprint(c->statebuf, "write user (auth attempt #%d)", ntry);
			c->state = c->statebuf;
		}
		if(convprint(c, "%s", strfindattr(k->attr, "user")) < 0)
			goto out;

		c->state = "write response";
		if(convprint(c, "%.*H", sizeof resp, resp) < 0)
			goto out;

		c->state = "read result";
		if(convreadm(c, &res) < 0)
			goto out;

		if(strcmp(res, "ok") == 0)
			break;

		if(strncmp(res, "bad ", 4) != 0){
			werrstr("bad result: %s", res);
			goto out;
		}

		c->state = "replace key";
		if((k = keyreplace(c, k, "%s", res+4)) == nil){
			c->state = "auth failed";
			werrstr("%s", res+4);
			goto out;
		}
		free(res);
		res = nil;
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

/* shared with auth dialing routines */
typedef struct ServerState ServerState;
struct ServerState
{
	int asfd;
	Key *k;
	Ticketreq tr;
	Ticket t;
	char *dom;
	char *hostid;
};

enum
{
	APOPCHALLEN = 128
};

static int apopchal(ServerState*, int, char[APOPCHALLEN]);
static int apopresp(ServerState*, char*, char*);

static int
apopserver(Conv *c)
{
	char chal[APOPCHALLEN], *user, *resp;
	ServerState s;
	int astype, ret;
	Attr *a;

	ret = -1;
	user = nil;
	resp = nil;
	memset(&s, 0, sizeof s);
	s.asfd = -1;

	if(c->proto == &apop)
		astype = AuthApop;
	else if(c->proto == &cram)
		astype = AuthCram;
	else{
		werrstr("bad proto");
		goto out;
	}

	c->state = "find key";
	if((s.k = plan9authkey(c->attr)) == nil)
		goto out;

	a = copyattr(s.k->attr);
	a = delattr(a, "proto");
	c->attr = addattrs(c->attr, a);
	freeattr(a);

	c->state = "authdial";
	s.hostid = strfindattr(s.k->attr, "user");
	s.dom = strfindattr(s.k->attr, "dom");
	if((s.asfd = xioauthdial(nil, s.dom)) < 0){
		werrstr("authdial %s: %r", s.dom);
		goto out;
	}

	c->state = "authchal";
	if(apopchal(&s, astype, chal) < 0)
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

static int
apopchal(ServerState *s, int astype, char chal[APOPCHALLEN])
{
	char trbuf[TICKREQLEN];
	Ticketreq tr;

	memset(&tr, 0, sizeof tr);

	tr.type = astype;

	if(strlen(s->hostid) >= sizeof tr.hostid){
		werrstr("hostid too long");
		return -1;
	}
	strcpy(tr.hostid, s->hostid);

	if(strlen(s->dom) >= sizeof tr.authdom){
		werrstr("domain too long");
		return -1;
	}
	strcpy(tr.authdom, s->dom);

	convTR2M(&tr, trbuf);
	if(xiowrite(s->asfd, trbuf, TICKREQLEN) != TICKREQLEN)
		return -1;

	if(xioasrdresp(s->asfd, chal, APOPCHALLEN) <= 5)
		return -1;

	s->tr = tr;
	return 0;
}

static int
apopresp(ServerState *s, char *user, char *resp)
{
	char tabuf[TICKETLEN+AUTHENTLEN];
	char trbuf[TICKREQLEN];
	int len;
	Authenticator a;
	Ticket t;
	Ticketreq tr;

	tr = s->tr;
	if(memrandom(tr.chal, CHALLEN) < 0)
		return -1;

	if(strlen(user) >= sizeof tr.uid){
		werrstr("uid too long");
		return -1;
	}
	strcpy(tr.uid, user);

	convTR2M(&tr, trbuf);
	if(xiowrite(s->asfd, trbuf, TICKREQLEN) != TICKREQLEN)
		return -1;

	len = strlen(resp);
	if(xiowrite(s->asfd, resp, len) != len)
		return -1;

	if(xioasrdresp(s->asfd, tabuf, TICKETLEN+AUTHENTLEN) != TICKETLEN+AUTHENTLEN)
		return 0;

	convM2T(tabuf, &t, s->k->priv);
	if(t.num != AuthTs
	|| memcmp(t.chal, tr.chal, sizeof tr.chal) != 0){
		werrstr("key mismatch with auth server");
		return -1;
	}

	convM2A(tabuf+TICKETLEN, &a, t.key);
	if(a.num != AuthAc
	|| memcmp(a.chal, tr.chal, sizeof a.chal) != 0
	|| a.id != 0){
		werrstr("key2 mismatch with auth server");
		return -1;
	}

	s->t = t;
	return 1;
}

static Role
apoproles[] =
{
	"client",	apopclient,
	"server",	apopserver,
	0
};

Proto apop = {
	"apop",
	apoproles,
	"user? !password?",
	apopcheck,
	nil
};

Proto cram = {
	"cram",
	apoproles,
	"user? !password?",
	apopcheck,
	nil
};
