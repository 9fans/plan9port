#include <u.h>
#include <libc.h>
#include <ip.h>
#include "dat.h"
#include "protos.h"

typedef struct Hdr	Hdr;
struct Hdr
{
	uchar	vcf[4];			/* Version and header length */
	uchar	length[2];		/* packet length */
	uchar	proto;			/* Protocol */
	uchar	ttl;			/* Time to live */
	uchar	src[IPaddrlen];		/* IP source */
	uchar	dst[IPaddrlen];		/* IP destination */
};

enum
{
	IP6HDR		= 40,		/* sizeof(Iphdr) */
	IP_VER		= 0x60,		/* Using IP version 4 */
	HBH_HDR		= 0,
	ROUT_HDR	= 43,
	FRAG_HDR	= 44,
	FRAG_HSZ	= 8, 		/* in bytes */
	DEST_HDR	= 60
};

static Mux p_mux[] =
{
	{ "igmp", 2, },
	{ "ggp", 3, },
	{ "ip", 4, },
	{ "st", 5, },
	{ "tcp", 6, },
	{ "ucl", 7, },
	{ "egp", 8, },
	{ "igp", 9, },
	{ "bbn-rcc-mon", 10, },
	{ "nvp-ii", 11, },
	{ "pup", 12, },
	{ "argus", 13, },
	{ "emcon", 14, },
	{ "xnet", 15, },
	{ "chaos", 16, },
	{ "udp", 17, },
	{ "mux", 18, },
	{ "dcn-meas", 19, },
	{ "hmp", 20, },
	{ "prm", 21, },
	{ "xns-idp", 22, },
	{ "trunk-1", 23, },
	{ "trunk-2", 24, },
	{ "leaf-1", 25, },
	{ "leaf-2", 26, },
	{ "rdp", 27, },
	{ "irtp", 28, },
	{ "iso-tp4", 29, },
	{ "netblt", 30, },
	{ "mfe-nsp", 31, },
	{ "merit-inp", 32, },
	{ "sep", 33, },
	{ "3pc", 34, },
	{ "idpr", 35, },
	{ "xtp", 36, },
	{ "ddp", 37, },
	{ "idpr-cmtp", 38, },
	{ "tp++", 39, },
	{ "il", 40, },
	{ "sip", 41, },
	{ "sdrp", 42, },
	{ "idrp", 45, },
	{ "rsvp", 46, },
	{ "gre", 47, },
	{ "mhrp", 48, },
	{ "bna", 49, },
	{ "sipp-esp", 50, },
	{ "sipp-ah", 51, },
	{ "i-nlsp", 52, },
	{ "swipe", 53, },
	{ "nhrp", 54, },
	{ "icmp6", 58, },
	{ "any", 61, },
	{ "cftp", 62, },
	{ "any", 63, },
	{ "sat-expak", 64, },
	{ "kryptolan", 65, },
	{ "rvd", 66, },
	{ "ippc", 67, },
	{ "any", 68, },
	{ "sat-mon", 69, },
	{ "visa", 70, },
	{ "ipcv", 71, },
	{ "cpnx", 72, },
	{ "cphb", 73, },
	{ "wsn", 74, },
	{ "pvp", 75, },
	{ "br-sat-mon", 76, },
	{ "sun-nd", 77, },
	{ "wb-mon", 78, },
	{ "wb-expak", 79, },
	{ "iso-ip", 80, },
	{ "vmtp", 81, },
	{ "secure-vmtp", 82, },
	{ "vines", 83, },
	{ "ttp", 84, },
	{ "nsfnet-igp", 85, },
	{ "dgp", 86, },
	{ "tcf", 87, },
	{ "igrp", 88, },
	{ "ospf", 89, },
	{ "sprite-rpc", 90, },
	{ "larp", 91, },
	{ "mtp", 92, },
	{ "ax.25", 93, },
	{ "ipip", 94, },
	{ "micp", 95, },
	{ "scc-sp", 96, },
	{ "etherip", 97, },
	{ "encap", 98, },
	{ "any", 99, },
	{ "gmtp", 100, },
	{ "rudp", 254, },
	{ 0 }
};

enum
{
	Os,	/* source */
	Od,	/* destination */
	Osd,	/* source or destination */
	Ot,	/* type */
};

static Field p_fields[] =
{
	{"s",	Fv6ip,	Os,	"source address",	} ,
	{"d",	Fv6ip,	Od,	"destination address",	} ,
	{"a",	Fv6ip,	Osd,	"source|destination address",} ,
	{"t",	Fnum,	Ot,	"sub protocol number",	} ,
	{0}
};

static void
p_compile(Filter *f)
{
	Mux *m;

	if(f->op == '='){
		compile_cmp(ip6.name, f, p_fields);
		return;
	}
	for(m = p_mux; m->name != nil; m++)
		if(strcmp(f->s, m->name) == 0){
			f->pr = m->pr;
			f->ulv = m->val;
			f->subop = Ot;
			return;
		}
	sysfatal("unknown ip6 field or protocol: %s", f->s);
}

static int
v6hdrlen(Hdr *h)
{
	int plen, len = IP6HDR;
	int pktlen = IP6HDR + NetS(h->length);
	uchar nexthdr = h->proto;
	uchar *pkt = (uchar*) h;

	pkt += len;
	plen = len;

	while ( (nexthdr == HBH_HDR) || (nexthdr == ROUT_HDR) ||
		(nexthdr == FRAG_HDR) || (nexthdr == DEST_HDR) ) {

		if (nexthdr == FRAG_HDR)
			len = FRAG_HSZ;
		else
			len = ( ((int) *(pkt+1)) + 1) * 8;

		if (plen + len > pktlen)
			return -1;

		pkt += len;
		nexthdr = *pkt;
		plen += len;
	}
	return plen;
}

static int
p_filter(Filter *f, Msg *m)
{
	Hdr *h;
	int hlen;

	if(m->pe - m->ps < IP6HDR)
		return 0;

	h = (Hdr*)m->ps;

	if ((hlen = v6hdrlen(h)) < 0)
		return 0;
	else
		m->ps += hlen;
	switch(f->subop){
	case Os:
		return !memcmp(h->src, f->a, IPaddrlen);
	case Od:
		return !memcmp(h->dst, f->a, IPaddrlen);
	case Osd:
		return !memcmp(h->src, f->a, IPaddrlen) || !memcmp(h->dst, f->a, IPaddrlen);
	case Ot:
		return h->proto == f->ulv;
	}
	return 0;
}

static int
v6hdr_seprint(Msg *m)
{
	int len = IP6HDR;
	uchar *pkt = m->ps;
	Hdr *h = (Hdr *) pkt;
	int pktlen = IP6HDR + NetS(h->length);
	uchar nexthdr = h->proto;
	int plen;

	pkt += len;
	plen = len;

	while ( (nexthdr == HBH_HDR) || (nexthdr == ROUT_HDR) ||
		(nexthdr == FRAG_HDR) || (nexthdr == DEST_HDR) ) {

		switch (nexthdr) {
		case FRAG_HDR:
			m->p = seprint(m->p, m->e, "\n	  xthdr=frag id=%d offset=%d pr=%d more=%d res1=%d res2=%d",
					NetL(pkt+4),
					NetS(pkt+2) & ~7,
					(int) (*pkt),
					(int) (*(pkt+3) & 0x1),
					(int) *(pkt+1),
					(int) (*(pkt+3) & 0x6)
				);
			len = FRAG_HSZ;
			break;

		case HBH_HDR:
		case ROUT_HDR:
		case DEST_HDR:
			len = ( ((int) *(pkt+1)) + 1) * 8;
			break;
		}

		if (plen + len > pktlen) {
			m->p = seprint(m->p, m->e, "bad pkt");
			m->pr = &dump;
			return -1;
		}
		plen += len;
		pkt += len;
		nexthdr = *pkt;
	}

	m->ps = pkt;
	return 1;

}

static int
p_seprint(Msg *m)
{
	Hdr *h;
	int len;

	if(m->pe - m->ps < IP6HDR)
		return -1;
	h = (Hdr*)m->ps;

	demux(p_mux, h->proto, h->proto, m, &dump);

	/* truncate the message if there's extra */
	len = NetS(h->length) + IP6HDR;
	if(len < m->pe - m->ps)
		m->pe = m->ps + len;

	m->p = seprint(m->p, m->e, "s=%I d=%I ttl=%3d pr=%d ln=%d",
			h->src, h->dst,
			h->ttl,
			h->proto,
			NetS(h->length)
			);

	v6hdr_seprint(m);

	return 0;
}

Proto ip6 =
{
	"ip6",
	p_compile,
	p_filter,
	p_seprint,
	p_mux,
	"%lud",
	p_fields,
	defaultframer
};
