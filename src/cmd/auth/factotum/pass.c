/*
 * This is just a repository for a password.
 * We don't want to encourage this, there's
 * no server side.
 */

#include "dat.h"

typedef struct State State;
struct State 
{
	Key *key;
};

enum
{
	HavePass,
	Maxphase,
};

static char *phasenames[Maxphase] =
{
[HavePass]	"HavePass",
};

static int
passinit(Proto *p, Fsstate *fss)
{
	int ask;
	Key *k;
	State *s;

	k = findkey(fss, Kuser, &ask, 0, fss->attr, "%s", p->keyprompt);
	if(k == nil){
		if(ask)
			return RpcNeedkey;
		return failure(fss, nil);
	}
	setattrs(fss->attr, k->attr);
	s = emalloc(sizeof(*s));
	s->key = k;
	fss->ps = s;
	return RpcOk;
}

static void
passclose(Fsstate *fss)
{
	State *s;

	s = fss->ps;
	if(s->key)
		closekey(s->key);
	free(s);
}

static int
passread(Fsstate *fss, void *va, uint *n)
{
	int m;
	char buf[500];
	char *pass, *user;
	State *s;

	s = fss->ps;
	switch(fss->phase){
	default:
		return phaseerror(fss, "read");

	case HavePass:
		user = strfindattr(s->key->attr, "user");
		pass = strfindattr(s->key->privattr, "!password");
		if(user==nil || pass==nil)
			return failure(fss, "passread cannot happen");
		snprint(buf, sizeof buf, "%q %q", user, pass);
		m = strlen(buf);
		if(m > *n)
			return toosmall(fss, m);
		*n = m;
		memmove(va, buf, m);
		return RpcOk;
	}
}

static int
passwrite(Fsstate *fss, void*, uint)
{
	return phaseerror(fss, "write");
}

Proto pass =
{
.name=		"pass",
.init=		passinit,
.write=		passwrite,
.read=		passread,
.close=		passclose,
.addkey=		replacekey,
.keyprompt=	"user? !password?",
};
