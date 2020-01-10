#include <u.h>
#include <libc.h>
#include <ip.h>
#include "dat.h"
#include "protos.h"

typedef struct Hdr Hdr;
struct Hdr {
	uchar	hdr;			/* RTCP header */
	uchar	pt;			/* Packet type */
	uchar	len[2];		/* Report length */
	uchar	ssrc[4];		/* Synchronization source identifier */
	uchar	ntp[8];		/* NTP time stamp */
	uchar	rtp[4];		/* RTP time stamp */
	uchar	pktc[4];		/* Sender's packet count */
	uchar	octc[4];		/* Sender's octect count */
};

typedef struct Report Report;
struct Report {
	uchar	ssrc[4];		/* SSRC identifier */
	uchar	lost[4];		/* Fraction + cumu lost */
	uchar	seqhi[4];		/* Highest seq number received */
	uchar	jitter[4];		/* Interarrival jitter */
	uchar	lsr[4];		/* Last SR */
	uchar	dlsr[4];		/* Delay since last SR */
};

enum{
	RTCPLEN = 28,		/* Minimum size of an RTCP header */
	REPORTLEN = 24
};

static int
p_seprint(Msg *m)
{
	Hdr*h;
	Report*r;
	int rc, i, frac;
	float dlsr;

	if(m->pe - m->ps < RTCPLEN)
		return -1;

	h = (Hdr*)m->ps;
	if(m->pe - m->ps < (NetS(h->len) + 1) * 4)
		return -1;

	rc = h->hdr & 0x1f;
	m->ps += RTCPLEN;
	m->p = seprint(m->p, m->e, "version=%d rc=%d tp=%d ssrc=%8ux ntp=%d.%.10ud rtp=%d pktc=%d octc=%d hlen=%d",
				(h->hdr >> 6) & 3, rc, h->pt, NetL(h->ssrc),
				NetL(h->ntp), (uint)NetL(&h->ntp[4]), NetL(h->rtp),
				NetL(h->pktc), NetL(h->octc),
				(NetS(h->len) + 1) * 4);

	for(i = 0; i < rc; i++){
		r = (Report*)m->ps;
		m->ps += REPORTLEN;

		frac = (int)(((float)r->lost[0] * 100.) / 256.);
		r->lost[0] = 0;
		dlsr = (float)NetL(r->dlsr) / 65536.;

		m->p = seprint(m->p, m->e, "\n\trr(csrc=%8ux frac=%3d%% cumu=%10d seqhi=%10ud jitter=%10d lsr=%8ux dlsr=%f)",
				NetL(r->ssrc), frac, NetL(r->lost), NetL(r->seqhi),
				NetL(r->jitter), NetL(r->lsr),
				dlsr);
	}
	m->pr = nil;
	return 0;
}

Proto rtcp = {
	"rtcp",
	nil,
	nil,
	p_seprint,
	nil,
	nil,
	nil,
	defaultframer
};
