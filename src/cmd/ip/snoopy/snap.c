/*
 * SNAP.
 */

#include <u.h>
#include <libc.h>
#include <ip.h>
#include "dat.h"
#include "protos.h"

enum
{
	Oorg,
	Oet,

	OuiEther = 0,
	OuiCisco = 0xc,
	OuiCisco90 = 0xf8,
	OuiRfc2684 = 0x80c2,
	OuiAppletalk = 0x80007,
};

static Mux p_mux[] =
{
	{"ip",		0x0800,	} ,
	{"arp",		0x0806,	} ,
	{"rarp",	0x0806,	} ,
	{"ip6", 	0x86dd, } ,
	{"pppoe_disc",	0x8863, },
	{"pppoe_sess",	0x8864, },
	{"eapol",	0x888e, },
	{ 0 }
};

typedef struct Hdr Hdr;
struct Hdr
{
	uchar org[3];
	uchar et[2];
};

static Field p_fields[] =
{
	{ "org",	Fnum,	Oorg,	"org" },
	{ "et",	Fnum,	Oet,	"et" },
	{ 0 }
};

static void
p_compile(Filter *f)
{
	Mux *m;

	if(f->op == '='){
		compile_cmp(snap.name, f, p_fields);
		return;
	}
	for(m = p_mux; m->name != nil; m++){
		if(strcmp(f->s, m->name) == 0){
			f->pr = m->pr;
			f->ulv = m->val;
			f->subop = Oet;
			return;
		}
	}
	sysfatal("unknown snap field or protocol: %s", f->s);
}

static int
p_filter(Filter *f, Msg *m)
{
	Hdr *h;

	if(m->pe - m->ps < sizeof(Hdr))
		return 0;
	h = (Hdr*)m->ps;
	m->ps += 5;
	switch(f->subop){
	case Oorg:
		return f->ulv == Net3(h->org);
	case Oet:
		return f->ulv == NetS(h->et);
	}
	return 0;
}

static int
p_seprint(Msg *m)
{
	Hdr *h;

	if(m->pe - m->ps < sizeof(Hdr))
		return 0;
	h = (Hdr*)m->ps;
	m->ps += 5;
	demux(p_mux, NetS(h->et), NetS(h->et), m, &dump);

	m->p = seprint(m->p, m->e, "org=%06x et=%04x", Net3(h->org), NetS(h->et));
	return 0;
}

Proto snap =
{
	"snap",
	p_compile,
	p_filter,
	p_seprint,
	p_mux,
	nil,
	nil,
	defaultframer
};
