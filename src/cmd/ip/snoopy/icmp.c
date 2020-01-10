#include <u.h>
#include <libc.h>
#include <ip.h>
#include "dat.h"
#include "protos.h"

typedef struct Hdr	Hdr;
struct Hdr
{	uchar	type;
	uchar	code;
	uchar	cksum[2];	/* Checksum */
	uchar	data[1];
};

enum
{
	ICMPLEN=	4
};

enum
{
	Ot,	/* type */
	Op,	/* next protocol */
};

static Field p_fields[] =
{
	{"t",		Fnum,	Ot,	"type",	} ,
	{0}
};

enum
{
	EchoRep=	0,
	Unreachable=	3,
	SrcQuench=	4,
	Redirect=	5,
	EchoReq=	8,
	TimeExceed=	11,
	ParamProb=	12,
	TSreq=		13,
	TSrep=		14,
	InfoReq=	15,
	InfoRep=	16
};

static Mux p_mux[] =
{
	{"ip",	Unreachable, },
	{"ip",	SrcQuench, },
	{"ip",	Redirect, },
	{"ip",	TimeExceed, },
	{"ip",	ParamProb, },
	{0}
};

char *icmpmsg[256] =
{
[EchoRep]	"EchoRep",
[Unreachable]	"Unreachable",
[SrcQuench]	"SrcQuench",
[Redirect]	"Redirect",
[EchoReq]	"EchoReq",
[TimeExceed]	"TimeExceed",
[ParamProb]	"ParamProb",
[TSreq]		"TSreq",
[TSrep]		"TSrep",
[InfoReq]	"InfoReq",
[InfoRep]	"InfoRep"
};

static void
p_compile(Filter *f)
{
	if(f->op == '='){
		compile_cmp(udp.name, f, p_fields);
		return;
	}
	if(strcmp(f->s, "ip") == 0){
		f->pr = p_mux->pr;
		f->subop = Op;
		return;
	}
	sysfatal("unknown icmp field or protocol: %s", f->s);
}

static int
p_filter(Filter *f, Msg *m)
{
	Hdr *h;

	if(m->pe - m->ps < ICMPLEN)
		return 0;

	h = (Hdr*)m->ps;
	m->ps += ICMPLEN;

	switch(f->subop){
	case Ot:
		if(h->type == f->ulv)
			return 1;
		break;
	case Op:
		switch(h->type){
		case Unreachable:
		case TimeExceed:
		case SrcQuench:
		case Redirect:
		case ParamProb:
			m->ps += 4;
			return 1;
		}
	}
	return 0;
}

static int
p_seprint(Msg *m)
{
	Hdr *h;
	char *tn;
	char *p = m->p;
	char *e = m->e;
	ushort cksum2, cksum;

	h = (Hdr*)m->ps;
	m->ps += ICMPLEN;
	m->pr = &dump;

	if(m->pe - m->ps < ICMPLEN)
		return -1;

	tn = icmpmsg[h->type];
	if(tn == nil)
		p = seprint(p, e, "t=%ud c=%d ck=%4.4ux", h->type,
			h->code, (ushort)NetS(h->cksum));
	else
		p = seprint(p, e, "t=%s c=%d ck=%4.4ux", tn,
			h->code, (ushort)NetS(h->cksum));
	if(Cflag){
		cksum = NetS(h->cksum);
		h->cksum[0] = 0;
		h->cksum[1] = 0;
		cksum2 = ~ptclbsum((uchar*)h, m->pe - m->ps + ICMPLEN) & 0xffff;
		if(cksum != cksum2)
			p = seprint(p,e, " !ck=%4.4ux", cksum2);
	}
	switch(h->type){
	case EchoRep:
	case EchoReq:
		m->ps += 4;
		p = seprint(p, e, " id=%ux seq=%ux",
			NetS(h->data), NetS(h->data+2));
		break;
	case TSreq:
	case TSrep:
		m->ps += 12;
		p = seprint(p, e, " orig=%ud rcv=%ux xmt=%ux",
			NetL(h->data), NetL(h->data+4),
			NetL(h->data+8));
		m->pr = nil;
		break;
	case InfoReq:
	case InfoRep:
		break;
	case Unreachable:
	case TimeExceed:
	case SrcQuench:
		m->ps += 4;
		m->pr = &ip;
		break;
	case Redirect:
		m->ps += 4;
		m->pr = &ip;
		p = seprint(p, e, "gw=%V", h->data);
		break;
	case ParamProb:
		m->ps += 4;
		m->pr = &ip;
		p = seprint(p, e, "ptr=%2.2ux", h->data[0]);
		break;
	}
	m->p = p;
	return 0;
}

Proto icmp =
{
	"icmp",
	p_compile,
	p_filter,
	p_seprint,
	p_mux,
	"%lud",
	p_fields,
	defaultframer
};
