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
	ICMP6LEN=	4
};

enum
{
	Ot,	/* type */
	Op,	/* next protocol */};

static Field p_fields[] =
{
	{"t",		Fnum,	Ot,	"type",	} ,
	{0}
};

enum
{
	/* ICMPv6 types */
	EchoReply	= 0,
	UnreachableV6	= 1,
	PacketTooBigV6	= 2,
	TimeExceedV6	= 3,
	ParamProblemV6	= 4,
	Redirect	= 5,
	EchoRequest	= 8,
	TimeExceed	= 11,
	InParmProblem	= 12,
	Timestamp	= 13,
	TimestampReply	= 14,
	InfoRequest	= 15,
	InfoReply	= 16,
	AddrMaskRequest = 17,
	AddrMaskReply   = 18,
	EchoRequestV6	= 128,
	EchoReplyV6	= 129,
	RouterSolicit	= 133,
	RouterAdvert	= 134,
	NbrSolicit	= 135,
	NbrAdvert	= 136,
	RedirectV6	= 137,

	Maxtype6	= 137
};

static Mux p_mux[] =
{
	{"ip6",	UnreachableV6, },
	{"ip6",	RedirectV6, },
	{"ip6",	TimeExceedV6, },
	{0}
};

char *icmpmsg6[256] =
{
[EchoReply]		"EchoReply",
[UnreachableV6]		"UnreachableV6",
[PacketTooBigV6]	"PacketTooBigV6",
[TimeExceedV6]		"TimeExceedV6",
[Redirect]		"Redirect",
[EchoRequest]		"EchoRequest",
[TimeExceed]		"TimeExceed",
[InParmProblem]		"InParmProblem",
[Timestamp]		"Timestamp",
[TimestampReply]	"TimestampReply",
[InfoRequest]		"InfoRequest",
[InfoReply]		"InfoReply",
[AddrMaskRequest]	"AddrMaskRequest",
[AddrMaskReply]		"AddrMaskReply",
[EchoRequestV6]		"EchoRequestV6",
[EchoReplyV6]		"EchoReplyV6",
[RouterSolicit]		"RouterSolicit",
[RouterAdvert]		"RouterAdvert",
[NbrSolicit]		"NbrSolicit",
[NbrAdvert]		"NbrAdvert",
[RedirectV6]		"RedirectV6"
};

static char *unreachcode[] =
{
[0]	"no route to destination",
[1]	"comm with destination administratively prohibited",
[2]	"icmp unreachable: unassigned error code (2)",
[3]	"address unreachable",
[4]	"port unreachable",
[5]	"icmp unreachable: unknown code"
};

static char *timexcode[] =
{
[0]	"hop limit exc",
[1]	"reassmbl time exc",
[2]	"icmp time exc: unknown code"
};

static char *parpcode[] =
{
[0]	"erroneous header field encountered",
[1]	"unrecognized Next Header type encountered",
[2]	"unrecognized IPv6 option encountered",
[3]	"icmp par prob: unknown code"
};
enum
{
	sll	= 1,
	tll	= 2,
	pref	= 3,
	redir	= 4,
	mtu	= 5
};

static char *icmp6opts[256] =
{
[0]	"unknown opt",
[1]	"sll_addr",
[2]	"tll_addr",
[3]	"pref_opt",
[4]	"redirect",
[5]	"mtu_opt"
};

static void
p_compile(Filter *f)
{
	if(f->op == '='){
		compile_cmp(icmp6.name, f, p_fields);
		return;
	}
	if(strcmp(f->s, "ip6") == 0){
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

	if(m->pe - m->ps < ICMP6LEN)
		return 0;

	h = (Hdr*)m->ps;
	m->ps += ICMP6LEN;

	switch(f->subop){

	case Ot:
		if(h->type == f->ulv)
			return 1;
		break;
	case Op:
		switch(h->type){
		case UnreachableV6:
		case RedirectV6:
		case TimeExceedV6:
			m->ps += 4;
			return 1;
		}
	}
	return 0;
}

static char*
opt_seprint(Msg *m)
{
	int otype, osz, pktsz;
	uchar *a;
	char *p = m->p;
	char *e = m->e;
	char *opt;
	char optbuf[12];

	pktsz = m->pe - m->ps;
	a = m->ps;
	while (pktsz > 0) {
		otype = *a;
		opt = icmp6opts[otype];
		if(opt == nil){
			sprint(optbuf, "0x%ux", otype);
			opt = optbuf;
		}
		osz = (*(a+1)) * 8;

		switch (otype) {
		default:
			p = seprint(p, e, "\n	  option=%s ", opt);
			m->pr = &dump;
			return p;

		case sll:
		case tll:
			if ((pktsz < osz) || (osz != 8)) {
				p = seprint(p, e, "\n	  option=%s bad size=%d", opt, osz);
				m->pr = &dump;
				return p;
			}
			p = seprint(p, e, "\n	  option=%s maddr=%E", opt, a+2);
			pktsz -= osz;
			a += osz;
			break;

		case pref:
			if ((pktsz < osz) || (osz != 32)) {
				p = seprint(p, e, "\n	  option=%s: bad size=%d", opt, osz);
				m->pr = &dump;
				return p;
			}

			p = seprint(p, e, "\n	  option=%s pref=%I preflen=%3.3d lflag=%1.1d aflag=%1.1d unused1=%1.1d validlt=%d preflt=%d unused2=%1.1d",
				opt,
				a+16,
				(int) (*(a+2)),
				(*(a+3) & (1 << 7))!=0,
				(*(a+3) & (1 << 6))!=0,
				(*(a+3) & 63) != 0,
				NetL(a+4),
				NetL(a+8),
				NetL(a+12)!=0);

			pktsz -= osz;
			a += osz;
			break;

		case redir:
			if (pktsz < osz) {
				p = seprint(p, e, "\n	  option=%s: bad size=%d", opt, osz);
				m->pr = &dump;
				return p;
			}

			p = seprint(p, e, "\n	  option=%s len %d", opt, osz);
			a += osz;
			m->ps = a;
			return p;
			break;

		case mtu:
			if ((pktsz < osz) || (osz != 8)) {
				p = seprint(p, e, "\n	  option=%s: bad size=%d", opt, osz);
				m->pr = &dump;
				return p;
			}

			p = seprint(p, e, "\n	  option=%s unused=%1.1d mtu=%d", opt, NetL(a+2)!=0, NetL(a+4));
			pktsz -= osz;
			a += osz;
			break;
		}
	}

	m->ps = a;
	return p;
}

static int
p_seprint(Msg *m)
{
	Hdr *h;
	char *tn;
	char *p = m->p;
	char *e = m->e;
	int i;
	uchar *a;
/*	ushort cksum2, cksum; */

	h = (Hdr*)m->ps;
	m->ps += ICMP6LEN;
	m->pr = &dump;
	a = m->ps;

	if(m->pe - m->ps < ICMP6LEN)
		return -1;

	tn = icmpmsg6[h->type];
	if(tn == nil)
		p = seprint(p, e, "t=%ud c=%d ck=%4.4ux", h->type,
			h->code, (ushort)NetS(h->cksum));
	else
		p = seprint(p, e, "t=%s c=%d ck=%4.4ux", tn,
			h->code, (ushort)NetS(h->cksum));

	/*
	if(Cflag){
		cksum = NetS(h->cksum);
		h->cksum[0] = 0;
		h->cksum[1] = 0;
		cksum2 = ~ptclbsum((uchar*)h, m->pe - m->ps + ICMP6LEN) & 0xffff;
		if(cksum != cksum2)
			p = seprint(p,e, " !ck=%4.4ux", cksum2);
	}
	*/

	switch(h->type){

	case UnreachableV6:
		m->ps += 4;
		m->pr = &ip6;
		if (h->code >= nelem(unreachcode))
			i = nelem(unreachcode)-1;
		else
			i = h->code;
		p = seprint(p, e, " code=%s unused=%1.1d ", unreachcode[i], NetL(a)!=0);
		break;

	case PacketTooBigV6:
		m->ps += 4;
		m->pr = &ip6;
		p = seprint(p, e, " mtu=%4.4d ", NetL(a));
		break;

	case TimeExceedV6:
		m->ps += 4;
		m->pr = &ip6;
		if (h->code >= nelem(timexcode))
			i = nelem(timexcode)-1;
		else
			i = h->code;
		p = seprint(p, e, " code=%s unused=%1.1d ", timexcode[i], NetL(a)!=0);
		break;

	case ParamProblemV6:
		m->ps += 4;
		m->pr = &ip6;
		if (h->code > nelem(parpcode))
			i = nelem(parpcode)-1;
		else
			i = h->code;
		p = seprint(p, e, " code=%s ptr=%2.2ux", parpcode[i], h->data[0]);
		break;

	case EchoReplyV6:
	case EchoRequestV6:
		m->ps += 4;
		p = seprint(p, e, " id=%ux seq=%ux",
			NetS(h->data), NetS(h->data+2));
		break;

	case RouterSolicit:
		m->ps += 4;
		m->pr = nil;
		m->p = seprint(p, e, " unused=%1.1d ", NetL(a)!=0);
		p = opt_seprint(m);
		break;

	case RouterAdvert:
		m->ps += 12;
		m->pr = nil;
		m->p = seprint(p, e, " hoplim=%3.3d mflag=%1.1d oflag=%1.1d unused=%1.1d routerlt=%8.8d reachtime=%d rxmtimer=%d",
			(int) *a,
			(*(a+1) & (1 << 7)) != 0,
			(*(a+1) & (1 << 6)) != 0,
			(*(a+1) & 63) != 0,
			NetS(a+2),
			NetL(a+4),
			NetL(a+8));
		p = opt_seprint(m);
		break;

	case NbrSolicit:
		m->ps += 20;
		m->pr = nil;
		m->p = seprint(p, e, " unused=%1.1d targ %I", NetL(a)!=0, a+4);
		p = opt_seprint(m);
		break;

	case NbrAdvert:
		m->ps += 20;
		m->pr = nil;
		m->p = seprint(p, e, " rflag=%1.1d sflag=%1.1d oflag=%1.1d targ=%I",
			(*a & (1 << 7)) != 0,
			(*a & (1 << 6)) != 0,
			(*a & (1 << 5)) != 0,
			a+4);
		p = opt_seprint(m);
		break;

	case RedirectV6:
		m->ps += 36;
		m->pr = &ip6;
		m->p = seprint(p, e, " unused=%1.1d targ=%I dest=%I", NetL(a)!=0, a+4, a+20);
		p = opt_seprint(m);
		break;

	case Timestamp:
	case TimestampReply:
		m->ps += 12;
		p = seprint(p, e, " orig=%ud rcv=%ux xmt=%ux",
			NetL(h->data), NetL(h->data+4),
			NetL(h->data+8));
		m->pr = nil;
		break;

	case InfoRequest:
	case InfoReply:
		break;

	}
	m->p = p;
	return 0;
}

Proto icmp6 =
{
	"icmp6",
	p_compile,
	p_filter,
	p_seprint,
	p_mux,
	"%lud",
	p_fields,
	defaultframer
};
