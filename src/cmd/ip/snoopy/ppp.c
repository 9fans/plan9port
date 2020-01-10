#include <u.h>
#include <libc.h>
#include <ip.h>
#include <libsec.h>
#include "dat.h"
#include "protos.h"

/* PPP stuff */
enum {
	PPP_addr=	0xff,
	PPP_ctl=	0x3,
	PPP_period=	3*1000,	/* period of retransmit process (in ms) */
};

/* PPP protocols */
enum {
	PPP_ip=		0x21,		/* internet */
	PPP_vjctcp=	0x2d,		/* compressing van jacobson tcp */
	PPP_vjutcp=	0x2f,		/* uncompressing van jacobson tcp */
	PPP_ml=		0x3d,		/* multi link */
	PPP_comp=	0xfd,		/* compressed packets */
	PPP_ipcp=	0x8021,		/* ip control */
	PPP_ccp=	0x80fd,		/* compression control */
	PPP_passwd=	0xc023,		/* passwd authentication */
	PPP_lcp=	0xc021,		/* link control */
	PPP_lqm=	0xc025,		/* link quality monitoring */
	PPP_chap=	0xc223,		/* challenge/response */
};

/* LCP protocol (and IPCP) */


typedef struct Lcppkt	Lcppkt;
struct Lcppkt
{
	uchar	code;
	uchar	id;
	uchar	len[2];
	uchar	data[1];
};

typedef struct Lcpopt	Lcpopt;
struct Lcpopt
{
	uchar	type;
	uchar	len;
	uchar	data[1];
};

enum
{
	/* LCP codes */
	Lconfreq=	1,
	Lconfack=	2,
	Lconfnak=	3,
	Lconfrej=	4,
	Ltermreq=	5,
	Ltermack=	6,
	Lcoderej=	7,
	Lprotorej=	8,
	Lechoreq=	9,
	Lechoack=	10,
	Ldiscard=	11,
	Lresetreq=	14,	/* for ccp only */
	Lresetack=	15,	/* for ccp only */

	/* Lcp configure options */
	Omtu=		1,
	Octlmap=	2,
	Oauth=		3,
	Oquality=	4,
	Omagic=		5,
	Opc=		7,
	Oac=		8,

	/* authentication protocols */
	APmd5=		5,
	APmschap=	128,

	/* Chap codes */
	Cchallenge=	1,
	Cresponse=	2,
	Csuccess=	3,
	Cfailure=	4,

	/* ipcp configure options */
	Oipaddrs=	1,
	Oipcompress=	2,
	Oipaddr=	3,
	Oipdns=		129,
	Oipwins=	130,
	Oipdns2=	131,
	Oipwins2=	132
};

char *
lcpcode[] = {
	0,
	"confreq",
	"confack",
	"confnak",
	"confrej",
	"termreq",
	"termack",
	"coderej",
	"protorej",
	"echoreq",
	"echoack",
	"discard",
	"id",
	"timeremain",
	"resetreq",
	"resetack"
};

static Mux p_mux[] =
{
	{"ip",		PPP_ip, },
	{"ppp_vjctcp",	PPP_vjctcp, },
	{"ppp_vjutcp",	PPP_vjutcp, },
	{"ppp_ml",	PPP_ml, },
	{"ppp_comp",	PPP_comp, },
	{"ppp_ipcp",	PPP_ipcp, },
	{"ppp_ccp",	PPP_ccp, },
	{"ppp_passwd",	PPP_passwd, },
	{"ppp_lcp",	PPP_lcp, },
	{"ppp_lqm",	PPP_lqm, },
	{"ppp_chap",	PPP_chap, },
	{0}
};

enum
{
	OOproto
};

static void
p_compile(Filter *f)
{
	Mux *m;

	for(m = p_mux; m->name != nil; m++)
		if(strcmp(f->s, m->name) == 0){
			f->pr = m->pr;
			f->ulv = m->val;
			f->subop = OOproto;
			return;
		}

	sysfatal("unknown ppp field or protocol: %s", f->s);
}

static int
p_filter(Filter *f, Msg *m)
{
	int proto;
	int len;

	if(f->subop != OOproto)
		return 0;

	len = m->pe - m->ps;
	if(len < 3)
		return -1;

	if(m->ps[0] == PPP_addr && m->ps[1] == PPP_ctl)
		m->ps += 2;

	proto = *m->ps++;
	if((proto&1) == 0)
		proto = (proto<<8) | *m->ps++;

	if(proto == f->ulv)
		return 1;

	return 0;
}

static int
p_seprint(Msg *m)
{
	int proto;
	int len;

	len = m->pe - m->ps;
	if(len < 3)
		return -1;

	if(m->ps[0] == PPP_addr && m->ps[1] == PPP_ctl)
		m->ps += 2;

	proto = *m->ps++;
	if((proto&1) == 0)
		proto = (proto<<8) | *m->ps++;

	m->p = seprint(m->p, m->e, "pr=%ud len=%d", proto, len);
	demux(p_mux, proto, proto, m, &dump);

	return 0;
}

static int
p_seprintchap(Msg *m)
{
	Lcppkt *lcp;
	char *p, *e;
	int len;

	if(m->pe-m->ps < 4)
		return -1;

	p = m->p;
	e = m->e;
	m->pr = nil;

	/* resize packet */
	lcp = (Lcppkt*)m->ps;
	len = NetS(lcp->len);
	if(m->ps+len < m->pe)
		m->pe = m->ps+len;
	else if(m->ps+len > m->pe)
		return -1;

	p = seprint(p, e, "id=%d code=%d", lcp->id, lcp->code);
	switch(lcp->code) {
	default:
		p = seprint(p, e, " data=%.*H", len>64?64:len, lcp->data);
		break;
	case 1:
	case 2:
		if(lcp->data[0] > len-4){
			p = seprint(p, e, "%.*H", len-4, lcp->data);
		} else {
			p = seprint(p, e, " %s=", lcp->code==1?"challenge ":"response ");
			p = seprint(p, e, "%.*H", lcp->data[0], lcp->data+1);
			p = seprint(p, e, " name=");
			p = seprint(p, e, "%.*H", len-4-lcp->data[0]-1, lcp->data+lcp->data[0]+1);
		}
		break;
	case 3:
	case 4:
		if(len > 64)
			len = 64;
		p = seprint(p, e, " %s=%.*H", lcp->code==3?"success ":"failure",
				len>64?64:len, lcp->data);
		break;
	}
	m->p = seprint(p, e, " len=%d", len);
	return 0;
}

static char*
seprintlcpopt(char *p, char *e, void *a, int len)
{
	Lcpopt *o;
	int proto, x, period;
	uchar *cp, *ecp;

	cp = a;
	ecp = cp+len;

	for(; cp < ecp; cp += o->len){
		o = (Lcpopt*)cp;
		if(cp + o->len > ecp || o->len == 0){
			p = seprint(p, e, " bad-opt-len=%d", o->len);
			return p;
		}

		switch(o->type){
		default:
			p = seprint(p, e, " (type=%d len=%d)", o->type, o->len);
			break;
		case Omtu:
			p = seprint(p, e, " mtu=%d", NetS(o->data));
			break;
		case Octlmap:
			p = seprint(p, e, " ctlmap=%ux", NetL(o->data));
			break;
		case Oauth:
			proto = NetS(o->data);
			switch(proto) {
			default:
				p = seprint(p, e, " auth=%d", proto);
				break;
			case PPP_passwd:
				p = seprint(p, e, " auth=passwd");
				break;
			case PPP_chap:
				p = seprint(p, e, " (auth=chap data=%2.2ux)", o->data[2]);
				break;
			}
			break;
		case Oquality:
			proto = NetS(o->data);
			switch(proto) {
			default:
				p = seprint(p, e, " qproto=%d", proto);
				break;
			case PPP_lqm:
				x = NetL(o->data+2)*10;
				period = (x+(PPP_period-1))/PPP_period;
				p = seprint(p, e, " (qproto=lqm period=%d)", period);
				break;
			}
		case Omagic:
			p = seprint(p, e, " magic=%ux", NetL(o->data));
			break;
		case Opc:
			p = seprint(p, e, " protocol-compress");
			break;
		case Oac:
			p = seprint(p, e, " addr-compress");
			break;
		}
	}
	return p;
}


static int
p_seprintlcp(Msg *m)
{
	Lcppkt *lcp;
	char *p, *e;
	int len;

	if(m->pe-m->ps < 4)
		return -1;

	p = m->p;
	e = m->e;
	m->pr = nil;

	lcp = (Lcppkt*)m->ps;
	len = NetS(lcp->len);
	if(m->ps+len < m->pe)
		m->pe = m->ps+len;
	else if(m->ps+len > m->pe)
		return -1;

	p = seprint(p, e, "id=%d code=%d", lcp->id, lcp->code);
	switch(lcp->code) {
	default:
		p = seprint(p, e, " data=%.*H", len>64?64:len, lcp->data);
		break;
	case Lconfreq:
	case Lconfack:
	case Lconfnak:
	case Lconfrej:
		p = seprint(p, e, "=%s", lcpcode[lcp->code]);
		p = seprintlcpopt(p, e, lcp->data, len-4);
		break;
	case Ltermreq:
	case Ltermack:
	case Lcoderej:
	case Lprotorej:
	case Lechoreq:
	case Lechoack:
	case Ldiscard:
		p = seprint(p, e, "=%s", lcpcode[lcp->code]);
		break;
	}
	m->p = seprint(p, e, " len=%d", len);
	return 0;
}

static char*
seprintipcpopt(char *p, char *e, void *a, int len)
{
	Lcpopt *o;
	uchar *cp, *ecp;

	cp = a;
	ecp = cp+len;

	for(; cp < ecp; cp += o->len){
		o = (Lcpopt*)cp;
		if(cp + o->len > ecp){
			p = seprint(p, e, " bad opt len %ux", o->type);
			return p;
		}

		switch(o->type){
		default:
			p = seprint(p, e, " (type=%d len=%d)", o->type, o->len);
			break;
		case Oipaddrs:
			p = seprint(p, e, " ipaddrs(deprecated)");
			break;
		case Oipcompress:
			p = seprint(p, e, " ipcompress");
			break;
		case Oipaddr:
			p = seprint(p, e, " ipaddr=%V", o->data);
			break;
		case Oipdns:
			p = seprint(p, e, " dnsaddr=%V", o->data);
			break;
		case Oipwins:
			p = seprint(p, e, " winsaddr=%V", o->data);
			break;
		case Oipdns2:
			p = seprint(p, e, " dns2addr=%V", o->data);
			break;
		case Oipwins2:
			p = seprint(p, e, " wins2addr=%V", o->data);
			break;
		}
	}
	return p;
}

static int
p_seprintipcp(Msg *m)
{
	Lcppkt *lcp;
	char *p, *e;
	int len;

	if(m->pe-m->ps < 4)
		return -1;

	p = m->p;
	e = m->e;
	m->pr = nil;

	lcp = (Lcppkt*)m->ps;
	len = NetS(lcp->len);
	if(m->ps+len < m->pe)
		m->pe = m->ps+len;
	else if(m->ps+len > m->pe)
		return -1;

	p = seprint(p, e, "id=%d code=%d", lcp->id, lcp->code);
	switch(lcp->code) {
	default:
		p = seprint(p, e, " data=%.*H", len>64?64:len, lcp->data);
		break;
	case Lconfreq:
	case Lconfack:
	case Lconfnak:
	case Lconfrej:
		p = seprint(p, e, "=%s", lcpcode[lcp->code]);
		p = seprintipcpopt(p, e, lcp->data, len-4);
		break;
	case Ltermreq:
	case Ltermack:
		p = seprint(p, e, "=%s", lcpcode[lcp->code]);
		break;
	}
	m->p = seprint(p, e, " len=%d", len);
	return 0;
}

static char*
seprintccpopt(char *p, char *e, void *a, int len)
{
	Lcpopt *o;
	uchar *cp, *ecp;

	cp = a;
	ecp = cp+len;

	for(; cp < ecp; cp += o->len){
		o = (Lcpopt*)cp;
		if(cp + o->len > ecp){
			p = seprint(p, e, " bad opt len %ux", o->type);
			return p;
		}

		switch(o->type){
		default:
			p = seprint(p, e, " type=%d ", o->type);
			break;
		case 0:
			p = seprint(p, e, " OUI=(%d %.2ux%.2ux%.2ux) ", o->type,
				o->data[0], o->data[1], o->data[2]);
			break;
		case 17:
			p = seprint(p, e, " Stac-LZS");
			break;
		case 18:
			p = seprint(p, e, " Microsoft-PPC=%ux", NetL(o->data));
			break;
		}
	}
	return p;
}

static int
p_seprintccp(Msg *m)
{
	Lcppkt *lcp;
	char *p, *e;
	int len;

	if(m->pe-m->ps < 4)
		return -1;

	p = m->p;
	e = m->e;
	m->pr = nil;

	lcp = (Lcppkt*)m->ps;
	len = NetS(lcp->len);
	if(m->ps+len < m->pe)
		m->pe = m->ps+len;
	else if(m->ps+len > m->pe)
		return -1;

	p = seprint(p, e, "id=%d code=%d", lcp->id, lcp->code);
	switch(lcp->code) {
	default:
		p = seprint(p, e, " data=%.*H", len>64?64:len, lcp->data);
		break;
	case Lconfreq:
	case Lconfack:
	case Lconfnak:
	case Lconfrej:
		p = seprint(p, e, "=%s", lcpcode[lcp->code]);
		p = seprintccpopt(p, e, lcp->data, len-4);
		break;
	case Ltermreq:
	case Ltermack:
	case Lresetreq:
	case Lresetack:
		p = seprint(p, e, "=%s", lcpcode[lcp->code]);
		break;
	}
	m->p = seprint(p, e, " len=%d", len);

	return 0;
}

static int
p_seprintcomp(Msg *m)
{
	char compflag[5];
	ushort x;
	int i;
	int len;

	len = m->pe-m->ps;
	if(len < 2)
		return -1;

	x = NetS(m->ps);
	m->ps += 2;
	i = 0;
	if(x & (1<<15))
		compflag[i++] = 'r';
	if(x & (1<<14))
		compflag[i++] = 'f';
	if(x & (1<<13))
		compflag[i++] = 'c';
	if(x & (1<<12))
		compflag[i++] = 'e';
	compflag[i] = 0;
	m->p = seprint(m->p, m->e, "flag=%s count=%.3ux", compflag, x&0xfff);
	m->p = seprint(m->p, m->e, " data=%.*H", len>64?64:len, m->ps);
	m->pr = nil;
	return 0;
}

Proto ppp =
{
	"ppp",
	p_compile,
	p_filter,
	p_seprint,
	p_mux,
	"%#.4lux",
	nil,
	defaultframer
};

Proto ppp_ipcp =
{
	"ppp_ipcp",
	p_compile,
	p_filter,
	p_seprintipcp,
	nil,
	nil,
	nil,
	defaultframer
};

Proto ppp_lcp =
{
	"ppp_lcp",
	p_compile,
	p_filter,
	p_seprintlcp,
	nil,
	nil,
	nil,
	defaultframer
};

Proto ppp_ccp =
{
	"ppp_ccp",
	p_compile,
	p_filter,
	p_seprintccp,
	nil,
	nil,
	nil,
	defaultframer
};

Proto ppp_chap =
{
	"ppp_chap",
	p_compile,
	p_filter,
	p_seprintchap,
	nil,
	nil,
	nil,
	defaultframer
};

Proto ppp_comp =
{
	"ppp_comp",
	p_compile,
	p_filter,
	p_seprintcomp,
	nil,
	nil,
	nil,
	defaultframer
};
