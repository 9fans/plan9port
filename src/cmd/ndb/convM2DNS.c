#include <u.h>
#include <libc.h>
#include <ip.h>
#include <bio.h>
#include <ndb.h>
#include "dns.h"

typedef struct Scan	Scan;
struct Scan
{
	uchar	*base;
	uchar	*p;
	uchar	*ep;
	char	*err;
};

#define NAME(x)		gname(x, sp)
#define SYMBOL(x)	(x = gsym(sp))
#define STRING(x)	(x = gstr(sp))
#define USHORT(x)	(x = gshort(sp))
#define ULONG(x)	(x = glong(sp))
#define UCHAR(x)	(x = gchar(sp))
#define V4ADDR(x)	(x = gv4addr(sp))
#define V6ADDR(x)	(x = gv6addr(sp))
#define BYTES(x, y)	(y = gbytes(sp, &x, len - (sp->p - data)))

static char *toolong = "too long";

/*
 *  get a ushort/ulong
 */
static ushort
gchar(Scan *sp)
{
	ushort x;

	if(sp->err)
		return 0;
	if(sp->ep - sp->p < 1){
		sp->err = toolong;
		return 0;
	}
	x = sp->p[0];
	sp->p += 1;
	return x;
}
static ushort
gshort(Scan *sp)
{
	ushort x;

	if(sp->err)
		return 0;
	if(sp->ep - sp->p < 2){
		sp->err = toolong;
		return 0;
	}
	x = (sp->p[0]<<8) | sp->p[1];
	sp->p += 2;
	return x;
}
static ulong
glong(Scan *sp)
{
	ulong x;

	if(sp->err)
		return 0;
	if(sp->ep - sp->p < 4){
		sp->err = toolong;
		return 0;
	}
	x = (sp->p[0]<<24) | (sp->p[1]<<16) | (sp->p[2]<<8) | sp->p[3];
	sp->p += 4;
	return x;
}

/*
 *  get an ip address
 */
static DN*
gv4addr(Scan *sp)
{
	char addr[32];

	if(sp->err)
		return 0;
	if(sp->ep - sp->p < 4){
		sp->err = toolong;
		return 0;
	}
	snprint(addr, sizeof(addr), "%V", sp->p);
	sp->p += 4;

	return dnlookup(addr, Cin, 1);
}
static DN*
gv6addr(Scan *sp)
{
	char addr[64];

	if(sp->err)
		return 0;
	if(sp->ep - sp->p < IPaddrlen){
		sp->err = toolong;
		return 0;
	}
	snprint(addr, sizeof(addr), "%I", sp->p);
	sp->p += IPaddrlen;

	return dnlookup(addr, Cin, 1);
}

/*
 *  get a string.  make it an internal symbol.
 */
static DN*
gsym(Scan *sp)
{
	int n;
	char sym[Strlen+1];

	if(sp->err)
		return 0;
	n = *(sp->p++);
	if(sp->p+n > sp->ep){
		sp->err = toolong;
		return 0;
	}

	if(n > Strlen){
		sp->err = "illegal string";
		return 0;
	}
	strncpy(sym, (char*)sp->p, n);
	sym[n] = 0;
	sp->p += n;

	return dnlookup(sym, Csym, 1);
}

/*
 *  get a string.  don't make it an internal symbol.
 */
static Txt*
gstr(Scan *sp)
{
	int n;
	char sym[Strlen+1];
	Txt *t;

	if(sp->err)
		return 0;
	n = *(sp->p++);
	if(sp->p+n > sp->ep){
		sp->err = toolong;
		return 0;
	}

	if(n > Strlen){
		sp->err = "illegal string";
		return 0;
	}
	strncpy(sym, (char*)sp->p, n);
	sym[n] = 0;
	sp->p += n;

	t = emalloc(sizeof(*t));
	t->next = nil;
	t->p = estrdup(sym);
	return t;
}

/*
 *  get a sequence of bytes
 */
static int
gbytes(Scan *sp, uchar **p, int n)
{
	if(sp->err)
		return 0;
	if(sp->p+n > sp->ep || n < 0){
		sp->err = toolong;
		return 0;
	}
	*p = emalloc(n);
	memmove(*p, sp->p, n);
	sp->p += n;

	return n;
}

/*
 *  get a domain name.  'to' must point to a buffer at least Domlen+1 long.
 */
static char*
gname(char *to, Scan *sp)
{
	int len, off;
	int pointer;
	int n;
	char *tostart;
	char *toend;
	uchar *p;

	tostart = to;
	if(sp->err)
		goto err;
	pointer = 0;
	p = sp->p;
	toend = to + Domlen;
	for(len = 0; *p; len += pointer ? 0 : (n+1)){
		if((*p & 0xc0) == 0xc0){
			/* pointer to other spot in message */
			if(pointer++ > 10){
				sp->err = "pointer loop";
				goto err;
			}
			off = ((p[0]<<8) + p[1]) & 0x3ff;
			p = sp->base + off;
			if(p >= sp->ep){
				sp->err = "bad pointer";
				goto err;
			}
			n = 0;
			continue;
		}
		n = *p++;
		if(len + n < Domlen - 1){
			if(to + n > toend){
				sp->err = toolong;
				goto err;
			}
			memmove(to, p, n);
			to += n;
		}
		p += n;
		if(*p){
			if(to >= toend){
				sp->err = toolong;
				goto err;
			}
			*to++ = '.';
		}
	}
	*to = 0;
	if(pointer)
		sp->p += len + 2;	/* + 2 for pointer */
	else
		sp->p += len + 1;	/* + 1 for the null domain */
	return tostart;
err:
	*tostart = 0;
	return tostart;
}

/*
 *  convert the next RR from a message
 */
static RR*
convM2RR(Scan *sp)
{
	RR *rp;
	int type;
	int class;
	uchar *data;
	int len;
	char dname[Domlen+1];
	Txt *t, **l;

retry:
	NAME(dname);
	USHORT(type);
	USHORT(class);

	rp = rralloc(type);
	rp->owner = dnlookup(dname, class, 1);
	rp->type = type;

	ULONG(rp->ttl);
	rp->ttl += now;
	USHORT(len);
	data = sp->p;

	if(sp->p + len > sp->ep)
		sp->err = toolong;
	if(sp->err){
		rrfree(rp);
		return 0;
	}

	switch(type){
	default:
		/* unknown type, just ignore it */
		sp->p = data + len;
		rrfree(rp);
		goto retry;
	case Thinfo:
		SYMBOL(rp->cpu);
		SYMBOL(rp->os);
		break;
	case Tcname:
	case Tmb:
	case Tmd:
	case Tmf:
	case Tns:
		rp->host = dnlookup(NAME(dname), Cin, 1);
		break;
	case Tmg:
	case Tmr:
		rp->mb = dnlookup(NAME(dname), Cin, 1);
		break;
	case Tminfo:
		rp->rmb = dnlookup(NAME(dname), Cin, 1);
		rp->mb = dnlookup(NAME(dname), Cin, 1);
		break;
	case Tmx:
		USHORT(rp->pref);
		rp->host = dnlookup(NAME(dname), Cin, 1);
		break;
	case Ta:
		V4ADDR(rp->ip);
		break;
	case Taaaa:
		V6ADDR(rp->ip);
		break;
	case Tptr:
		rp->ptr = dnlookup(NAME(dname), Cin, 1);
		break;
	case Tsoa:
		rp->host = dnlookup(NAME(dname), Cin, 1);
		rp->rmb = dnlookup(NAME(dname), Cin, 1);
		ULONG(rp->soa->serial);
		ULONG(rp->soa->refresh);
		ULONG(rp->soa->retry);
		ULONG(rp->soa->expire);
		ULONG(rp->soa->minttl);
		break;
	case Ttxt:
		l = &rp->txt;
		*l = nil;
		while(sp->p-data < len){
			STRING(t);
			*l = t;
			l = &t->next;
		}
		break;
	case Tnull:
		BYTES(rp->null->data, rp->null->dlen);
		break;
	case Trp:
		rp->rmb = dnlookup(NAME(dname), Cin, 1);
		rp->rp = dnlookup(NAME(dname), Cin, 1);
		break;
	case Tkey:
		USHORT(rp->key->flags);
		UCHAR(rp->key->proto);
		UCHAR(rp->key->alg);
		BYTES(rp->key->data, rp->key->dlen);
		break;
	case Tsig:
		USHORT(rp->sig->type);
		UCHAR(rp->sig->alg);
		UCHAR(rp->sig->labels);
		ULONG(rp->sig->ttl);
		ULONG(rp->sig->exp);
		ULONG(rp->sig->incep);
		USHORT(rp->sig->tag);
		rp->sig->signer = dnlookup(NAME(dname), Cin, 1);
		BYTES(rp->sig->data, rp->sig->dlen);
		break;
	case Tcert:
		USHORT(rp->cert->type);
		USHORT(rp->cert->tag);
		UCHAR(rp->cert->alg);
		BYTES(rp->cert->data, rp->cert->dlen);
		break;
	}
	if(sp->p - data != len)
		sp->err = "bad RR len";
	return rp;
}

/*
 *  convert the next question from a message
 */
static RR*
convM2Q(Scan *sp)
{
	char dname[Domlen+1];
	int type;
	int class;
	RR *rp;

	NAME(dname);
	USHORT(type);
	USHORT(class);
	if(sp->err)
		return 0;

	rp = rralloc(type);
	rp->owner = dnlookup(dname, class, 1);

	return rp;
}

static RR*
rrloop(Scan *sp, int count, int quest)
{
	int i;
	RR *first, *rp, **l;

	if(sp->err)
		return 0;
	l = &first;
	first = 0;
	for(i = 0; i < count; i++){
		rp = quest ? convM2Q(sp) : convM2RR(sp);
		if(rp == 0)
			break;
		if(sp->err){
			rrfree(rp);
			break;
		}
		*l = rp;
		l = &rp->next;
	}
	return first;
}

/*
 *  convert the next DNS from a message stream
 */
char*
convM2DNS(uchar *buf, int len, DNSmsg *m)
{
	Scan scan;
	Scan *sp;
	char *err;

	scan.base = buf;
	scan.p = buf;
	scan.ep = buf + len;
	scan.err = 0;
	sp = &scan;
	memset(m, 0, sizeof(DNSmsg));
	USHORT(m->id);
	USHORT(m->flags);
	USHORT(m->qdcount);
	USHORT(m->ancount);
	USHORT(m->nscount);
	USHORT(m->arcount);
	m->qd = rrloop(sp, m->qdcount, 1);
	m->an = rrloop(sp, m->ancount, 0);
	m->ns = rrloop(sp, m->nscount, 0);
	err = scan.err;				/* live with bad ar's */
	m->ar = rrloop(sp, m->arcount, 0);
	return err;
}
