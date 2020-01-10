/*
 * Radio tap as exported by BSD and Linux.
 * The wireless ethernet devices return this format on Linux
 * when running in monitor mode.
 *
 * TODO: Automatically determine whether the ethernet
 * device is radio or ether, so that -h is not needed.
 */

#include <u.h>
#include <libc.h>
#include <ip.h>
#include "dat.h"
#include "protos.h"

static Mux p_mux[] =
{
	{ "802.11", 0 },
	{ 0 }
};

typedef struct Hdr Hdr;
struct Hdr
{
	uchar vers;
	uchar pad;
	ushort	len;
	ulong	present;
};

static int
unpackhdr(uchar *p, uchar *ep, Hdr *h)
{
	if(p+sizeof(Hdr) > ep)
		return -1;
	h->vers = p[0];
	h->pad = p[1];
	h->len = LittleS(p+2);
	h->present = LittleL(p+4);
	// can be more present fields if 0x80000000 is set in each along the chain.
	if(p+h->len > ep)
		return -1;
	return 0;
}

enum
{
	Ot,
};

static Field p_fields[] =
{
	{ 0 }
};

static void
p_compile(Filter *f)
{
	Mux *m;

	if(f->op == '='){
		compile_cmp(radiotap.name, f, p_fields);
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
	sysfatal("unknown radiotap field or protocol: %s", f->s);
}

static int
p_filter(Filter *f, Msg *m)
{
	Hdr h;

	memset(&h, 0, sizeof h);
	if(unpackhdr(m->ps, m->pe, &h) < 0)
		return 0;
	m->ps += h.len;
	switch(f->subop){
	case Ot:
		return 1;
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

	m->p = seprint(m->p, m->e, "%.*H", h.len, m->ps);
	m->ps += h.len;
	m->pr = &p80211;
	return 0;
}

Proto radiotap =
{
	"radiotap",
	p_compile,
	p_filter,
	p_seprint,
	p_mux,
	nil,
	nil,
	defaultframer
};
