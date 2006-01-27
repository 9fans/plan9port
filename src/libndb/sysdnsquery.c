#include <u.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <netdb.h>
#include <libc.h>
#include <ip.h>
#include <bio.h>
#include <ndb.h>
#include "ndbhf.h"

static void nstrcpy(char*, char*, int);
static void mkptrname(char*, char*, int);
static Ndbtuple *doquery(char*, char*);

/*
 * Run a DNS lookup for val/type on net.
 */
Ndbtuple*
dnsquery(char *net, char *val, char *type)
{
	static int init;
	char rip[128];	
	Ndbtuple *t;

	USED(net);
	
	if(!init){
		init = 1;
		fmtinstall('I', eipfmt);
	}
	/* give up early on stupid questions - vwhois */
	if(strcmp(val, "::") == 0 || strcmp(val, "0.0.0.0") == 0)
		return nil;
	
	/* zero out the error string */
	werrstr("");
	
	/* if this is a reverse lookup, first look up the domain name */
	if(strcmp(type, "ptr") == 0){
		mkptrname(val, rip, sizeof rip);
		t = doquery(rip, "ptr");
	}else
		t = doquery(val, type);
	
	return t;
}

/*
 *  convert address into a reverse lookup address
 */
static void
mkptrname(char *ip, char *rip, int rlen)
{
	char buf[128];
	char *p, *np;
	int len;

	if(strstr(ip, "in-addr.arpa") || strstr(ip, "IN-ADDR.ARPA")){
		nstrcpy(rip, ip, rlen);
		return;
	}

	nstrcpy(buf, ip, sizeof buf);
	for(p = buf; *p; p++)
		;
	*p = '.';
	np = rip;
	len = 0;
	while(p >= buf){
		len++;
		p--;
		if(*p == '.'){
			memmove(np, p+1, len);
			np += len;
			len = 0;
		}
	}
	memmove(np, p+1, len);
	np += len;
	strcpy(np, "in-addr.arpa");
}

static void
nstrcpy(char *to, char *from, int len)
{
	strncpy(to, from, len);
	to[len-1] = 0;
}

/*
 * Disgusting, ugly interface to libresolv,
 * which everyone seems to have.
 */
enum
{
	MAXRR = 100,
	MAXDNS = 4096,
};

static int name2type(char*);
static uchar *skipquestion(uchar*, uchar*, uchar*, int);
static uchar *unpack(uchar*, uchar*, uchar*, Ndbtuple**, int);
static uchar *rrnext(uchar*, uchar*, uchar*, Ndbtuple**);
static Ndbtuple *rrunpack(uchar*, uchar*, uchar**, char*, ...);

static Ndbtuple*
doquery(char *name, char *type)
{
	int n, nstype;
	uchar *buf, *p;
	HEADER *h;
	Ndbtuple *t;

	if((nstype = name2type(type)) < 0){
		werrstr("unknown dns type %s", type);
		return nil;
	}

	buf = malloc(MAXDNS);
	if(buf == nil)
		return nil;

	if((n = res_search(name, ns_c_in, nstype, buf, MAXDNS)) < 0){
		free(buf);
		return nil;
	}
	if(n >= MAXDNS){
		free(buf);
		werrstr("too much dns information");
		return nil;
	}
	
	h = (HEADER*)buf;
	h->qdcount = ntohs(h->qdcount);
	h->ancount = ntohs(h->ancount);
	h->nscount = ntohs(h->nscount);
	h->arcount = ntohs(h->arcount);
	
	p = buf+sizeof(HEADER);
	p = skipquestion(buf, buf+n, p, h->qdcount);
	p = unpack(buf, buf+n, p, &t, h->ancount);
	USED(p);
	return t;
}

static struct {
	char *s;
	int t;
} dnsnames[] =
{
	"ip",		ns_t_a,
	"ns",	ns_t_ns,
	"md",	ns_t_md,
	"mf",	ns_t_mf,
	"cname",	ns_t_cname,
	"soa",	ns_t_soa,
	"mb",	ns_t_mb,
	"mg",	ns_t_mg,
	"mr",	ns_t_mr,
	"null",	ns_t_null,
	"ptr",	ns_t_ptr,
	"hinfo",	ns_t_hinfo,
	"minfo",	ns_t_minfo,
	"mx",	ns_t_mx,
	"txt",	ns_t_txt,
	"rp",	ns_t_rp,
	"key",	ns_t_key,
	"cert",	ns_t_cert,
	"sig",	ns_t_sig,
	"aaaa",	ns_t_aaaa,
	"ixfr",	ns_t_ixfr,
	"axfr",	ns_t_axfr,
	"all",	ns_t_any,
};

static char*
type2name(int t)
{
	int i;
	
	for(i=0; i<nelem(dnsnames); i++)
		if(dnsnames[i].t == t)
			return dnsnames[i].s;
	return nil;
}

static int
name2type(char *name)
{
	int i;
	
	for(i=0; i<nelem(dnsnames); i++)
		if(strcmp(name, dnsnames[i].s) == 0)
			return dnsnames[i].t;
	return -1;
}

static uchar*
skipquestion(uchar *buf, uchar *ebuf, uchar *p, int n)
{
	int i, len;
	char tmp[100];
	
	for(i=0; i<n; i++){
		if((len = dn_expand(buf, ebuf, p, tmp, sizeof tmp)) <= 0)
			return nil;
		p += NS_QFIXEDSZ+len;
	}
	return p;
}

static uchar*
unpack(uchar *buf, uchar *ebuf, uchar *p, Ndbtuple **tt, int n)
{
	int i;
	Ndbtuple *first, *last, *t;

	*tt = nil;
	first = nil;
	last = nil;
	for(i=0; i<n; i++){
		if((p = rrnext(buf, ebuf, p, &t)) == nil){
			if(first)
				ndbfree(first);
			return nil;
		}
		if(t == nil)	/* unimplemented rr type */
			continue;
		if(last)
			last->entry = t;
		else
			first = t;
		for(last=t; last->entry; last=last->entry)
			last->line = last->entry;
		last->line = t;
	}
	*tt = first;
	return p;
}

#define G2(p) nhgets(p)
#define G4(p) nhgetl(p)

static uchar*
rrnext(uchar *buf, uchar *ebuf, uchar *p, Ndbtuple **tt)
{
	char tmp[Ndbvlen];
	char b[MAXRR];
	uchar ip[IPaddrlen];
	int len;
	Ndbtuple *first, *t;
	int rrtype;
	int rrlen;

	first = nil;
	t = nil;
	*tt = nil;
	if(p == nil)
		return nil;

	if((len = dn_expand(buf, ebuf, p, b, sizeof b)) < 0){
	corrupt:
		werrstr("corrupt dns packet");
		if(first)
			ndbfree(first);
		return nil;
	}
	p += len;
	
	rrtype = G2(p);
	rrlen = G2(p+8);
	p += 10;
	
	if(rrtype == ns_t_ptr)
		first = ndbnew("ptr", b);
	else
		first = ndbnew("dom", b);

	switch(rrtype){
	default:
		goto end;
	case ns_t_hinfo:
		t = rrunpack(buf, ebuf, &p, "YY", "cpu", "os");
		break;
	case ns_t_minfo:
		t = rrunpack(buf, ebuf, &p, "NN", "mbox", "mbox");
		break;
	case ns_t_mx:
		t = rrunpack(buf, ebuf, &p, "SN", "pref", "mx");
		break;
	case ns_t_cname:
	case ns_t_md:
	case ns_t_mf:
	case ns_t_mg:
	case ns_t_mr:
	case ns_t_mb:
	case ns_t_ns:
	case ns_t_ptr:
	case ns_t_rp:
		t = rrunpack(buf, ebuf, &p, "N", type2name(rrtype));
		break;
	case ns_t_a:
		if(rrlen != IPv4addrlen)
			goto corrupt;
		memmove(ip, v4prefix, IPaddrlen);
		memmove(ip+IPv4off, p, IPv4addrlen);
		snprint(tmp, sizeof tmp, "%I", ip);
		t = ndbnew("ip", tmp);
		p += rrlen;
		break;
	case ns_t_aaaa:
		if(rrlen != IPaddrlen)
			goto corrupt;
		snprint(tmp, sizeof tmp, "%I", ip);
		t = ndbnew("ip", tmp);
		p += rrlen;
		break;
	case ns_t_null:
		snprint(tmp, sizeof tmp, "%.*H", rrlen, p);
		t = ndbnew("null", tmp);
		p += rrlen;
		break;
	case ns_t_txt:
		t = rrunpack(buf, ebuf, &p, "Y", "txt");
		break;

	case ns_t_soa:
		t = rrunpack(buf, ebuf, &p, "NNLLLLL", "ns", "mbox", 
			"serial", "refresh", "retry", "expire", "ttl");
		break;

	case ns_t_key:
		t = rrunpack(buf, ebuf, &p, "SCCY", "flags", "proto", "alg", "key");
		break;
	
	case ns_t_sig:
		t = rrunpack(buf, ebuf, &p, "SCCLLLSNY", "type", "alg", "labels",
			"ttl", "exp", "incep", "tag", "signer", "sig");
		break;
	
	case ns_t_cert:
		t = rrunpack(buf, ebuf, &p, "SSCY", "type", "tag", "alg", "cert");
		break;
	}
	if(t == nil)
		goto corrupt;

end:
	first->entry = t;
	*tt = first;
	return p;
}

static Ndbtuple*
rrunpack(uchar *buf, uchar *ebuf, uchar **pp, char *fmt, ...)
{
	char *name;
	int len, n;
	uchar *p;
	va_list arg;
	Ndbtuple *t, *first, *last;
	char tmp[Ndbvlen];
	
	p = *pp;
	va_start(arg, fmt);
	first = nil;
	last = nil;
	for(; *fmt; fmt++){
		name = va_arg(arg, char*);
		switch(*fmt){
		default:
			return nil;
		case 'C':
			snprint(tmp, sizeof tmp, "%d", *p++);
			break;
		case 'S':
			snprint(tmp, sizeof tmp, "%d", G2(p));
			p += 2;
			break;
		case 'L':
			snprint(tmp, sizeof tmp, "%d", G4(p));
			p += 4;
			break;
		case 'N':
			if((len = dn_expand(buf, ebuf, p, tmp, sizeof tmp)) < 0)
				return nil;
			p += len;
			break;
		case 'Y':
			len = *p++;
			n = len;
			if(n >= sizeof tmp)
				n = sizeof tmp-1;
			memmove(tmp, p, n);
			p += len;
			tmp[n] = 0;
			break;
		}
		t = ndbnew(name, tmp);
		if(last)
			last->entry = t;
		else
			first = t;
		last = t;
	}
	*pp = p;
	return first;
}
