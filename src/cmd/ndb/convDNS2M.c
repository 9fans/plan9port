#include <u.h>
#include <libc.h>
#include <ip.h>
#include <bio.h>
#include <ndb.h>
#include "dns.h"

/*
 *  a dictionary of domain names for packing messages
 */
enum
{
	Ndict=	64
};
typedef struct Dict	Dict;
struct Dict
{
	struct {
		ushort	offset;		/* pointer to packed name in message */
		char	*name;		/* pointer to unpacked name in buf */
	} x[Ndict];
	int n;			/* size of dictionary */
	uchar *start;		/* start of packed message */
	char buf[4*1024];	/* buffer for unpacked names */
	char *ep;		/* first free char in buf */
};

#define NAME(x)		p = pname(p, ep, x, dp)
#define SYMBOL(x)	p = psym(p, ep, x)
#define STRING(x)	p = pstr(p, ep, x)
#define BYTES(x, n)	p = pbytes(p, ep, x, n)
#define USHORT(x)	p = pushort(p, ep, x)
#define UCHAR(x)	p = puchar(p, ep, x)
#define ULONG(x)	p = pulong(p, ep, x)
#define V4ADDR(x)	p = pv4addr(p, ep, x)
#define V6ADDR(x)	p = pv6addr(p, ep, x)

static uchar*
psym(uchar *p, uchar *ep, char *np)
{
	int n;

	n = strlen(np);
	if(n >= Strlen)			/* DNS maximum length string */
		n = Strlen - 1;
	if(ep - p < n+1)		/* see if it fits in the buffer */
		return ep+1;
	*p++ = n;
	memcpy(p, np, n);
	return p + n;
}

static uchar*
pstr(uchar *p, uchar *ep, char *np)
{
	int n;

	n = strlen(np);
	if(n >= Strlen)			/* DNS maximum length string */
		n = Strlen - 1;
	if(ep - p < n+1)		/* see if it fits in the buffer */
		return ep+1;
	*p++ = n;
	memcpy(p, np, n);
	return p + n;
}

static uchar*
pbytes(uchar *p, uchar *ep, uchar *np, int n)
{
	if(ep - p < n)
		return ep+1;
	memcpy(p, np, n);
	return p + n;
}

static uchar*
puchar(uchar *p, uchar *ep, int val)
{
	if(ep - p < 1)
		return ep+1;
	*p++ = val;
	return p;
}

static uchar*
pushort(uchar *p, uchar *ep, int val)
{
	if(ep - p < 2)
		return ep+1;
	*p++ = val>>8;
	*p++ = val;
	return p;
}

static uchar*
pulong(uchar *p, uchar *ep, int val)
{
	if(ep - p < 4)
		return ep+1;
	*p++ = val>>24;
	*p++ = val>>16;
	*p++ = val>>8;
	*p++ = val;
	return p;
}

static uchar*
pv4addr(uchar *p, uchar *ep, char *name)
{
	uchar ip[IPaddrlen];

	if(ep - p < 4)
		return ep+1;
	parseip(ip, name);
	v6tov4(p, ip);
	return p + 4;

}

static uchar*
pv6addr(uchar *p, uchar *ep, char *name)
{
	if(ep - p < IPaddrlen)
		return ep+1;
	parseip(p, name);
	return p + IPaddrlen;

}

static uchar*
pname(uchar *p, uchar *ep, char *np, Dict *dp)
{
	char *cp;
	int i;
	char *last;		/* last component packed */

	if(strlen(np) >= Domlen)	/* make sure we don't exceed DNS limits */
		return ep+1;

	last = 0;
	while(*np){
		/* look through every component in the dictionary for a match */
		for(i = 0; i < dp->n; i++){
			if(strcmp(np, dp->x[i].name) == 0){
				if(ep - p < 2)
					return ep+1;
				*p++ = (dp->x[i].offset>>8) | 0xc0;
				*p++ = dp->x[i].offset;
				return p;
			}
		}

		/* if there's room, enter this name in dictionary */
		if(dp->n < Ndict){
			if(last){
				/* the whole name is already in dp->buf */
				last = strchr(last, '.') + 1;
				dp->x[dp->n].name = last;
				dp->x[dp->n].offset = p - dp->start;
				dp->n++;
			} else {
				/* add to dp->buf */
				i = strlen(np);
				if(dp->ep + i + 1 < &dp->buf[sizeof(dp->buf)]){
					strcpy(dp->ep, np);
					dp->x[dp->n].name = dp->ep;
					last = dp->ep;
					dp->x[dp->n].offset = p - dp->start;
					dp->ep += i + 1;
					dp->n++;
				}
			}
		}

		/* put next component into message */
		cp = strchr(np, '.');
		if(cp == 0){
			i = strlen(np);
			cp = np + i;	/* point to null terminator */
		} else {
			i = cp - np;
			cp++;		/* point past '.' */
		}
		if(ep-p < i+1)
			return ep+1;
		*p++ = i;		/* count of chars in label */
		memcpy(p, np, i);
		np = cp;
		p += i;
	}

	if(p >= ep)
		return ep+1;
	*p++ = 0;	/* add top level domain */

	return p;
}

static uchar*
convRR2M(RR *rp, uchar *p, uchar *ep, Dict *dp)
{
	uchar *lp, *data;
	int len, ttl;
	Txt *t;

	NAME(rp->owner->name);
	USHORT(rp->type);
	USHORT(rp->owner->class);

	/* egregious overuse of ttl (it's absolute time in the cache) */
	if(rp->db)
		ttl = rp->ttl;
	else
		ttl = rp->ttl - now;
	if(ttl < 0)
		ttl = 0;
	ULONG(ttl);

	lp = p;			/* leave room for the rdata length */
	p += 2;
	data = p;

	if(data >= ep)
		return p+1;

	switch(rp->type){
	case Thinfo:
		SYMBOL(rp->cpu->name);
		SYMBOL(rp->os->name);
		break;
	case Tcname:
	case Tmb:
	case Tmd:
	case Tmf:
	case Tns:
		NAME(rp->host->name);
		break;
	case Tmg:
	case Tmr:
		NAME(rp->mb->name);
		break;
	case Tminfo:
		NAME(rp->rmb->name);
		NAME(rp->mb->name);
		break;
	case Tmx:
		USHORT(rp->pref);
		NAME(rp->host->name);
		break;
	case Ta:
		V4ADDR(rp->ip->name);
		break;
	case Taaaa:
		V6ADDR(rp->ip->name);
		break;
	case Tptr:
		NAME(rp->ptr->name);
		break;
	case Tsoa:
		NAME(rp->host->name);
		NAME(rp->rmb->name);
		ULONG(rp->soa->serial);
		ULONG(rp->soa->refresh);
		ULONG(rp->soa->retry);
		ULONG(rp->soa->expire);
		ULONG(rp->soa->minttl);
		break;
	case Ttxt:
		for(t = rp->txt; t != nil; t = t->next)
			STRING(t->p);
		break;
	case Tnull:
		BYTES(rp->null->data, rp->null->dlen);
		break;
	case Trp:
		NAME(rp->rmb->name);
		NAME(rp->rp->name);
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
		NAME(rp->sig->signer->name);
		BYTES(rp->sig->data, rp->sig->dlen);
		break;
	case Tcert:
		USHORT(rp->cert->type);
		USHORT(rp->cert->tag);
		UCHAR(rp->cert->alg);
		BYTES(rp->cert->data, rp->cert->dlen);
		break;
	}

	/* stuff in the rdata section length */
	len = p - data;
	*lp++ = len >> 8;
	*lp = len;

	return p;
}

static uchar*
convQ2M(RR *rp, uchar *p, uchar *ep, Dict *dp)
{
	NAME(rp->owner->name);
	USHORT(rp->type);
	USHORT(rp->owner->class);
	return p;
}

static uchar*
rrloop(RR *rp, int *countp, uchar *p, uchar *ep, Dict *dp, int quest)
{
	uchar *np;

	*countp = 0;
	for(; rp && p < ep; rp = rp->next){
		if(quest)
			np = convQ2M(rp, p, ep, dp);
		else
			np = convRR2M(rp, p, ep, dp);
		if(np > ep)
			break;
		p = np;
		(*countp)++;
	}
	return p;
}

/*
 *  convert into a message
 */
int
convDNS2M(DNSmsg *m, uchar *buf, int len)
{
	uchar *p, *ep, *np;
	Dict d;

	d.n = 0;
	d.start = buf;
	d.ep = d.buf;
	memset(buf, 0, len);
	m->qdcount = m->ancount = m->nscount = m->arcount = 0;

	/* first pack in the RR's so we can get real counts */
	p = buf + 12;
	ep = buf + len;
	p = rrloop(m->qd, &m->qdcount, p, ep, &d, 1);
	p = rrloop(m->an, &m->ancount, p, ep, &d, 0);
	p = rrloop(m->ns, &m->nscount, p, ep, &d, 0);
	p = rrloop(m->ar, &m->arcount, p, ep, &d, 0);
	if(p > ep)
		return -1;

	/* now pack the rest */
	np = p;
	p = buf;
	ep = buf + len;
	USHORT(m->id);
	USHORT(m->flags);
	USHORT(m->qdcount);
	USHORT(m->ancount);
	USHORT(m->nscount);
	USHORT(m->arcount);
	if(p > ep)
		return -1;

	return np - buf;
}
