#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include "dat.h"
#include "fns.h"

/*
 * We used to use 100 i/o buffers of size 2kb (Sectorsize).
 * Unfortunately, reading 2kb at a time often hopping around
 * the disk doesn't let us get near the disk bandwidth.
 *
 * Based on a trace of iobuf address accesses taken while
 * tarring up a Plan 9 distribution CD, we now use 16 128kb
 * buffers.  This works for ISO9660 because data is required
 * to be laid out contiguously; effectively we're doing agressive
 * readahead.  Because the buffers are so big and the typical
 * disk accesses so concentrated, it's okay that we have so few
 * of them.
 *
 * If this is used to access multiple discs at once, it's not clear
 * how gracefully the scheme degrades, but I'm not convinced
 * it's worth worrying about.		-rsc
 */

/* trying a larger value to get greater throughput - geoff */
#define	BUFPERCLUST	256 /* sectors/cluster; was 64, 64*Sectorsize = 128kb */
#define	NCLUST		16

int nclust = NCLUST;

static Ioclust*	iohead;
static Ioclust*	iotail;

static Ioclust*	getclust(Xdata*, long);
static void	putclust(Ioclust*);
static void	xread(Ioclust*);

void
iobuf_init(void)
{
	int i, j, n;
	Ioclust *c;
	Iobuf *b;
	uchar *mem;

	n = nclust*sizeof(Ioclust) +
		nclust*BUFPERCLUST*(sizeof(Iobuf)+Sectorsize);
	mem = malloc(n);
	if(mem == (void*)0)
		panic(0, "iobuf_init");
	memset(mem, 0, n);

	for(i=0; i<nclust; i++){
		c = (Ioclust*)mem;
		mem += sizeof(Ioclust);
		c->addr = -1;
		c->prev = iotail;
		if(iotail)
			iotail->next = c;
		iotail = c;
		if(iohead == nil)
			iohead = c;

		c->buf = (Iobuf*)mem;
		mem += BUFPERCLUST*sizeof(Iobuf);
		c->iobuf = mem;
		mem += BUFPERCLUST*Sectorsize;
		for(j=0; j<BUFPERCLUST; j++){
			b = &c->buf[j];
			b->clust = c;
			b->addr = -1;
			b->iobuf = c->iobuf+j*Sectorsize;
		}
	}
}

void
purgebuf(Xdata *dev)
{
	Ioclust *p;

	for(p=iohead; p!=nil; p=p->next)
		if(p->dev == dev){
			p->addr = -1;
			p->busy = 0;
		}
}

static Ioclust*
getclust(Xdata *dev, long addr)
{
	Ioclust *c, *f;

	f = nil;
	for(c=iohead; c; c=c->next){
		if(!c->busy)
			f = c;
		if(c->addr == addr && c->dev == dev){
			c->busy++;
			return c;
		}
	}

	if(f == nil)
		panic(0, "out of buffers");

	f->addr = addr;
	f->dev = dev;
	f->busy++;
	if(waserror()){
		f->addr = -1;	/* stop caching */
		putclust(f);
		nexterror();
	}
	xread(f);
	poperror();
	return f;
}

static void
putclust(Ioclust *c)
{
	if(c->busy <= 0)
		panic(0, "putbuf");
	c->busy--;

	/* Link onto head for LRU */
	if(c == iohead)
		return;
	c->prev->next = c->next;

	if(c->next)
		c->next->prev = c->prev;
	else
		iotail = c->prev;

	c->prev = nil;
	c->next = iohead;
	iohead->prev = c;
	iohead = c;
}

Iobuf*
getbuf(Xdata *dev, ulong addr)
{
	int off;
	Ioclust *c;

	off = addr%BUFPERCLUST;
	c = getclust(dev, addr - off);
	if(c->nbuf < off){
		c->busy--;
		error("I/O read error");
	}
	return &c->buf[off];
}

void
putbuf(Iobuf *b)
{
	putclust(b->clust);
}

static void
xread(Ioclust *c)
{
	int n;
	Xdata *dev;

	dev = c->dev;
	seek(dev->dev, (vlong)c->addr * Sectorsize, 0);
	n = readn(dev->dev, c->iobuf, BUFPERCLUST*Sectorsize);
	if(n < Sectorsize)
		error("I/O read error");
	c->nbuf = n/Sectorsize;
}
