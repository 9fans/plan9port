/*
 * p9cr - one-sided challenge/response authentication
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
	MAXCHAL = 64,
	MAXRESP = 64,
};

extern Proto p9cr, vnc;
static int p9response(char*, uchar*, uchar*);
// static int vncresponse(char*, uchar*, uchar*);
static int p9crchal(ServerState *s, int, char*, uchar*, int);
static int p9crresp(ServerState*, uchar*, int);

static int
p9crcheck(Key *k)
{
	if(!strfindattr(k->attr, "user") || !strfindattr(k->privattr, "!password")){
		werrstr("need user and !password attributes");
		return -1;
	}
	return 0;
}

static int
p9crclient(Conv *c)
{
	char *pw, *res, *user;
	int astype, challen, resplen, ntry, ret;
	Attr *attr;
	Key *k;
	uchar chal[MAXCHAL+1], resp[MAXRESP];
	int (*response)(char*, uchar*, uchar*);

	k = nil;
	res = nil;
	ret = -1;
	attr = c->attr;
	astype = -1;

	if(c->proto == &p9cr){
		astype = AuthChal;
		challen = NETCHLEN;
		response = p9response;
		attr = _mkattr(AttrNameval, "proto", "p9sk1", _delattr(_copyattr(attr), "proto"));
	}else if(c->proto == &vnc){
		astype = AuthVNC;
		challen = MAXCHAL;
	//	response = vncresponse;
		werrstr("no vnc");
		goto out;
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
		if((user = strfindattr(k->attr, "user")) == nil){
			werrstr("key has no user (cannot happen)");
			goto out;
		}

		if(convprint(c, "%s", user) < 0)
			goto out;

		if(convread(c, chal, challen) < 0)
			goto out;
		chal[challen] = 0;

		if((resplen = (*response)(pw, chal, resp)) < 0)
			goto out;

		if(convwrite(c, resp, resplen) < 0)
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
	USED(astype);
	keyclose(k);
	if(c->attr != attr)
		freeattr(attr);
	return ret;
}

static int
p9crserver(Conv *c)
{
	uchar chal[MAXCHAL], *resp, *resp1;
	char *user;
	ServerState s;
	int astype, ret, challen, resplen;
	Attr *a;

	ret = -1;
	user = nil;
	resp = nil;
	memset(&s, 0, sizeof s);
	s.asfd = -1;

	if(c->proto == &p9cr){
		astype = AuthChal;
		challen = NETCHLEN;
	}else if(c->proto == &vnc){
		challen = MAXCHAL;
	}else{
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

	for(;;){
		c->state = "read user";
		if(convreadm(c, &user) < 0)
			goto out;

		c->state = "authchal";
		if(p9crchal(&s, astype, user, chal, challen) < 0)
			goto out;

		c->state = "write challenge";
		if(convwrite(c, chal, challen) < 0)
			goto out;

		c->state = "read response";
		if((resplen = convreadm(c, (char**)(void*)&resp)) < 0)
			goto out;
		if(c->proto == &p9cr){
			if(resplen > NETCHLEN){
				convprint(c, "bad response too long");
				goto out;
			}
			resp1 = emalloc(NETCHLEN);
			memset(resp1, 0, NETCHLEN);
			memmove(resp1, resp, resplen);
			free(resp);
			resp = resp1;
			resplen = NETCHLEN;
		}

		c->state = "authwrite";
		switch(p9crresp(&s, resp, resplen)){
		case -1:
			fprint(2, "p9crresp: %r\n");
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
p9crchal(ServerState *s, int astype, char *user, uchar *chal, int challen)
{
	char trbuf[TICKREQLEN];
	Ticketreq tr;
	int n;

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

	if(strlen(user) >= sizeof tr.uid){
		werrstr("user name too long");
		return -1;
	}
	strcpy(tr.uid, user);
	convTR2M(&tr, trbuf);

	if(xiowrite(s->asfd, trbuf, TICKREQLEN) != TICKREQLEN)
		return -1;

	if((n=xioasrdresp(s->asfd, chal, challen)) <= 0)
		return -1;
	return n;
}

static int
p9crresp(ServerState *s, uchar *resp, int resplen)
{
	char tabuf[TICKETLEN+AUTHENTLEN];
	Authenticator a;
	Ticket t;
	Ticketreq tr;

	memset(&tr, 0, sizeof tr);	// TODO: what should tr be initialized to?

	if(xiowrite(s->asfd, resp, resplen) != resplen)
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

static int
p9response(char *pw, uchar *chal, uchar *resp)
{
	char key[DESKEYLEN];
	uchar buf[8];
	ulong x;

	passtokey(key, pw);
	memset(buf, 0, 8);
	snprint((char*)buf, sizeof buf, "%d", atoi((char*)chal));
	if(encrypt(key, buf, 8) < 0){
		werrstr("can't encrypt response");
		return -1;
	}
	x = (buf[0]<<24)+(buf[1]<<16)+(buf[2]<<8)+buf[3];
	return snprint((char*)resp, MAXRESP, "%.8lux", x);
}

/*
static int
vncresponse(char *pw, uchar *chal, uchar *resp)
{
	DESstate des;

	memmove(resp, chal, MAXCHAL);
	setupDESstate(&des, 0, nil);  // XXX put key in for 0
	desECBencrypt(resp, MAXCHAL, &des);
	return MAXCHAL;
}
*/

static Role
p9crroles[] =
{
	"client", p9crclient,
	"server", p9crserver,
	0
};

Proto p9cr = {
	"p9cr",
	p9crroles,
	"user? !password?",
	p9crcheck,
	nil
};

/* still need to implement vnc key generator */
Proto vnc = {
	"vnc",
	p9crroles,
	"user? !password?",
	p9crcheck,
	nil
};
