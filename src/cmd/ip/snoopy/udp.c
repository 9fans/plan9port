#include <u.h>
#include <libc.h>
#include <ip.h>
#include "dat.h"
#include "protos.h"

typedef struct Hdr	Hdr;
struct Hdr
{
	uchar	sport[2];	/* Source port */
	uchar	dport[2];	/* Destination port */
	uchar	len[2];		/* data length */
	uchar	cksum[2];	/* Checksum */
};

enum
{
	UDPLEN=	8
};

enum
{
	Os,
	Od,
	Osd,
	Osetport
};

static Field p_fields[] =
{
	{"s",		Fnum,	Os,	"source port",	} ,
	{"d",		Fnum,	Od,	"dest port",	} ,
	{"a",		Fnum,	Osd,	"source/dest port",	} ,
	{"sd",		Fnum,	Osd,	"source/dest port",	} ,
	{0}
};

#define ANYPORT ~0UL

static Mux p_mux[] =
{
	{"bootp",	67, },
	{"ninep",	6346, },	/* tvs */
	{"dns", 53 },
	{"rtp",		ANYPORT, },
	{"rtcp",	ANYPORT, },
	{0}
};

/* default next protocol, can be changed by p_filter, reset by p_compile */
static Proto	*defproto = &dump;

static void
p_compile(Filter *f)
{
	Mux *m;

	if(f->op == '='){
		compile_cmp(udp.name, f, p_fields);
		return;
	}
	for(m = p_mux; m->name != nil; m++)
		if(strcmp(f->s, m->name) == 0){
			f->pr = m->pr;
			f->ulv = m->val;
			f->subop = Osd;
			return;
		}

	sysfatal("unknown udp field or protocol: %s", f->s);
}

static int
p_filter(Filter *f, Msg *m)
{
	Hdr *h;

	if(m->pe - m->ps < UDPLEN)
		return 0;

	h = (Hdr*)m->ps;
	m->ps += UDPLEN;

	switch(f->subop){
	case Os:
		return NetS(h->sport) == f->ulv;
	case Od:
		return NetS(h->dport) == f->ulv;
	case Osd:
		if(f->ulv == ANYPORT){
			defproto = f->pr;
			return 1;
		}
		return NetS(h->sport) == f->ulv || NetS(h->dport) == f->ulv;
	}
	return 0;
}

static int
p_seprint(Msg *m)
{
	Hdr *h;
	int dport, sport;


	if(m->pe - m->ps < UDPLEN)
		return -1;
	h = (Hdr*)m->ps;
	m->ps += UDPLEN;

	/* next protocol */
	sport = NetS(h->sport);
	dport = NetS(h->dport);
	demux(p_mux, sport, dport, m, defproto);
	defproto = &dump;

	m->p = seprint(m->p, m->e, "s=%d d=%d ck=%4.4ux ln=%4d",
			NetS(h->sport), dport,
			NetS(h->cksum), NetS(h->len));
	return 0;
}

Proto udp =
{
	"udp",
	p_compile,
	p_filter,
	p_seprint,
	p_mux,
	"%lud",
	p_fields,
	defaultframer
};
