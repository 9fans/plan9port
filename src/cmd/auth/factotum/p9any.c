#include "std.h"
#include "dat.h"

/*
 * p9any - protocol negotiator
 *
 * Protocol:
 *	S->C: v.2 proto@dom proto@dom proto@dom... NUL
 *	C->S: proto dom NUL
 *	[negotiated proto continues]
 */

extern Proto p9sk1, p9sk2, p9cr;

static Proto* okproto[] =
{
	&p9sk1,
	nil
};

static int
rolecall(Role *r, char *name, Conv *c)
{
	for(; r->name; r++)
		if(strcmp(r->name, name) == 0)
			return (*r->fn)(c);
	werrstr("unknown role");
	return -1;
}

static int
hasnul(void *v, int n)
{
	char *c;

	c = v;
	if(n > 0 && c[n-1] == '\0')
		return n;
	else
		return AuthRpcMax;
}

static int
p9anyserver(Conv *c)
{
	char *s, *dom;
	int i, j, n, m, ret;
	char *tok[3];
	Attr *attr;
	Key *k;

	ret = -1;
	s = estrdup("v.2");
	n = 0;
	attr = delattr(copyattr(c->attr), "proto");

	for(i=0; i<ring.nkey; i++){
		k = ring.key[i];
		for(j=0; okproto[j]; j++)
			if(k->proto == okproto[j]
			&& (dom = strfindattr(k->attr, "dom")) != nil
			&& matchattr(attr, k->attr, k->privattr)){
				s = estrappend(s, " %s@%s", k->proto->name, dom);
				n++;
			}
	}

	if(n == 0){
		werrstr("no valid keys");
		goto out;
	}

	c->state = "write offer";
	if(convwrite(c, s, strlen(s)+1) < 0)
		goto out;
	free(s);
	s = nil;

	c->state = "read choice";
	if(convreadfn(c, hasnul, &s) < 0)
		goto out;

	m = tokenize(s, tok, nelem(tok));
	if(m != 2){
		werrstr("bad protocol message");
		goto out;
	}

	for(i=0; okproto[i]; i++)
		if(strcmp(okproto[i]->name, tok[0]) == 0)
			break;
	if(!okproto[i]){
		werrstr("bad chosen protocol %q", tok[0]);
		goto out;
	}

	c->state = "write ok";
	if(convwrite(c, "OK\0", 3) < 0)
		goto out;

	c->state = "start choice";
	attr = addattr(attr, "proto=%q dom=%q", tok[0], tok[1]);
	free(c->attr);
	c->attr = attr;
	attr = nil;
	c->proto = okproto[i];

	if(rolecall(c->proto->roles, "server", c) < 0){
		werrstr("%s: %r", tok[0]);
		goto out;
	}

	ret = 0;

out:
	free(s);
	freeattr(attr);
	return ret;
}

static int
p9anyclient(Conv *c)
{
	char *s, **f, *tok[20], ok[3], *q, *user, *dom, *choice;
	int i, n, ret, version;
	Key *k;
	Attr *attr;
	Proto *p;

	ret = -1;
	s = nil;
	k = nil;

	user = strfindattr(c->attr, "user");
	dom = strfindattr(c->attr, "dom");

	/*
	 * if the user is the factotum owner, any key will do.
	 * if not, then if we have a speakfor key,
	 * we will only vouch for the user's local identity.
	 *
	 * this logic is duplicated in p9sk1.c
	 */
	attr = delattr(copyattr(c->attr), "role");
	attr = delattr(attr, "proto");
	if(strcmp(c->sysuser, owner) == 0)
		attr = addattr(attr, "role=client");
	else if(user==nil || strcmp(c->sysuser, user)==0){
		attr = delattr(attr, "user");
		attr = addattr(attr, "role=speakfor");
	}else{
		werrstr("will not authenticate for %q as %q", c->sysuser, user);
		goto out;
	}

	c->state = "read offer";
	if(convreadfn(c, hasnul, &s) < 0)
		goto out;

	c->state = "look for keys";
	n = tokenize(s, tok, nelem(tok));
	f = tok;
	version = 1;
	if(n > 0 && memcmp(f[0], "v.", 2) == 0){
		version = atoi(f[0]+2);
		if(version != 2){
			werrstr("unknown p9any version: %s", f[0]);
			goto out;
		}
		f++;
		n--;
	}

	/* look for keys that don't need confirmation */
	for(i=0; i<n; i++){
		if((q = strchr(f[i], '@')) == nil)
			continue;
		if(dom && strcmp(q+1, dom) != 0)
			continue;
		*q++ = '\0';
		if((k = keylookup("%A proto=%q dom=%q", attr, f[i], q))
		&& strfindattr(k->attr, "confirm") == nil)
			goto found;
		*--q = '@';
	}

	/* look for any keys at all */
	for(i=0; i<n; i++){
		if((q = strchr(f[i], '@')) == nil)
			continue;
		if(dom && strcmp(q+1, dom) != 0)
			continue;
		*q++ = '\0';
		if(k = keylookup("%A proto=%q dom=%q", attr, f[i], q))
			goto found;
		*--q = '@';
	}

	/* ask for new keys */
	c->state = "ask for keys";
	for(i=0; i<n; i++){
		if((q = strchr(f[i], '@')) == nil)
			continue;
		if(dom && strcmp(q+1, dom) != 0)
			continue;
		*q++ = '\0';
		p = protolookup(f[i]);
		if(p == nil || p->keyprompt == nil){
			*--q = '@';
			continue;
		}
		if(k = keyfetch(c, "%A proto=%q dom=%q %s", attr, f[i], q, p->keyprompt))
			goto found;
		*--q = '@';
	}

	/* nothing worked */
	werrstr("unable to find common key");
	goto out;

found:
	/* f[i] is the chosen protocol, q the chosen domain */
	attr = addattr(attr, "proto=%q dom=%q", f[i], q);
	c->state = "write choice";

	/* have a key: go for it */
	choice = estrappend(nil, "%q %q", f[i], q);
	if(convwrite(c, choice, strlen(choice)+1) < 0){
		free(choice);
		goto out;
	}
	free(choice);

	if(version == 2){
		c->state = "read ok";
		if(convread(c, ok, 3) < 0 || memcmp(ok, "OK\0", 3) != 0)
			goto out;
	}

	c->state = "start choice";
	c->proto = protolookup(f[i]);
	freeattr(c->attr);
	c->attr = attr;
	attr = nil;

	if(rolecall(c->proto->roles, "client", c) < 0){
		werrstr("%s: %r", c->proto->name);
		goto out;
	}

	ret = 0;

out:
	keyclose(k);
	freeattr(attr);
	free(s);
	return ret;
}

static Role
p9anyroles[] =
{
	"client",	p9anyclient,
	"server",	p9anyserver,
	0
};

Proto p9any = {
	"p9any",
	p9anyroles
};
