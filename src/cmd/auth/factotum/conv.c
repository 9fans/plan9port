#include "std.h"
#include "dat.h"

Conv *conv;

ulong taggen = 1;

Conv*
convalloc(char *sysuser)
{
	Conv *c;

	c = mallocz(sizeof(Conv), 1);
	if(c == nil)
		return nil;
	c->ref = 1;
	c->tag = taggen++;
	c->next = conv;
	c->sysuser = estrdup(sysuser);
	c->state = "nascent";
	c->rpcwait = chancreate(sizeof(void*), 0);
	c->keywait = chancreate(sizeof(void*), 0);
	strcpy(c->err, "protocol has not started");
	conv = c;
	convreset(c);
	return c;
}

void
convreset(Conv *c)
{
	if(c->ref != 1){
		c->hangup = 1;
		nbsendp(c->rpcwait, 0);
		while(c->ref > 1)
			yield();
		c->hangup = 0;
	}
	c->state = "nascent";
	c->err[0] = '\0';
	freeattr(c->attr);
	c->attr = nil;
	c->proto = nil;
	c->rpc.op = 0;
	c->active = 0;
	c->done = 0;
	c->hangup = 0;
}

void
convhangup(Conv *c)
{
	c->hangup = 1;
	c->rpc.op = 0;
	(*c->kickreply)(c);
	nbsendp(c->rpcwait, 0);
}

void
convclose(Conv *c)
{
	Conv *p;

	if(c == nil)
		return;

	if(--c->ref > 0)
		return;

	if(c == conv){
		conv = c->next;
		goto free;
	}
	for(p=conv; p && p->next!=c; p=p->next)
		;
	if(p == nil){
		print("cannot find conv in list\n");
		return;
	}
	p->next = c->next;

free:
	c->next = nil;
	free(c);
}

static Rpc*
convgetrpc(Conv *c, int want)
{
	for(;;){
		if(c->hangup){
			flog("convgetrpc: hangup");
			werrstr("hangup");
			return nil;
		}
		if(c->rpc.op == RpcUnknown){
			recvp(c->rpcwait);
			if(c->hangup){
				flog("convgetrpc: hangup");
				werrstr("hangup");
				return nil;
			}
			if(c->rpc.op == RpcUnknown)
				continue;
		}
		if(want < 0 || c->rpc.op == want)
			return &c->rpc;
		rpcrespond(c, "phase in state '%s' want '%s'", c->state, rpcname[want]);
	}
	/* not reached */
}

/* read until the done function tells us that's enough */
int
convreadfn(Conv *c, int (*done)(void*, int), char **ps)
{
	int n;
	Rpc *r;
	char *s;

	for(;;){
		r = convgetrpc(c, RpcWrite);
		if(r == nil)
			return -1;
		n = (*done)(r->data, r->count);
		if(n == r->count)
			break;
		rpcrespond(c, "toosmall %d", n);
	}

	s = emalloc(r->count+1);
	memmove(s, r->data, r->count);
	s[r->count] = 0;
	*ps = s;
	rpcrespond(c, "ok");
	return r->count;
}

/*
 * read until we get a non-zero write.  assumes remote side
 * knows something about the protocol (is not auth_proxy).
 * the remote side typically won't bother with the zero-length
 * write to find out the length -- the loop is there only so the
 * test program can call auth_proxy on both sides of a pipe
 * to play a conversation.
 */
int
convreadm(Conv *c, char **ps)
{
	char *s;
	Rpc *r;

	for(;;){
		r = convgetrpc(c, RpcWrite);
		if(r == nil)
			return -1;
		if(r->count > 0)
			break;
		rpcrespond(c, "toosmall %d", AuthRpcMax);
	}
	s = emalloc(r->count+1);
	memmove(s, r->data, r->count);
	s[r->count] = 0;
	*ps = s;
	rpcrespond(c, "ok");
	return r->count;
}

/* read exactly count bytes */
int
convread(Conv *c, void *data, int count)
{
	Rpc *r;

	for(;;){
		r = convgetrpc(c, RpcWrite);
		if(r == nil)
			return -1;
		if(r->count == count)
			break;
		if(r->count < count)
			rpcrespond(c, "toosmall %d", count);
		else
			rpcrespond(c, "error too much data; want %d got %d", count, r->count);
	}
	memmove(data, r->data, count);
	rpcrespond(c, "ok");
	return 0;
}

/* write exactly count bytes */
int
convwrite(Conv *c, void *data, int count)
{
	Rpc *r;

	r = convgetrpc(c, RpcRead);
	if(r == nil)
		return -1;
	rpcrespondn(c, "ok", data, count);
	return 0;
}

/* print to the conversation */
int
convprint(Conv *c, char *fmt, ...)
{
	char *s;
	va_list arg;
	int ret;

	va_start(arg, fmt);
	s = vsmprint(fmt, arg);
	va_end(arg);
	if(s == nil)
		return -1;
	ret = convwrite(c, s, strlen(s));
	free(s);
	return ret;
}

/* ask for a key */
int
convneedkey(Conv *c, Attr *a)
{
	/*
	 * Piggyback key requests in the usual RPC channel.
	 * Wait for the next RPC and then send a key request
	 * in response.  The keys get added out-of-band (via the
	 * ctl file), so assume the key has been added when the
	 * next request comes in.
	 *
	 * The convgetrpc seems dodgy, because we might be in
	 * the middle of an rpc, and what about the one that comes
	 * in later?  It's all actually okay: convgetrpc is idempotent
	 * until rpcrespond is called, so if we're in the middle of an rpc,
	 * the first convgetrpc is a no-op, the rpcrespond sends back
	 * the needkey, and then the client repeats the rpc we're in
	 * the middle of.  Otherwise, if we're not in the middle of an
	 * rpc, the first convgetrpc waits for one, we respond needkey,
	 * and then the second convgetrpc waits for another.  Because
	 * there is no second response, eventually the caller will get
	 * around to asking for an rpc itself, at which point the already
	 * gotten rpc will be returned again.
	 */
	if(convgetrpc(c, -1) == nil)
		return -1;
	if(conv->proto)
		a = addattrs(parseattr(c->proto->keyprompt), a);
	flog("convneedkey %A", a);
	rpcrespond(c, "needkey %A", a);
	if(convgetrpc(c, -1) == nil)
		return -1;
	flog("convneedkey returning");
	return 0;
}

/* ask for a replacement for a bad key*/
int
convbadkey(Conv *c, Key *k, char *msg, Attr *a)
{
	if(convgetrpc(c, -1) == nil)
		return -1;
	flog("convbadkey %A %N / %s / %A", k->attr, k->privattr, msg, a);
	rpcrespond(c, "badkey %A %N\n%s\n%A",
		k->attr, k->privattr, msg, a);
	if(convgetrpc(c, -1) == nil)
		return -1;
	return 0;
}
