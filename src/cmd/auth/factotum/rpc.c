#include "std.h"
#include "dat.h"

/*
 * Factotum RPC
 *
 * Must be paired write/read cycles on /mnt/factotum/rpc.
 * The format of a request is verb, single space, data.
 * Data format is verb-dependent; in particular, it can be binary.
 * The format of a response is the same.  The write only sets up
 * the RPC.  The read tries to execute it.  If the /mnt/factotum/key
 * file is open, we ask for new keys using that instead of returning
 * an error in the RPC.  This means the read blocks.
 * Textual arguments are parsed with tokenize, so rc-style quoting
 * rules apply.
 *
 * Only authentication protocol messages go here.  Configuration
 * is still via ctl (below).
 *
 * Request RPCs are:
 *	start attrs - initializes protocol for authentication, can fail.
 *		returns "ok read" or "ok write" on success.
 *	read - execute protocol read
 *	write - execute protocol write
 *	authinfo - if the protocol is finished, return the AI if any
 *	attr - return protocol information
 * Return values are:
 *	error message - an error happened.
 *	ok [data] - success, possible data is request dependent.
 *	needkey attrs - request aborted, get me this key and try again
 *	badkey attrs - request aborted, this key might be bad
 *	done [haveai] - authentication is done [haveai: you can get an ai with authinfo]
 */

char *rpcname[] =
{
	"unknown",
	"authinfo",
	"attr",
	"read",
	"start",
	"write",
	"readhex",
	"writehex"
};

static int
classify(char *s)
{
	int i;

	for(i=1; i<nelem(rpcname); i++)
		if(strcmp(s, rpcname[i]) == 0)
			return i;
	return RpcUnknown;
}

int
rpcwrite(Conv *c, void *data, int count)
{
	int op;
	uchar *p;

	if(count >= MaxRpc){
		werrstr("rpc too large");
		return -1;
	}

	/* cancel any current rpc */
	c->rpc.op = RpcUnknown;
	c->nreply = 0;

	/* parse new rpc */
	memmove(c->rpcbuf, data, count);
	c->rpcbuf[count] = 0;
	if(p = (uchar*)strchr((char*)c->rpcbuf, ' ')){
		*p++ = '\0';
		c->rpc.data = p;
		c->rpc.count = count - (p - (uchar*)c->rpcbuf);
	}else{
		c->rpc.data = "";
		c->rpc.count = 0;
	}
	op = classify(c->rpcbuf);
	if(op == RpcUnknown){
		werrstr("bad rpc verb: %s", c->rpcbuf);
		return -1;
	}

	c->rpc.op = op;
	return 0;
}

void
convthread(void *v)
{
	Conv *c;
	Attr *a;
	char *role, *proto;
	Proto *p;
	Role *r;

	c = v;
	a = parseattr(c->rpc.data);
	if(a == nil){
		werrstr("empty attr");
		goto out;
	}
	c->attr = a;
	proto = strfindattr(a, "proto");
	if(proto == nil){
		werrstr("no proto in attrs");
		goto out;
	}

	p = protolookup(proto);
	if(p == nil){
		werrstr("unknown proto %s", proto);
		goto out;
	}
	c->proto = p;

	role = strfindattr(a, "role");
	if(role == nil){
		werrstr("no role in attrs");
		goto out;
	}

	for(r=p->roles; r->name; r++){
		if(strcmp(r->name, role) != 0)
			continue;
		rpcrespond(c, "ok");
		c->active = 1;
		if((*r->fn)(c) == 0){
			c->done = 1;
			werrstr("protocol finished");
		}else
			werrstr("%s %s %s: %r", p->name, r->name, c->state);
		goto out;
	}
	werrstr("unknown role");

out:
	c->active = 0;
	c->state = 0;
	rerrstr(c->err, sizeof c->err);
	rpcrespond(c, "error %r");
	convclose(c);
}

static uchar* convAI2M(uchar *p, int n, char *cuid, char *suid, char *cap, char *hex);

void
rpcexec(Conv *c)
{
	uchar *p;

	c->rpc.hex = 0;
	switch(c->rpc.op){
	case RpcWriteHex:
		c->rpc.op = RpcWrite;
		if(dec16(c->rpc.data, c->rpc.count, c->rpc.data, c->rpc.count) != c->rpc.count/2){
			rpcrespond(c, "bad hex");
			break;
		}
		c->rpc.count /= 2;
		goto Default;
	case RpcReadHex:
		c->rpc.hex = 1;
		c->rpc.op = RpcRead;
		/* fall through */
	case RpcRead:
		if(c->rpc.count > 0){
			rpcrespond(c, "error read takes no parameters");
			break;
		}
		/* fall through */
	default:
	Default:
		if(!c->active){
			if(c->done)
				rpcrespond(c, "done");
			else
				rpcrespond(c, "error %s", c->err);
			break;
		}
		nbsendp(c->rpcwait, 0);
		break;
	case RpcUnknown:
		break;
	case RpcAuthinfo:
		/* deprecated */
		if(c->active)
			rpcrespond(c, "error conversation still active");
		else if(!c->done)
			rpcrespond(c, "error conversation not successful");
		else{
			/* make up an auth info using the attr */
			p = convAI2M((uchar*)c->reply+3, sizeof c->reply-3,
				strfindattr(c->attr, "cuid"),
				strfindattr(c->attr, "suid"),
				strfindattr(c->attr, "cap"),
				strfindattr(c->attr, "secret"));
			if(p == nil)
				rpcrespond(c, "error %r");
			else
				rpcrespondn(c, "ok", c->reply+3, p-(uchar*)(c->reply+3));
		}
		break;
	case RpcAttr:
		rpcrespond(c, "ok %A", c->attr);
		break;
	case RpcStart:
		convreset(c);
		c->ref++;
		threadcreate(convthread, c, STACK);
		break;
	}
}

void
rpcrespond(Conv *c, char *fmt, ...)
{
	va_list arg;

	if(c->hangup)
		return;

	if(fmt == nil)
		fmt = "";

	va_start(arg, fmt);
	c->nreply = vsnprint(c->reply, sizeof c->reply, fmt, arg);
	va_end(arg);
	(*c->kickreply)(c);
	c->rpc.op = RpcUnknown;
}

void
rpcrespondn(Conv *c, char *verb, void *data, int count)
{
	char *p;
	int need, hex;

	if(c->hangup)
		return;

	need = strlen(verb)+1+count;
	hex = 0;
	if(c->rpc.hex && strcmp(verb, "ok") == 0){
		need += count;
		hex = 1;
	}
	if(need > sizeof c->reply){
		print("RPC response too large; caller %#lux", getcallerpc(&c));
		return;
	}

	strcpy(c->reply, verb);
	p = c->reply + strlen(c->reply);
	*p++ = ' ';
	if(hex){
		enc16(p, 2*count+1, data, count);
		p += 2*count;
	}else{
		memmove(p, data, count);
		p += count;
	}
	c->nreply = p - c->reply;
	(*c->kickreply)(c);
	c->rpc.op = RpcUnknown;
}

/* deprecated */
static uchar*
pstring(uchar *p, uchar *e, char *s)
{
	uint n;

	if(p == nil)
		return nil;
	if(s == nil)
		s = "";
	n = strlen(s);
	if(p+n+BIT16SZ >= e)
		return nil;
	PBIT16(p, n);
	p += BIT16SZ;
	memmove(p, s, n);
	p += n;
	return p;
}

static uchar*
pcarray(uchar *p, uchar *e, uchar *s, uint n)
{
	if(p == nil)
		return nil;
	if(s == nil){
		if(n > 0)
			sysfatal("pcarray");
		s = (uchar*)"";
	}
	if(p+n+BIT16SZ >= e)
		return nil;
	PBIT16(p, n);
	p += BIT16SZ;
	memmove(p, s, n);
	p += n;
	return p;
}

static uchar*
convAI2M(uchar *p, int n, char *cuid, char *suid, char *cap, char *hex)
{
	uchar *e = p+n;
	uchar *secret;
	int nsecret;

	if(cuid == nil)
		cuid = "";
	if(suid == nil)
		suid = "";
	if(cap == nil)
		cap = "";
	if(hex == nil)
		hex = "";
	nsecret = strlen(hex)/2;
	secret = emalloc(nsecret);
	if(hexparse(hex, secret, nsecret) < 0){
		werrstr("hexparse %s failed", hex);	/* can't happen */
		free(secret);
		return nil;
	}
	p = pstring(p, e, cuid);
	p = pstring(p, e, suid);
	p = pstring(p, e, cap);
	p = pcarray(p, e, secret, nsecret);
	free(secret);
	if(p == nil)
		werrstr("authinfo too big");
	return p;
}
