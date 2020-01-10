/*
 * LLC.  Only enough to dispatch to SNAP and IP.
 */

#include <u.h>
#include <libc.h>
#include <ip.h>
#include "dat.h"
#include "protos.h"

enum
{
	UFmt = 3,
	Gsap = 1,
	IG = 1,
	SFmt = 1,
	UPoll = 0x10,
	IsPoll = 0x100,
	XidFi = 0x81,

	SapNull = 0,
	SapGlobal = 0xff,
	Sap8021BI = 0x02,
	Sap8021BG = 0x03,
	SapSNA = 0x04,
	SapIP = 0x06,
	SapProwayNM = 0x0e,
	Sap8021D = 0x42,
	SapRS511 = 0x4e,
	SapISO8208 = 0x7e,
	SapProway = 0x8e,
	SapSnap = 0xaa,
	SapIpx = 0xe0,
	SapNetbeui = 0xf0,
	SapIsons = 0xfe,
};

static Mux p_mux[] =
{
// Linux gives llc -> snap not llc -> ip.
// If we don't tell snoopy about llc -> ip, then the default patterns
// like snoopy -h radiotap -f dns work better.
//	{ "ip", SapIP },
	{ "snap", SapSnap },
	{ 0 }
};

typedef struct Hdr Hdr;
struct Hdr
{
	uchar dsap;
	uchar ssap;
	uchar dsapf;
	uchar ssapf;
	ushort ctl;
	uchar isu;
	int hdrlen;
};

static int
unpackhdr(uchar *p, uchar *ep, Hdr *h)
{
	if(p+3 > ep)
		return -1;
	h->dsapf = p[0];
	h->dsap = h->dsapf & ~IG;
	h->ssapf = p[1];
	h->ssap = h->ssapf & ~Gsap;
	h->ctl = p[2];
	h->hdrlen = 3;
	if((h->ctl&UFmt) == UFmt)
		h->isu = 1;
	else{
		if(p+4 > ep)
			return -1;
		h->hdrlen = 4;
		h->ctl = LittleS(p+2);
	}
	return 0;
}

enum
{
	Ossap,
	Odsap,
	Ot,
};

static Field p_fields[] =
{
	{ "ssap",	Fnum,	Ossap,	"ssap" },
	{ "dsap",	Fnum,	Odsap,	"dsap" },
	{ 0 }
};

static void
p_compile(Filter *f)
{
	Mux *m;

	if(f->op == '='){
		compile_cmp(llc.name, f, p_fields);
		return;
	}
	for(m = p_mux; m->name != nil; m++){
		if(strcmp(f->s, m->name) == 0){
			f->pr = m->pr;
			f->ulv = m->val;
			f->subop = Ot;
			return;
		}
	}
	sysfatal("unknown llc field or protocol: %s", f->s);
}

static int
p_filter(Filter *f, Msg *m)
{
	Hdr h;

	memset(&h, 0, sizeof h);
	if(unpackhdr(m->ps, m->pe, &h) < 0)
		return 0;
	m->ps += h.hdrlen;

	switch(f->subop){
	case Ossap:
		return f->ulv == h.ssap;
	case Odsap:
		return f->ulv == h.dsap;
	case Ot:
		return f->ulv == h.ssap && f->ulv == h.dsap;
	}
	return 0;
}

static int
p_seprint(Msg *m)
{
	Hdr h;

	memset(&h, 0, sizeof h);
	if(unpackhdr(m->ps, m->pe, &h) < 0)
		return -1;

	m->pr = &dump;
	m->p = seprint(m->p, m->e, "ssap=%02x dsap=%02x ctl=%04x", h.ssap, h.dsap, h.ctl);
	m->ps += h.hdrlen;
	m->pr = &dump;
	if(h.ssap == h.dsap){
		switch(h.ssap){
		case SapIP:
			m->pr = &ip;
			break;
		case SapSnap:
			m->pr = &snap;
			break;
		}
	}
	return 0;
}

Proto llc =
{
	"llc",
	p_compile,
	p_filter,
	p_seprint,
	p_mux,
	nil,
	nil,
	defaultframer
};
