#include <u.h>
#include <libc.h>
#include <venti.h>
#include <libsec.h>

typedef struct Mem Mem;
typedef struct Frag Frag;

enum {
	BigMemSize = MaxFragSize,
	SmallMemSize = BigMemSize/8,
	NLocalFrag = 2,
};

/* position to carve out of a Mem */
enum {
	PFront,
	PMiddle,
	PEnd,
};

struct Mem
{
	Lock lk;
	int ref;
	uchar *bp;
	uchar *ep;
	uchar *rp;
	uchar *wp;
	Mem *next;
};

enum {
	FragLocalFree,
	FragLocalAlloc,
	FragGlobal,
};
	
struct Frag
{
	int state;
	Mem *mem;
	uchar *rp;
	uchar *wp;
	Frag *next;
	void (*free)(void*);
	void *a;
};

struct Packet
{
	int size;
	int asize;  /* allocated memory - greater than size unless foreign frags */
	
	Packet *next;
	
	Frag *first;
	Frag *last;
	
	Frag local[NLocalFrag];
};

static Frag *fragalloc(Packet*, int n, int pos, Frag *next);
static Frag *fragdup(Packet*, Frag*);
static void fragfree(Frag*);

static Mem *memalloc(int, int);
static void memfree(Mem*);
static int memhead(Mem *m, uchar *rp, int n);
static int memtail(Mem *m, uchar *wp, int n);

static char EPacketSize[] = "bad packet size";
static char EPacketOffset[] = "bad packet offset";
static char EBadSize[] = "bad size";

static struct {
	Lock lk;
	Packet *packet;
	int npacket;
	Frag *frag;
	int nfrag;
	Mem *bigmem;
	int nbigmem;
	Mem *smallmem;
	int nsmallmem;
} freelist;

#define FRAGSIZE(f) ((f)->wp - (f)->rp)
#define FRAGASIZE(f) ((f)->mem ? (f)->mem->ep - (f)->mem->bp : 0)

#define NOTFREE(p) assert((p)->size>=0)

Packet *
packetalloc(void)
{
	Packet *p;

	lock(&freelist.lk);
	p = freelist.packet;
	if(p != nil)
		freelist.packet = p->next;
	else
		freelist.npacket++;
	unlock(&freelist.lk);

	if(p == nil)
		p = vtbrk(sizeof(Packet));
	else
		assert(p->size == -1);
	p->size = 0;
	p->asize = 0;
	p->first = nil;
	p->last = nil;
	p->next = nil;

if(0)fprint(2, "packetalloc %p from %08lux %08lux %08lux\n", p, *((uint*)&p+2), *((uint*)&p+3), *((uint*)&p+4));

	return p;
}

void
packetfree(Packet *p)
{
	Frag *f, *ff;

if(0)fprint(2, "packetfree %p from %08lux\n", p, getcallerpc(&p));

	if(p == nil)
		return;

	NOTFREE(p);
	p->size = -1;

	for(f=p->first; f!=nil; f=ff) {
		ff = f->next;
		fragfree(f);
	}
	p->first = nil;
	p->last = nil;

	lock(&freelist.lk);
	p->next = freelist.packet;
	freelist.packet = p;
	unlock(&freelist.lk);
}

Packet *
packetdup(Packet *p, int offset, int n)
{	
	Frag *f, *ff;
	Packet *pp;

	NOTFREE(p);
	if(offset < 0 || n < 0 || offset+n > p->size) {
		werrstr(EBadSize);
		return nil;
	}

	pp = packetalloc();
	if(n == 0)
		return pp;

	pp->size = n;

	/* skip offset */
	for(f=p->first; offset >= FRAGSIZE(f); f=f->next)
		offset -= FRAGSIZE(f);
	
	/* first frag */
	ff = fragdup(pp, f);
	ff->rp += offset;
	pp->first = ff;
	n -= FRAGSIZE(ff);
	pp->asize += FRAGASIZE(ff);

	/* the remaining */
	while(n > 0) {
		f = f->next;
		ff->next = fragdup(pp, f);
		ff = ff->next;
		n -= FRAGSIZE(ff);
		pp->asize += FRAGASIZE(ff);
	}
	
	/* fix up last frag: note n <= 0 */
	ff->wp += n;
	ff->next = nil;
	pp->last = ff;

	return pp;
}

Packet *
packetsplit(Packet *p, int n)
{
	Packet *pp;
	Frag *f, *ff;

	NOTFREE(p);
	if(n < 0 || n > p->size) {
		werrstr(EPacketSize);
		return nil;
	}

	pp = packetalloc();
	if(n == 0)
		return pp;

	pp->size = n;
	p->size -= n;
	ff = nil;
	for(f=p->first; n > 0 && n >= FRAGSIZE(f); f=f->next) {
		n -= FRAGSIZE(f);
		p->asize -= FRAGASIZE(f);
		pp->asize += FRAGASIZE(f);
		ff = f;
	}

	/* split shared frag */
	if(n > 0) {
		ff = f;
		f = fragdup(pp, ff);
		pp->asize += FRAGASIZE(ff);
		ff->next = nil;
		ff->wp = ff->rp + n;
		f->rp += n;
	}

	pp->first = p->first;
	pp->last = ff;
	p->first = f;
	return pp;
}

int
packetconsume(Packet *p, uchar *buf, int n)
{
	NOTFREE(p);
	if(buf && packetcopy(p, buf, 0, n) < 0)
		return -1;
	return packettrim(p, n, p->size-n);
}

int
packettrim(Packet *p, int offset, int n)
{
	Frag *f, *ff;

	NOTFREE(p);
	if(offset < 0 || offset > p->size) {
		werrstr(EPacketOffset);
		return -1;
	}

	if(n < 0 || offset + n > p->size) {
		werrstr(EPacketOffset);
		return -1;
	}

	p->size = n;

	/* easy case */
	if(n == 0) {
		for(f=p->first; f != nil; f=ff) {
			ff = f->next;
			fragfree(f);
		}
		p->first = p->last = nil;
		p->asize = 0;
		return 0;
	}
	
	/* free before offset */
	for(f=p->first; offset >= FRAGSIZE(f); f=ff) {
		p->asize -= FRAGASIZE(f);
		offset -= FRAGSIZE(f);
		ff = f->next;
		fragfree(f);
	}

	/* adjust frag */
	f->rp += offset;
	p->first = f;

	/* skip middle */
	for(; n > 0 && n > FRAGSIZE(f); f=f->next)
		n -= FRAGSIZE(f);

	/* adjust end */
	f->wp = f->rp + n;
	p->last = f;
	ff = f->next;
	f->next = nil;

	/* free after */
	for(f=ff; f != nil; f=ff) {
		p->asize -= FRAGASIZE(f);
		ff = f->next;
		fragfree(f);
	}
	return 0;
}

uchar *
packetheader(Packet *p, int n)
{
	Frag *f;
	Mem *m;

	NOTFREE(p);
	if(n <= 0 || n > MaxFragSize) {
		werrstr(EPacketSize);
		return nil;
	}

	p->size += n;
	
	/* try and fix in current frag */
	f = p->first;
	if(f != nil) {
		m = f->mem;
		if(n <= f->rp - m->bp)
		if(m->ref == 1 || memhead(m, f->rp, n) >= 0) {
			f->rp -= n;
			return f->rp;
		}
	}

	/* add frag to front */
	f = fragalloc(p, n, PEnd, p->first);
	p->asize += FRAGASIZE(f);
	if(p->first == nil)
		p->last = f;
	p->first = f;
	return f->rp;
}

uchar *
packettrailer(Packet *p, int n)
{
	Mem *m;
	Frag *f;

	NOTFREE(p);
	if(n <= 0 || n > MaxFragSize) {
		werrstr(EPacketSize);
		return nil;
	}

	p->size += n;
	
	/* try and fix in current frag */
	if(p->first != nil) {
		f = p->last;
		m = f->mem;
		if(n <= m->ep - f->wp)
		if(m->ref == 1 || memtail(m, f->wp, n) >= 0) {
			f->wp += n;
			return f->wp - n;
		}
	}

	/* add frag to end */
	f = fragalloc(p, n, (p->first == nil)?PMiddle:PFront, nil);
	p->asize += FRAGASIZE(f);
	if(p->first == nil)
		p->first = f;
	else
		p->last->next = f;
	p->last = f;
	return f->rp;
}

void
packetprefix(Packet *p, uchar *buf, int n)
{
	Frag *f;
	int nn;
	Mem *m;

	NOTFREE(p);
	if(n <= 0)
		return;

	p->size += n;

	/* try and fix in current frag */
	f = p->first;
	if(f != nil) {
		m = f->mem;
		nn = f->rp - m->bp;
		if(nn > n)
			nn = n;
		if(m->ref == 1 || memhead(m, f->rp, nn) >= 0) {
			f->rp -= nn;
			n -= nn;
			memmove(f->rp, buf+n, nn);
		}
	}

	while(n > 0) {
		nn = n;
		if(nn > MaxFragSize)
			nn = MaxFragSize;
		f = fragalloc(p, nn, PEnd, p->first);	
		p->asize += FRAGASIZE(f);
		if(p->first == nil)
			p->last = f;
		p->first = f;
		n -= nn;
		memmove(f->rp, buf+n, nn);
	}
}

void
packetappend(Packet *p, uchar *buf, int n)
{
	Frag *f;
	int nn;
	Mem *m;

	NOTFREE(p);
	if(n <= 0)
		return;

	p->size += n;
	/* try and fix in current frag */
	if(p->first != nil) {
		f = p->last;
		m = f->mem;
		nn = m->ep - f->wp;
		if(nn > n)
			nn = n;
		if(m->ref == 1 || memtail(m, f->wp, nn) >= 0) {
			memmove(f->wp, buf, nn);
			f->wp += nn;
			buf += nn;
			n -= nn;
		}
	}
	
	while(n > 0) {
		nn = n;
		if(nn > MaxFragSize)
			nn = MaxFragSize;
		f = fragalloc(p, nn, (p->first == nil)?PMiddle:PFront, nil);
		p->asize += FRAGASIZE(f);
		if(p->first == nil)
			p->first = f;
		else
			p->last->next = f;
		p->last = f;
		memmove(f->rp, buf, nn);
		buf += nn;
		n -= nn;
	}
	return;
}

void
packetconcat(Packet *p, Packet *pp)
{
	NOTFREE(p);
	NOTFREE(pp);
	if(pp->size == 0)
		return;
	p->size += pp->size;
	p->asize += pp->asize;

	if(p->first != nil)
		p->last->next = pp->first;
	else
		p->first = pp->first;
	p->last = pp->last;
	pp->size = 0;
	pp->asize = 0;
	pp->first = nil;
	pp->last = nil;
}

uchar *
packetpeek(Packet *p, uchar *buf, int offset, int n)
{
	Frag *f;
	int nn;
	uchar *b;

	NOTFREE(p);
	if(n == 0)
		return buf;

	if(offset < 0 || offset >= p->size) {
		werrstr(EPacketOffset);
		return nil;
	}

	if(n < 0 || offset + n > p->size) {
		werrstr(EPacketSize);
		return nil;
	}
	
	/* skip up to offset */
	for(f=p->first; offset >= FRAGSIZE(f); f=f->next)
		offset -= FRAGSIZE(f);

	/* easy case */
	if(offset + n <= FRAGSIZE(f))
		return f->rp + offset;

	for(b=buf; n>0; n -= nn) {
		nn = FRAGSIZE(f) - offset;
		if(nn > n)
			nn = n;
		memmove(b, f->rp+offset, nn);
		offset = 0;
		f = f->next;
		b += nn;
	}

	return buf;
}

int
packetcopy(Packet *p, uchar *buf, int offset, int n)
{
	uchar *b;

	NOTFREE(p);
	b = packetpeek(p, buf, offset, n);
	if(b == nil)
		return -1;
	if(b != buf)
		memmove(buf, b, n);
	return 0;
}

int
packetfragments(Packet *p, IOchunk *io, int nio, int offset)
{
	Frag *f;
	int size;
	IOchunk *eio;

	NOTFREE(p);
	if(p->size == 0 || nio <= 0)
		return 0;
	
	if(offset < 0 || offset > p->size) {
		werrstr(EPacketOffset);
		return -1;
	}

	for(f=p->first; offset >= FRAGSIZE(f); f=f->next)
		offset -= FRAGSIZE(f);

	size = 0;
	eio = io + nio;
	for(; f != nil && io < eio; f=f->next) {
		io->addr = f->rp + offset;
		io->len = f->wp - (f->rp + offset);	
		offset = 0;
		size += io->len;
		io++;
	}

	return size;
}

void
packetstats(void)
{
	Packet *p;
	Frag *f;
	Mem *m;

	int np, nf, nsm, nbm;

	lock(&freelist.lk);
	np = 0;
	for(p=freelist.packet; p; p=p->next)
		np++;
	nf = 0;
	for(f=freelist.frag; f; f=f->next)
		nf++;
	nsm = 0;
	for(m=freelist.smallmem; m; m=m->next)
		nsm++;
	nbm = 0;
	for(m=freelist.bigmem; m; m=m->next)
		nbm++;
	
	fprint(2, "packet: %d/%d frag: %d/%d small mem: %d/%d big mem: %d/%d\n",
		np, freelist.npacket,
		nf, freelist.nfrag,
		nsm, freelist.nsmallmem,
		nbm, freelist.nbigmem);

	unlock(&freelist.lk);
}


uint
packetsize(Packet *p)
{
	NOTFREE(p);
	if(0) {
		Frag *f;
		int size = 0;
	
		for(f=p->first; f; f=f->next)
			size += FRAGSIZE(f);
		if(size != p->size)
			fprint(2, "packetsize %d %d\n", size, p->size);
		assert(size == p->size);
	}
	return p->size;
}

uint
packetasize(Packet *p)
{
	NOTFREE(p);
	if(0) {
		Frag *f;
		int asize = 0;
	
		for(f=p->first; f; f=f->next)
			asize += FRAGASIZE(f);
		if(asize != p->asize)
			fprint(2, "packetasize %d %d\n", asize, p->asize);
		assert(asize == p->asize);
	}
	return p->asize;
}

void
packetsha1(Packet *p, uchar digest[VtScoreSize])
{
	DigestState ds;
	Frag *f;
	int size;

	NOTFREE(p);
	memset(&ds, 0, sizeof ds);
	size = p->size;
	for(f=p->first; f; f=f->next) {
		sha1(f->rp, FRAGSIZE(f), nil, &ds);
		size -= FRAGSIZE(f);
	}
	assert(size == 0);
	sha1(nil, 0, digest, &ds);
}

int
packetcmp(Packet *pkt0, Packet *pkt1)
{
	Frag *f0, *f1;
	int n0, n1, x;

	NOTFREE(pkt0);
	NOTFREE(pkt1);
	f0 = pkt0->first;
	f1 = pkt1->first;

	if(f0 == nil)
		return (f1 == nil)?0:-1;
	if(f1 == nil)
		return 1;
	n0 = FRAGSIZE(f0);
	n1 = FRAGSIZE(f1);

	for(;;) {
		if(n0 < n1) {
			x = memcmp(f0->wp - n0, f1->wp - n1, n0);
			if(x != 0)
				return x;
			n1 -= n0;
			f0 = f0->next;
			if(f0 == nil)
				return -1;
			n0 = FRAGSIZE(f0);
		} else if (n0 > n1) {
			x = memcmp(f0->wp - n0, f1->wp - n1, n1);
			if(x != 0)
				return x;
			n0 -= n1;
			f1 = f1->next;
			if(f1 == nil)
				return 1;
			n1 = FRAGSIZE(f1);
		} else { /* n0 == n1 */
			x = memcmp(f0->wp - n0, f1->wp - n1, n0);
			if(x != 0)
				return x;
			f0 = f0->next;
			f1 = f1->next;
			if(f0 == nil)
				return (f1 == nil)?0:-1;
			if(f1 == nil)
				return 1;
			n0 = FRAGSIZE(f0);
			n1 = FRAGSIZE(f1);
		}
	}
	return 0; /* for ken */
}
	

static Frag *
fragalloc(Packet *p, int n, int pos, Frag *next)
{
	Frag *f, *ef;
	Mem *m;

	/* look for local frag */
	f = &p->local[0];
	ef = &p->local[NLocalFrag];
	for(;f<ef; f++) {
		if(f->state == FragLocalFree) {
			f->state = FragLocalAlloc;
			goto Found;
		}
	}
	lock(&freelist.lk);	
	f = freelist.frag;
	if(f != nil)
		freelist.frag = f->next;
	else
		freelist.nfrag++;
	unlock(&freelist.lk);

	if(f == nil) {
		f = vtbrk(sizeof(Frag));
		f->state = FragGlobal;
	}

Found:
	f->next = next;

	if(n == 0){
		f->mem = 0;
		f->rp = 0;
		f->wp = 0;
		return f;
	}

	if(pos == PEnd && next == nil)
		pos = PMiddle;
	m = memalloc(n, pos);
	f->mem = m;
	f->rp = m->rp;
	f->wp = m->wp;
	return f;
}

Packet*
packetforeign(uchar *buf, int n, void (*free)(void *a), void *a)
{
	Packet *p;
	Frag *f;

	p = packetalloc();
	f = fragalloc(p, 0, 0, nil);
	f->free = free;
	f->a = a;
	f->next = nil;
	f->rp = buf;
	f->wp = buf+n;

	p->first = f;
	p->size = n;
	return p;
}

static Frag *
fragdup(Packet *p, Frag *f)
{
	Frag *ff;
	Mem *m;

	m = f->mem;	

	/*
	 * m->rp && m->wp can be out of date when ref == 1
	 * also, potentially reclaims space from previous frags
	 */
	if(m && m->ref == 1) {
		m->rp = f->rp;
		m->wp = f->wp;
	}

	ff = fragalloc(p, 0, 0, nil);
	*ff = *f;
	if(m){
		lock(&m->lk);
		m->ref++;
		unlock(&m->lk);
	}
	return ff;
}


static void
fragfree(Frag *f)
{
	if(f->mem == nil){
		if(f->free)
			(*f->free)(f->a);
	}else{
		memfree(f->mem);
		f->mem = 0;
	}

	if(f->state == FragLocalAlloc) {
		f->state = FragLocalFree;
		return;
	}

	lock(&freelist.lk);
	f->next = freelist.frag;
	freelist.frag = f;
	unlock(&freelist.lk);	
}

static Mem *
memalloc(int n, int pos)
{
	Mem *m;
	int nn;

	if(n < 0 || n > MaxFragSize) {
		werrstr(EPacketSize);
		return 0;
	}
	if(n <= SmallMemSize) {
		lock(&freelist.lk);
		m = freelist.smallmem;
		if(m != nil)
			freelist.smallmem = m->next;
		else
			freelist.nsmallmem++;
		unlock(&freelist.lk);
		nn = SmallMemSize;
	} else {
		lock(&freelist.lk);
		m = freelist.bigmem;
		if(m != nil)
			freelist.bigmem = m->next;
		else
			freelist.nbigmem++;
		unlock(&freelist.lk);
		nn = BigMemSize;
	}

	if(m == nil) {
		m = vtbrk(sizeof(Mem));
		m->bp = vtbrk(nn);
		m->ep = m->bp + nn;
	}
	assert(m->ref == 0);	
	m->ref = 1;

	switch(pos) {
	default:
		assert(0);
	case PFront:
		m->rp = m->bp;
		break;
	case PMiddle:
		/* leave a little bit at end */
		m->rp = m->ep - n - 32;
		break;
	case PEnd:
		m->rp = m->ep - n;
		break; 
	}
	/* check we did not blow it */
	if(m->rp < m->bp)
		m->rp = m->bp;
	m->wp = m->rp + n;
	assert(m->rp >= m->bp && m->wp <= m->ep);
	return m;
}

static void
memfree(Mem *m)
{
	lock(&m->lk);
	m->ref--;
	if(m->ref > 0) {
		unlock(&m->lk);
		return;
	}
	unlock(&m->lk);
	assert(m->ref == 0);

	switch(m->ep - m->bp) {
	default:
		assert(0);
	case SmallMemSize:
		lock(&freelist.lk);
		m->next = freelist.smallmem;
		freelist.smallmem = m;
		unlock(&freelist.lk);
		break;
	case BigMemSize:
		lock(&freelist.lk);
		m->next = freelist.bigmem;
		freelist.bigmem = m;
		unlock(&freelist.lk);
		break;
	}
}

static int
memhead(Mem *m, uchar *rp, int n)
{
	lock(&m->lk);
	if(m->rp != rp) {
		unlock(&m->lk);
		return -1;
	}
	m->rp -= n;
	unlock(&m->lk);
	return 0;
}

static int
memtail(Mem *m, uchar *wp, int n)
{
	lock(&m->lk);
	if(m->wp != wp) {
		unlock(&m->lk);
		return -1;
	}
	m->wp += n;
	unlock(&m->lk);
	return 0;
}
