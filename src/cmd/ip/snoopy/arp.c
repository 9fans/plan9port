#include <u.h>
#include <libc.h>
#include <ip.h>
#include "dat.h"
#include "protos.h"

typedef struct Hdr	Hdr;
struct Hdr
{
	uchar	hrd[2];
	uchar	pro[2];
	uchar	hln;
	uchar	pln;
	uchar	op[2];
	uchar	sha[6];
	uchar	spa[4];
	uchar	tha[6];
	uchar	tpa[4];
};

enum
{
	ARPLEN=	28
};

enum
{
	Ospa,
	Otpa,
	Ostpa,
	Osha,
	Otha,
	Ostha,
	Opa
};

static Field p_fields[] =
{
	{"spa",		Fv4ip,	Ospa,	"protocol source",	} ,
	{"tpa",		Fv4ip,	Otpa,	"protocol target",	} ,
	{"a",		Fv4ip,	Ostpa,	"protocol source/target",	} ,
	{"sha",		Fba,	Osha,	"hardware source",	} ,
	{"tha",		Fba,	Otha,	"hardware target",	} ,
	{"ah",	 	Fba,	Ostha,	"hardware source/target",	} ,
	{0}
};

static void
p_compile(Filter *f)
{
	if(f->op == '='){
		compile_cmp(arp.name, f, p_fields);
		return;
	}
	sysfatal("unknown arp field: %s", f->s);
}

static int
p_filter(Filter *f, Msg *m)
{
	Hdr *h;

	if(m->pe - m->ps < ARPLEN)
		return 0;

	h = (Hdr*)m->ps;
	m->ps += ARPLEN;

	switch(f->subop){
	case Ospa:
		return h->pln == 4 && NetL(h->spa) == f->ulv;
	case Otpa:
		return h->pln == 4 && NetL(h->tpa) == f->ulv;
	case Ostpa:
		return h->pln == 4 && (NetL(h->tpa) == f->ulv ||
			NetL(h->spa) == f->ulv);
	case Osha:
		return memcmp(h->sha, f->a, h->hln) == 0;
	case Otha:
		return memcmp(h->tha, f->a, h->hln) == 0;
	case Ostha:
		return memcmp(h->sha, f->a, h->hln)==0
			||memcmp(h->tha, f->a, h->hln)==0;
	}
	return 0;
}

static int
p_seprint(Msg *m)
{
	Hdr *h;

	if(m->pe - m->ps < ARPLEN)
		return -1;

	h = (Hdr*)m->ps;
	m->ps += ARPLEN;

	/* no next protocol */
	m->pr = nil;

	m->p = seprint(m->p, m->e, "op=%1d len=%1d/%1d spa=%V sha=%E tpa=%V tha=%E",
			NetS(h->op), h->pln, h->hln,
			h->spa, h->sha, h->tpa, h->tha);
	return 0;
}

Proto arp =
{
	"arp",
	p_compile,
	p_filter,
	p_seprint,
	nil,
	nil,
	p_fields,
	defaultframer
};

Proto rarp =
{
	"rarp",
	p_compile,
	p_filter,
	p_seprint,
	nil,
	nil,
	p_fields,
	defaultframer
};
