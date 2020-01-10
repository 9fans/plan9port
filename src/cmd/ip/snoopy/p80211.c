/*
 * IEEE 802.11.
 */

#include <u.h>
#include <libc.h>
#include <ip.h>
#include "dat.h"
#include "protos.h"

enum
{
	Tmgmt = 0,
	Tctl,
	Tdata,

	CtlPoll = 0xA,
	CtlRts,
	CtlCts,
	CtlAck,
	CtlCfEnd,
	CtlCfEndAck,

	Data = 0,
	DataCfAck,
	DataCfPoll,
	DataCfAckPoll,
	Nodata,
	NodataCfAck,
	NodataCfPoll,
	NodataCfAckPoll,

	FlagTods = 0x1,
	FlagFromds = 0x2,
	FlagMoreflag = 0x4,
	FlagRetry = 0x8,
	FlagPowerMgmt = 0x10,
	FlagMoreData = 0x20,
	FlagWep = 0x40,
	FlagOrder = 0x80,

	ProtoNone = 0,
	ProtoLlc,
};

static Mux p_mux[] =
{
	{ "llc", ProtoLlc },
	{ 0 }
};

typedef struct Hdr Hdr;
struct Hdr
{
	uchar vers;
	uchar type;
	uchar subtype;
	uchar flags;
	ushort dur;
	uchar aid;
	uchar ra[6];
	uchar ta[6];
	uchar bssid[6];
	uchar sa[6];
	uchar da[6];
	ushort seq;
	int proto;
	int hdrlen;
};

static int
unpackhdr(uchar *p, uchar *ep, Hdr *h)
{
	if(p+2 > ep)
		return -1;
	h->vers = p[0]&3;
	if(h->vers != 0){
		h->hdrlen = 2;
		return -1;
	}
	h->type = (p[0]>>2)&3;
	h->subtype = (p[0]>>4)&15;
	h->flags = p[1];
	h->hdrlen = 2;

	if(h->vers != 0)
		return 0;

	switch(h->type){
	case Tmgmt:
		// fc dur da sa bssid seq
		if(p+2+2+6+6+6+2 > ep)
			return -1;
		h->hdrlen = 24;
		h->dur = LittleS(p+2);
		memmove(h->da, p+4, 6);
		memmove(h->sa, p+10, 6);
		memmove(h->bssid, p+16, 6);
		h->seq = LittleS(p+22);
		break;

	case Tctl:
		switch(h->subtype){
		case CtlPoll:
			// fc aid bssid ta
			if(p+2+2+6+6 > ep)
				return -1;
			h->hdrlen = 16;
			h->aid = LittleS(p+2);
			memmove(h->bssid, p+4, 6);
			memmove(h->ta, p+10, 6);
			break;

		case CtlRts:
			// fc dur ra ta
			if(p+2+2+6+6 > ep)
				return -1;
			h->hdrlen = 16;
			h->dur = LittleS(p+2);
			memmove(h->ra, p+4, 6);
			memmove(h->ta, p+10, 6);
			break;

		case CtlCts:
		case CtlAck:
			// fc dur ra
			if(p+2+2+6 > ep)
				return -1;
			h->hdrlen = 10;
			h->dur = LittleS(p+2);
			memmove(h->ra, p+4, 6);
			break;

		case CtlCfEnd:
		case CtlCfEndAck:
			// fc dur ra bssid
			if(p+2+2+6+6 > ep)
				return -1;
			h->hdrlen = 16;
			h->dur = LittleS(p+2);
			memmove(h->ra, p+4, 6);
			memmove(h->bssid, p+10, 6);
			break;
		}
		break;

	case Tdata:
		if(p+24 > ep)
			return -1;
		h->hdrlen = 24;
		h->dur = LittleS(p+2);	// ??? maybe
		// Also, what is at p+22?

		switch(h->flags&(FlagFromds|FlagTods)){
		case 0:
			memmove(h->da, p+4, 6);
			memmove(h->sa, p+10, 6);
			memmove(h->bssid, p+16, 6);
			break;
		case FlagFromds:
			memmove(h->da, p+4, 6);
			memmove(h->bssid, p+10, 6);
			memmove(h->sa, p+16, 6);
			break;
		case FlagTods:
			memmove(h->bssid, p+4, 6);
			memmove(h->sa, p+10, 6);
			memmove(h->da, p+16, 6);
			break;
		case FlagFromds|FlagTods:
			if(p+30 > ep)
				return -1;
			h->hdrlen = 30;
			memmove(h->ra, p+4, 6);
			memmove(h->ta, p+10, 6);
			memmove(h->da, p+16, 6);
			memmove(h->sa, p+24, 6);	// 24 sic
			break;
		}
		p += h->hdrlen;
		h->proto = ProtoNone;
		if(!(h->flags&FlagWep))
			h->proto = ProtoLlc;
		break;
	}
	return 0;
}

enum
{
	Os,
	Od,
	Ot,
	Or,
	Obssid,
	Oa,
	Opr,
};

static Field p_fields[] =
{
	{ "s",	Fether,	Os,	"source address" },
	{ "d",	Fether,	Od,	"destination address" },
	{ "t",	Fether,	Ot,	"transmit address" },
	{ "r",	Fether,	Or,	"receive address" },
	{ "bssid", Fether,	Obssid, "bssid address" },
	{ "a",	Fether,	Oa,	"any address" },
	{ "sd",	Fether,	Oa,	"source|destination address" },
	{ 0 }
};

static void
p_compile(Filter *f)
{
	Mux *m;

	if(f->op == '='){
		compile_cmp(p80211.name, f, p_fields);
		return;
	}
	if(strcmp(f->s, "mgmt") == 0){
		f->pr = &p80211;
		f->ulv = Tmgmt;
		f->subop = Ot;
		return;
	}
	if(strcmp(f->s, "ctl") == 0){
		f->pr = &p80211;
		f->ulv = Tctl;
		f->subop = Ot;
		return;
	}
	if(strcmp(f->s, "data") == 0){
		f->pr = &p80211;
		f->ulv = Tdata;
		f->subop = Ot;
		return;
	}
	for(m = p_mux; m->name != nil; m++){
		if(strcmp(f->s, m->name) == 0){
			f->pr = m->pr;
			f->ulv = m->val;
			f->subop = Opr;
			return;
		}
	}
	sysfatal("unknown 802.11 field or protocol: %s", f->s);
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
	case Os:
		return memcmp(h.sa, f->a, 6) == 0;
	case Od:
		return memcmp(h.da, f->a, 6) == 0;
	case Ot:
		return memcmp(h.ta, f->a, 6) == 0;
	case Or:
		return memcmp(h.ra, f->a, 6) == 0;
	case Obssid:
		return memcmp(h.bssid, f->a, 6) == 0;
	case Oa:
		return memcmp(h.sa, f->a, 6) == 0
			|| memcmp(h.da, f->a, 6) == 0
			|| memcmp(h.ta, f->a, 6) == 0
			|| memcmp(h.ra, f->a, 6) == 0
			|| memcmp(h.bssid, f->a, 6) == 0;
	case Opr:
		return h.proto == f->ulv;
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
	m->p = seprint(m->p, m->e, "fc=%02x flags=%02x ", m->ps[0], m->ps[1]);
	switch(h.type){
	case Tmgmt:
		m->p = seprint(m->p, m->e, "mgmt dur=%d d=%E s=%E bssid=%E seq=%d",
			h.dur, h.da, h.sa, h.bssid, h.seq);
		break;
	case Tctl:
		switch(h.subtype){
		case CtlPoll:
			m->p = seprint(m->p, m->e, "ctl poll aid=%d bssid=%E t=%E",
				h.aid, h.bssid, h.ta);
			break;
		case CtlRts:
			m->p = seprint(m->p, m->e, "ctl rts dur=%d r=%E t=%E",
				h.dur, h.ra, h.ta);
			break;
		case CtlCts:
			m->p = seprint(m->p, m->e, "ctl cts dur=%d r=%E",
				h.dur, h.ra);
			break;
		case CtlAck:
			m->p = seprint(m->p, m->e, "ctl ack dur=%d r=%E",
				h.dur, h.ra);
			break;
		case CtlCfEnd:
			m->p = seprint(m->p, m->e, "ctl cf end dur=%d r=%E bssid=%E",
				h.dur, h.ra, h.bssid);
			break;
		case CtlCfEndAck:
			m->p = seprint(m->p, m->e, "ctl cf end ack dur=%d r=%E bssid=%E",
				h.dur, h.ra, h.bssid);
			break;
		default:
			m->p = seprint(m->p, m->e, "ctl %.*H", m->ps, h.hdrlen);
			break;
		}
		break;
	case Tdata:
		switch(h.flags&(FlagFromds|FlagTods)){
		case 0:
			m->p = seprint(m->p, m->e, "data d=%E s=%E bssid=%E",
				h.da, h.sa, h.bssid);
			break;
		case FlagFromds:
			m->p = seprint(m->p, m->e, "data fds d=%E bssid=%E s=%E",
				h.da, h.bssid, h.sa);
			break;
		case FlagTods:
			m->p = seprint(m->p, m->e, "data tds bssid=%E s=%E d=%E",
				h.bssid, h.sa, h.da);
			break;
		case FlagFromds|FlagTods:
			m->p = seprint(m->p, m->e, "data fds tds r=%E t=%E d=%E s=%E",
				h.ra, h.ta, h.da, h.sa);
			break;
		}
		if(!(h.flags&FlagWep))
			m->pr = &llc;
		break;
	}
	m->ps += h.hdrlen;
	return 0;
}

Proto p80211 =
{
	"802.11",
	p_compile,
	p_filter,
	p_seprint,
	p_mux,
	nil,
	nil,
	defaultframer
};
