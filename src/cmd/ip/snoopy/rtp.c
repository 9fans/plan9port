#include <u.h>
#include <libc.h>
#include <ip.h>
#include "dat.h"
#include "protos.h"

typedef struct Hdr Hdr;
struct Hdr {
	uchar	hdr;		// RTP header
	uchar	marker;	// Payload and marker
	uchar	seq[2];	// Sequence number
	uchar	ts[4];		// Time stamp
	uchar	ssrc[4];	// Synchronization source identifier
};

enum{
	RTPLEN = 12,		// Minimum size of an RTP header
};


static void
p_compile(Filter *f)
{
	sysfatal("unknown rtp field: %s", f->s);
}

static int
p_filter(Filter *f, Msg *m)
{
	USED(f);
	USED(m);
	return 0;
}

static int
p_seprint(Msg *m)
{
	Hdr*h;
	ushort seq;
	ulong ssrc, ts;
	int cc, i;

	if(m->pe - m->ps < RTPLEN)
		return -1;

	h = (Hdr*)m->ps;
	cc = h->hdr & 0xf;
	if(m->pe - m->ps < RTPLEN + cc * 4)
		return -1;

	m->ps += RTPLEN;

	seq = NetS(h->seq);
	ts = NetL(h->ts);
	ssrc = NetL(h->ssrc);

	m->p = seprint(m->p, m->e, "version=%d x=%d cc=%d seq=%d ts=%ld ssrc=%ulx",
				(h->hdr >> 6) & 3, (h->hdr >> 4) & 1, cc, seq, ts, ssrc);
	for(i = 0; i < cc; i++){
		m->p = seprint(m->p, m->e, " csrc[%d]=%d",
				i, NetL(m->ps));
		m->ps += 4;
	}
	m->pr = nil;
	return 0;
}

Proto rtp = {
	"rtp",
	p_compile,
	p_filter,
	p_seprint,
	nil,
	nil,
	defaultframer,
};
