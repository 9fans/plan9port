/*
 * CHAP, MSCHAP
 *
 * The client does not authenticate the server, hence no CAI
 *
 * Protocol:
 *
 *	S -> C: random 8-byte challenge
 *	C -> S: user in UTF-8
 *	C -> S: Chapreply or MSchapreply structure
 *	S -> C: ok or 'bad why'
 *
 * The chap protocol requires the client to give it id=%d, the id of
 * the PPP message containing the challenge, which is used
 * as part of the response.  Because the client protocol is message-id
 * specific, there is no point in looping to try multiple keys.
 *
 * The MS chap protocol actually uses two different hashes, an
 * older insecure one called the LM (Lan Manager) hash, and a newer
 * more secure one called the NT hash.  By default we send back only
 * the NT hash, because the LM hash can help an eavesdropper run
 * a brute force attack.  If the key has an lm attribute, then we send only the
 * LM hash.
 */

#include "std.h"
#include "dat.h"

extern Proto chap, mschap;

enum {
	ChapChallen = 8,

	MShashlen = 16,
	MSchallen = 8,
	MSresplen = 24
};

static int
chapcheck(Key *k)
{
	if(!strfindattr(k->attr, "user") || !strfindattr(k->privattr, "!password")){
		werrstr("need user and !password attributes");
		return -1;
	}
	return 0;
}

static void
nthash(uchar hash[MShashlen], char *passwd)
{
	uchar buf[512];
	int i;

	for(i=0; *passwd && i<sizeof(buf); passwd++) {
		buf[i++] = *passwd;
		buf[i++] = 0;
	}

	memset(hash, 0, 16);

	md4(buf, i, hash, 0);
}

static void
desencrypt(uchar data[8], uchar key[7])
{
	ulong ekey[32];

	key_setup(key, ekey);
	block_cipher(ekey, data, 0);
}

static void
lmhash(uchar hash[MShashlen], char *passwd)
{
	uchar buf[14];
	char *stdtext = "KGS!@#$%";
	int i;

	strncpy((char*)buf, passwd, sizeof(buf));
	for(i=0; i<sizeof(buf); i++)
		if(buf[i] >= 'a' && buf[i] <= 'z')
			buf[i] += 'A' - 'a';

	memset(hash, 0, 16);
	memcpy(hash, stdtext, 8);
	memcpy(hash+8, stdtext, 8);

	desencrypt(hash, buf);
	desencrypt(hash+8, buf+7);
}

static void
mschalresp(uchar resp[MSresplen], uchar hash[MShashlen], uchar chal[MSchallen])
{
	int i;
	uchar buf[21];

	memset(buf, 0, sizeof(buf));
	memcpy(buf, hash, MShashlen);

	for(i=0; i<3; i++) {
		memmove(resp+i*MSchallen, chal, MSchallen);
		desencrypt(resp+i*MSchallen, buf+i*7);
	}
}

static int
chapclient(Conv *c)
{
	int id, astype, nchal, npw, ret;
	uchar *chal;
	char *s, *pw, *user, *res;
	Attr *attr;
	Key *k;
	Chapreply cr;
	MSchapreply mscr;
	DigestState *ds;

	ret = -1;
	chal = nil;
	k = nil;
	attr = c->attr;
	res = nil;

	if(c->proto == &chap){
		astype = AuthChap;
		s = strfindattr(attr, "id");
		if(s == nil || *s == 0){
			werrstr("need id=n attr in start message");
			goto out;
		}
		id = strtol(s, &s, 10);
		if(*s != 0 || id < 0 || id >= 256){
			werrstr("bad id=n attr in start message");
			goto out;
		}
		cr.id = id;
	}else if(c->proto == &mschap)
		astype = AuthMSchap;
	else{
		werrstr("bad proto");
		goto out;
	}

	c->state = "find key";
	k = keyfetch(c, "%A %s", attr, c->proto->keyprompt);
	if(k == nil)
		goto out;

	c->attr = addattrs(copyattr(attr), k->attr);

	c->state = "read challenge";
	if((nchal = convreadm(c, (char**)(void*)&chal)) < 0)
		goto out;
	if(astype == AuthMSchap && nchal != MSchallen)
	c->state = "write user";
	if((user = strfindattr(k->attr, "user")) == nil){
		werrstr("key has no user (cannot happen?)");
		goto out;
	}
	if(convprint(c, "%s", user) < 0)
		goto out;

	c->state = "write response";
	if((pw = strfindattr(k->privattr, "!password")) == nil){
		werrstr("key has no password (cannot happen?)");
		goto out;
	}
	npw = strlen(pw);

	if(astype == AuthChap){
		ds = md5(&cr.id, 1, 0, 0);
		md5((uchar*)pw, npw, 0, ds);
		md5(chal, nchal, (uchar*)cr.resp, ds);
		if(convwrite(c, &cr, sizeof cr) < 0)
			goto out;
	}else{
		uchar hash[MShashlen];

		memset(&mscr, 0, sizeof mscr);
		if(strfindattr(k->attr, "lm")){
			lmhash(hash, pw);
			mschalresp((uchar*)mscr.LMresp, hash, chal);
		}else{
			nthash(hash, pw);
			mschalresp((uchar*)mscr.NTresp, hash, chal);
		}
		if(convwrite(c, &mscr, sizeof mscr) < 0)
			goto out;
	}

	c->state = "read result";
	if(convreadm(c, &res) < 0)
		goto out;
	if(strcmp(res, "ok") == 0){
		ret = 0;
		werrstr("succeeded");
		goto out;
	}
	if(strncmp(res, "bad ", 4) != 0){
		werrstr("bad result: %s", res);
		goto out;
	}

	c->state = "replace key";
	keyevict(c, k, "%s", res+4);
	werrstr("%s", res+4);

out:
	free(res);
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

static int chapchal(ServerState*, int, char[ChapChallen]);
static int chapresp(ServerState*, char*, char*);

static int
chapserver(Conv *c)
{
	char chal[ChapChallen], *user, *resp;
	ServerState s;
	int astype, ret;
	Attr *a;

	ret = -1;
	user = nil;
	resp = nil;
	memset(&s, 0, sizeof s);
	s.asfd = -1;

	if(c->proto == &chap)
		astype = AuthChap;
	else if(c->proto == &mschap)
		astype = AuthMSchap;
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
	if(chapchal(&s, astype, chal) < 0)
		goto out;

	c->state = "write challenge";
	if(convprint(c, "%s", chal) < 0)
		goto out;

	c->state = "read user";
	if(convreadm(c, &user) < 0)
		goto out;

	c->state = "read response";
	if(convreadm(c, &resp) < 0)
		goto out;

	c->state = "authwrite";
	switch(chapresp(&s, user, resp)){
	default:
		fprint(2, "factotum: bad result from chapresp\n");
		goto out;
	case -1:
		goto out;
	case 0:
		c->state = "write status";
		if(convprint(c, "bad authentication failed") < 0)
			goto out;
		goto out;

	case 1:
		c->state = "write status";
		if(convprint(c, "ok") < 0)
			goto out;
		goto ok;
	}

ok:
	ret = 0;
	c->attr = addcap(c->attr, c->sysuser, &s.t);

out:
	keyclose(s.k);
	free(user);
	free(resp);
/*	xioclose(s.asfd); */
	return ret;
}

static int
chapchal(ServerState *s, int astype, char chal[ChapChallen])
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

	if(xioasrdresp(s->asfd, chal, ChapChallen) <= 5)
		return -1;

	s->tr = tr;
	return 0;
}

static int
chapresp(ServerState *s, char *user, char *resp)
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
chaproles[] =
{
	"client",	chapclient,
	"server",	chapserver,
	0
};

Proto chap = {
	"chap",
	chaproles,
	"user? !password?",
	chapcheck
};

Proto mschap = {
	"mschap",
	chaproles,
	"user? !password?",
	chapcheck
};
