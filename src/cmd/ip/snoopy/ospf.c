#include <u.h>
#include <libc.h>
#include <ip.h>
#include <libsec.h>
#include "dat.h"
#include "protos.h"


/*
 *  OSPF packets
 */
typedef struct Ospfpkt	Ospfpkt;
struct Ospfpkt
{
	uchar	version;
	uchar	type;
	uchar	length[2];
	uchar	router[4];
	uchar	area[4];
	uchar	sum[2];
	uchar	autype[2];
	uchar	auth[8];
	uchar	data[1];
};
#define OSPF_HDRSIZE	24

enum
{
	OSPFhello=	1,
	OSPFdd=		2,
	OSPFlsrequest=	3,
	OSPFlsupdate=	4,
	OSPFlsack=	5
};


char *ospftype[] = {
	[OSPFhello]	"hello",
	[OSPFdd]	"data definition",
	[OSPFlsrequest]	"link state request",
	[OSPFlsupdate]	"link state update",
	[OSPFlsack]	"link state ack"
};

char*
ospfpkttype(int x)
{
	static char type[16];

	if(x > 0 && x <= OSPFlsack)
		return ospftype[x];
	sprint(type, "type %d", x);
	return type;
}

char*
ospfauth(Ospfpkt *ospf)
{
	static char auth[100];

	switch(ospf->type){
	case 0:
		return "no authentication";
	case 1:
		sprint(auth, "password(%8.8ux %8.8ux)", NetL(ospf->auth),
			NetL(ospf->auth+4));
		break;
	case 2:
		sprint(auth, "crypto(plen %d id %d dlen %d)", NetS(ospf->auth),
			ospf->auth[2], ospf->auth[3]);
		break;
	default:
		sprint(auth, "auth%d(%8.8ux %8.8ux)", NetS(ospf->autype), NetL(ospf->auth),
			NetL(ospf->auth+4));
	}
	return auth;
}

typedef struct Ospfhello	Ospfhello;
struct Ospfhello
{
	uchar	mask[4];
	uchar	interval[2];
	uchar	options;
	uchar	pri;
	uchar	deadint[4];
	uchar	designated[4];
	uchar	bdesignated[4];
	uchar	neighbor[1];
};

char*
seprintospfhello(char *p, char *e, void *a)
{
	Ospfhello *h = a;

	return seprint(p, e, "%s(mask %V interval %d opt %ux pri %ux deadt %d designated %V bdesignated %V)",
		ospftype[OSPFhello],
		h->mask, NetS(h->interval), h->options, h->pri,
		NetL(h->deadint), h->designated, h->bdesignated);
}

enum
{
	LSARouter=	1,
	LSANetwork=	2,
	LSASummN=	3,
	LSASummR=	4,
	LSAASext=	5
};


char *lsatype[] = {
	[LSARouter]	"Router LSA",
	[LSANetwork]	"Network LSA",
	[LSASummN]	"Summary LSA (Network)",
	[LSASummR]	"Summary LSA (Router)",
	[LSAASext]	"LSA AS external"
};

char*
lsapkttype(int x)
{
	static char type[16];

	if(x > 0 && x <= LSAASext)
		return lsatype[x];
	sprint(type, "type %d", x);
	return type;
}

/* OSPF Link State Advertisement Header */
/* rfc2178 section 12.1 */
/* data of Ospfpkt point to a 4-uchar value that is the # of LSAs */
struct OspfLSAhdr {
	uchar	lsage[2];
	uchar	options;	/* 0x2=stub area, 0x1=TOS routing capable */

	uchar	lstype;	/* 1=Router-LSAs
						 * 2=Network-LSAs
						 * 3=Summary-LSAs (to network)
						 * 4=Summary-LSAs (to AS boundary routers)
						 * 5=AS-External-LSAs
						 */
	uchar	lsid[4];
	uchar	advtrt[4];

	uchar	lsseqno[4];
	uchar	lscksum[2];
	uchar	lsalen[2];	/* includes the 20 byte lsa header */
};

struct Ospfrt {
	uchar	linkid[4];
	uchar	linkdata[4];
	uchar	typ;
	uchar	numtos;
	uchar	metric[2];

};

struct OspfrtLSA {
	struct OspfLSAhdr	hdr;
	uchar			netmask[4];
};

struct OspfntLSA {
	struct OspfLSAhdr	hdr;
	uchar			netmask[4];
	uchar			attrt[4];
};

/* Summary Link State Advertisement info */
struct Ospfsumm {
	uchar	flag;	/* always zero */
	uchar	metric[3];
};

struct OspfsummLSA {
	struct OspfLSAhdr	hdr;
	uchar			netmask[4];
	struct Ospfsumm		lsa;
};

/* AS external Link State Advertisement info */
struct OspfASext {
	uchar	flag;	/* external */
	uchar	metric[3];
	uchar	fwdaddr[4];
	uchar	exrttag[4];
};

struct OspfASextLSA {
	struct OspfLSAhdr	hdr;
	uchar			netmask[4];
	struct OspfASext	lsa;
};

/* OSPF Link State Update Packet */
struct OspfLSupdpkt {
	uchar	lsacnt[4];
	union {
		uchar			hdr[1];
		struct OspfrtLSA	rt[1];
		struct OspfntLSA	nt[1];
		struct OspfsummLSA	sum[1];
		struct OspfASextLSA	as[1];
	};
};

char*
seprintospflsaheader(char *p, char *e, struct OspfLSAhdr *h)
{
	return seprint(p, e, "age %d opt %ux type %ux lsid %V adv_rt %V seqno %ux c %4.4ux l %d",
		NetS(h->lsage), h->options&0xff, h->lstype,
		h->lsid, h->advtrt, NetL(h->lsseqno), NetS(h->lscksum),
		NetS(h->lsalen));
}

/* OSPF Database Description Packet */
struct OspfDDpkt {
	uchar	intMTU[2];
	uchar	options;
	uchar	bits;
	uchar	DDseqno[4];
	struct OspfLSAhdr	hdr[1];		/* LSA headers... */
};

char*
seprintospfdatadesc(char *p, char *e, void *a, int len)
{
	int nlsa, i;
	struct OspfDDpkt *g;

	g = (struct OspfDDpkt *)a;
	nlsa = len/sizeof(struct OspfLSAhdr);
	for (i=0; i<nlsa; i++) {
		p = seprint(p, e, "lsa%d(", i);
		p = seprintospflsaheader(p, e, &(g->hdr[i]));
		p = seprint(p, e, ")");
	}
	return seprint(p, e, ")");
}

char*
seprintospflsupdate(char *p, char *e, void *a, int len)
{
	int nlsa, i;
	struct OspfLSupdpkt *g;
	struct OspfLSAhdr *h;

	g = (struct OspfLSupdpkt *)a;
	nlsa = NetL(g->lsacnt);
	h = (struct OspfLSAhdr *)(g->hdr);
	p = seprint(p, e, "%d-%s(", nlsa, ospfpkttype(OSPFlsupdate));

	switch(h->lstype) {
	case LSARouter:
		{
/*			struct OspfrtLSA *h;
 */
		}
		break;
	case LSANetwork:
		{
			struct OspfntLSA *h;

			for (i=0; i<nlsa; i++) {
				h = &(g->nt[i]);
				p = seprint(p, e, "lsa%d(", i);
				p = seprintospflsaheader(p, e, &(h->hdr));
				p = seprint(p, e, " mask %V attrt %V)",
					h->netmask, h->attrt);
			}
		}
		break;
	case LSASummN:
	case LSASummR:
		{
			struct OspfsummLSA *h;

			for (i=0; i<nlsa; i++) {
				h = &(g->sum[i]);
				p = seprint(p, e, "lsa%d(", i);
				p = seprintospflsaheader(p, e, &(h->hdr));
				p = seprint(p, e, " mask %V met %d)",
					h->netmask, Net3(h->lsa.metric));
			}
		}
		break;
	case LSAASext:
		{
			struct OspfASextLSA *h;

			for (i=0; i<nlsa; i++) {
				h = &(g->as[i]);
				p = seprint(p, e, " lsa%d(", i);
				p = seprintospflsaheader(p, e, &(h->hdr));
				p = seprint(p, e, " mask %V extflg %1.1ux met %d fwdaddr %V extrtflg %ux)",
					h->netmask, h->lsa.flag, Net3(h->lsa.metric),
					h->lsa.fwdaddr, NetL(h->lsa.exrttag));
			}
		}
		break;
	default:
		p = seprint(p, e, "Not an LS update, lstype %d ", h->lstype);
		p = seprint(p, e, " %.*H", len>64?64:len, a);
		break;
	}
	return seprint(p, e, ")");
}

char*
seprintospflsack(char *p, char *e, void *a, int len)
{
	int nlsa, i;
	struct OspfLSAhdr *h;

	h = (struct OspfLSAhdr *)a;
	nlsa = len/sizeof(struct OspfLSAhdr);
	p = seprint(p, e, "%d-%s(", nlsa, ospfpkttype(OSPFlsack));
	for (i=0; i<nlsa; i++) {
		p = seprint(p, e, " lsa%d(", i);
		p = seprintospflsaheader(p, e, &(h[i]));
		p = seprint(p, e, ")");
	}
	return seprint(p, e, ")");
}

int
p_seprint(Msg *m)
{
	Ospfpkt *ospf;
	int len, x;
	char *p, *e;

	len = m->pe - m->ps;
	if(len < OSPF_HDRSIZE)
		return -1;
	p = m->p;
	e = m->e;

	/* adjust packet size */
	ospf = (Ospfpkt*)m->ps;
	x = NetS(ospf->length);
	if(x < len)
		return -1;
	x -= OSPF_HDRSIZE;

	p = seprint(p, e, "ver=%d type=%d len=%d r=%V a=%V c=%4.4ux %s ",
		ospf->version, ospf->type, x,
		ospf->router, ospf->area, NetS(ospf->sum),
		ospfauth(ospf));

	switch (ospf->type) {
	case OSPFhello:
		p = seprintospfhello(p, e, ospf->data);
		break;
	case OSPFdd:
		p = seprintospfdatadesc(p, e, ospf->data, x);
		break;
	case OSPFlsrequest:
		p = seprint(p, e, " %s->", ospfpkttype(ospf->type));
		goto Default;
	case OSPFlsupdate:
		p = seprintospflsupdate(p, e, ospf->data, x);
		break;
	case OSPFlsack:
		p = seprintospflsack(p, e, ospf->data, x);
		break;
	default:
Default:
		p = seprint(p, e, " data=%.*H", x>64?64:x, ospf->data);
	}
	m->p = p;
	m->pr = nil;
	return 0;
}

Proto ospf =
{
	"ospf",
	nil,
	nil,
	p_seprint,
	nil,
	nil,
	nil,
	defaultframer
};
