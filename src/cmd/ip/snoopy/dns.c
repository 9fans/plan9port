#include <u.h>
#include <libc.h>
#include <ip.h>
#include "dat.h"
#include "protos.h"

enum
{
	/* RR types */
	Ta=	1,
	Tns=	2,
	Tmd=	3,
	Tmf=	4,
	Tcname=	5,
	Tsoa=	6,
	Tmb=	7,
	Tmg=	8,
	Tmr=	9,
	Tnull=	10,
	Twks=	11,
	Tptr=	12,
	Thinfo=	13,
	Tminfo=	14,
	Tmx=	15,
	Ttxt=	16,
	Trp=	17,
	Tsig=	24,
	Tkey=	25,
	Taaaa=	28,
	Tcert=	37,

	/* query types (all RR types are also queries) */
	Tixfr=	251,	/* incremental zone transfer */
	Taxfr=	252,	/* zone transfer */
	Tmailb=	253,	/* { Tmb, Tmg, Tmr } */
	Tall=	255,	/* all records */

	/* classes */
	Csym=	0,	/* internal symbols */
	Cin=	1,	/* internet */
	Ccs,		/* CSNET (obsolete) */
	Cch,		/* Chaos net */
	Chs,		/* Hesiod (?) */

	/* class queries (all class types are also queries) */
	Call=	255,	/* all classes */

	/* opcodes */
	Oquery=		0<<11,		/* normal query */
	Oinverse=	1<<11,		/* inverse query */
	Ostatus=	2<<11,		/* status request */
	Onotify=	4<<11,		/* notify slaves of updates */
	Omask=		0xf<<11,	/* mask for opcode */

	/* response codes */
	Rok=		0,
	Rformat=	1,	/* format error */
	Rserver=	2,	/* server failure (e.g. no answer from something) */
	Rname=		3,	/* bad name */
	Runimplimented=	4,	/* unimplemented */
	Rrefused=	5,	/* we don't like you */
	Rmask=		0xf,	/* mask for response */
	Rtimeout=	0x10,	/* timeout sending (for internal use only) */

	/* bits in flag word (other than opcode and response) */
	Fresp=		1<<15,	/* message is a response */
	Fauth=		1<<10,	/* true if an authoritative response */
	Ftrunc=		1<<9,	/* truncated message */
	Frecurse=	1<<8,	/* request recursion */
	Fcanrec=	1<<7,	/* server can recurse */
};

typedef struct Hdr	Hdr;
struct Hdr
{
	uchar	id[2];
	uchar	flags[2];
	uchar	qdcount[2];
	uchar	ancount[2];
	uchar	nscount[2];
	uchar	arcount[2];
};


static char*
getstr(uchar **pp, int *len, uchar *ep)
{
	uchar *p;
	int n;

	p = *pp;
	n = *p++;
	if(p+n > ep)
		return nil;
	*len = n;
	*pp = p+n;
	return (char*)p;
}

static char*
getname(uchar **pp, uchar *bp, uchar *ep)
{
	static char buf[2][512];
	static int toggle;
	char *tostart, *to;
	char *toend;
	int len, off, pointer, n;
	uchar *p;

	to = buf[toggle^=1];
	toend = to+sizeof buf[0];
	tostart = to;
	p = *pp;
	len = 0;
	pointer = 0;
	while(p < ep && *p){
		if((*p & 0xc0) == 0xc0){
			/* pointer to another spot in message */
			if(pointer == 0)
				*pp = p + 2;
			if(pointer++ > 10)
				return nil;
			off = ((p[0]<<8) + p[1]) & 0x3ff;
			p = bp + off;
			if(p >= ep)
				return nil;
			n = 0;
			continue;
		}
		n = *p++;
		if(to+n >= toend || p+n > ep)
			return nil;
		memmove(to, p, n);
		to += n;
		p += n;
		if(*p){
			if(to >= toend)
				return nil;
			*to++ = '.';
		}
	}
	if(to >= toend || p >= ep)
		return nil;
	*to = 0;
	if(!pointer)
		*pp = ++p;
	return tostart;
}

static char*
tname(int type)
{
	static char buf[20];

	switch(type){
	case Ta:
		return "a";
	case Tns:
		return "ns";
	case Tmd:
		return "md";
	case Tmf:
		return "mf";
	case Tcname:
		return "cname";
	case Tsoa:
		return "soa";
	case Tmb:
		return "mb";
	case Tmg:
		return "mg";
	case Tmr:
		return "mr";
	case Tnull:
		return "null";
	case Twks:
		return "wks";
	case Tptr:
		return "ptr";
	case Thinfo:
		return "hinfo";
	case Tminfo:
		return "minfo";
	case Tmx:
		return "mx";
	case Ttxt:
		return "txt";
	case Trp:
		return "rp";
	case Tsig:
		return "sig";
	case Tkey:
		return "key";
	case Taaaa:
		return "aaaa";
	case Tcert:
		return "cert";
	case Tixfr:
		return "ixfr";
	case Taxfr:
		return "axfr";
	case Tmailb:
		return "mailb";
	case Tall:
		return "all";
	}
	snprint(buf, sizeof buf, "%d", type);
	return buf;
}

static char*
cname(int class)
{
	static char buf[40];

	if(class == Cin)
		return "";

	snprint(buf, sizeof buf, "class=%d", class);
	return buf;
}

#define PR(name, len) utfnlen(name, len), name

extern int sflag;

static int
p_seprint(Msg *m)
{
	int i, pref;
	Hdr *h;
	uchar *p, *ep;
	int an, ns, ar, rlen;
	char *name, *prefix;
	int len1, len2;
	char *sym1, *sym2, *sep;
	int type;
	static int first = 1;

	if(first){
		first = 0;
		quotefmtinstall();
	}

	if(m->pe - m->ps < sizeof(Hdr))
		return -1;
	h = (Hdr*)m->ps;
	m->pr = nil;

	m->p = seprint(m->p, m->e, "id=%d flags=%04ux %d/%d/%d/%d",
			NetS(h->id), NetS(h->flags),
			NetS(h->qdcount), NetS(h->ancount),
			NetS(h->nscount), NetS(h->arcount));
	sep = ")\n\t";
	if(sflag)
		sep = ") ";
	p = m->ps + sizeof(Hdr);
	for(i=0; i<NetS(h->qdcount); i++){
		name = getname(&p, m->ps, m->pe);
		if(name == nil || p+4 > m->pe)
			goto error;
		m->p = seprint(m->p, m->e, "%sq=(%q %s%s",
			sep, name, tname(NetS(p)), cname(NetS(p+2)));
		p += 4;
	}

	an = NetS(h->ancount);
	ns = NetS(h->nscount);
	ar = NetS(h->arcount);
	while(an+ns+ar > 0){
		if(an > 0){
			prefix = "an";
			an--;
		}else if(ns > 0){
			prefix = "ns";
			ns--;
		}else{
			prefix = "ar";
			ar--;
		}
		name = getname(&p, m->ps, m->pe);
		if(name == nil || p+10 > m->pe)
			goto error;
		type = NetS(p);
		rlen = NetS(p+8);
		ep = p+10+rlen;
		if(ep > m->pe)
			goto error;
		m->p = seprint(m->p, m->e, "%s%s=(%q %s%s | ttl=%lud",
			sep, prefix, name, tname(type), cname(NetS(p+2)), NetL(p+4), rlen);
		p += 10;
		switch(type){
		default:
			p = ep;
			break;
		case Thinfo:
			sym1 = getstr(&p, &len1, ep);
			if(sym1 == nil)
				goto error;
			sym2 = getstr(&p, &len2, ep);
			if(sym2 == nil)
				goto error;
			m->p = seprint(m->p, m->e, " cpu=%.*s os=%.*s",
				PR(sym1, len1),
				PR(sym2, len2));
			break;
		case Tcname:
		case Tmb:
		case Tmd:
		case Tmf:
		case Tns:
		case Tmg:
		case Tmr:
		case Tptr:
			sym1 = getname(&p, m->ps, m->pe);
			if(sym1 == nil)
				goto error;
			m->p = seprint(m->p, m->e, " %q", sym1);
			break;
		case Tminfo:
			sym1 = getname(&p, m->ps, m->pe);
			if(sym1 == nil)
				goto error;
			sym2 = getname(&p, m->ps, m->pe);
			if(sym2 == nil)
				goto error;
			m->p = seprint(m->p, m->e, " %q %q", sym1, sym2);
			break;
		case Tmx:
			if(p+2 >= ep)
				goto error;
			pref = NetS(p);
			p += 2;
			sym1 = getname(&p, m->ps, m->pe);
			if(sym1 == nil)
				goto error;
			break;
		case Ta:
			if(p+4 > ep)
				goto error;
			m->p = seprint(m->p, m->e, " %V", p);
			p += 4;
			break;
		case Taaaa:
			if(p+16 > ep)
				goto error;
			m->p = seprint(m->p, m->e, " %I", p);
			p += 16;
			break;
		case Tsoa:
			sym1 = getname(&p, m->ps, m->pe);
			if(sym1 == nil)
				goto error;
			sym2 = getname(&p, m->ps, m->pe);
			if(sym2 == nil)
				goto error;
			if(p+20 > ep)
				goto error;
			m->p = seprint(m->p, m->e, " host=%q rmb=%q serial=%lud refresh=%lud retry=%lud expire=%lud minttl=%lud",
				sym1, sym2, NetL(p), NetL(p+4),
				NetL(p+8), NetL(p+12), NetL(p+16));
			break;
		case Ttxt:
			while(p < ep){
				sym1 = getstr(&p, &len1, ep);
				if(sym1 == nil)
					goto error;
				m->p = seprint(m->p, m->e, " %.*q", PR(sym1, len1));
			}
			break;
		case Tnull:
			m->p = seprint(m->p, m->e, " %.*H", rlen, p);
			p += rlen;
			break;
		case Trp:
			sym1 = getname(&p, m->ps, m->pe);
			if(sym1 == nil)
				goto error;
			sym2 = getname(&p, m->ps, m->pe);
			if(sym2 == nil)
				goto error;
			m->p = seprint(m->p, m->e, " rmb=%q rp=%q", sym1, sym2);
			break;
		case Tkey:
			if(rlen < 4)
				goto error;
			m->p = seprint(m->p, m->e, " flags=%04ux proto=%d alg=%d %.*H",
				NetS(p), p[3], p[4], rlen-4, p+4);
			p += rlen;
			break;

		case Tsig:
			if(rlen < 18)
				goto error;
			m->p = seprint(m->p, m->e, " type=%d alg=%d labels=%d ttl=%lud exp=%lud incep=%lud tag=%d %.*H",
				NetS(p), p[3], p[4], NetL(p+4), NetL(p+8), NetL(p+12), NetS(p+16),
				rlen-18, p+18);
			p += rlen;
			break;

		case Tcert:
			if(rlen < 5)
				goto error;
			m->p = seprint(m->p, m->e, " type=%d tag=%d alg=%d %.*H",
				NetS(p), NetS(p+2), p[4], rlen-5, p+5);
			p += rlen;
			break;
		}
		if(p != ep)
			goto error;
	}
	return 0;

error:
	m->p = seprint(m->p, m->e, " packet error!");
	return 0;
}

Proto dns =
{
	"dns",
	nil,
	nil,
	p_seprint,
	nil,
	nil,
	nil,
	defaultframer
};
