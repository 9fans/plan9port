/*
 * Write the dirty icache entries to disk.  Random seeks are
 * so expensive that it makes sense to wait until we have
 * a lot and then just make a sequential pass over the disk.
 */
#include "stdinc.h"
#include "dat.h"
#include "fns.h"

static void icachewriteproc(void*);
static void icachewritecoord(void*);
static IEntry *iesort(IEntry*);

int icachesleeptime = 1000;	/* milliseconds */

enum
{
	Bufsize = 8*1024*1024
};

typedef struct IWrite IWrite;
struct IWrite
{
	Round round;
	AState as;
};

static IWrite iwrite;

void
initicachewrite(void)
{
	int i;
	Index *ix;

	initround(&iwrite.round, "icache", 120*60*1000);
	ix = mainindex;
	for(i=0; i<ix->nsects; i++){
		ix->sects[i]->writechan = chancreate(sizeof(ulong), 1);
		ix->sects[i]->writedonechan = chancreate(sizeof(ulong), 1);
		vtproc(icachewriteproc, ix->sects[i]);
	}
	vtproc(icachewritecoord, nil);
	vtproc(delaykickroundproc, &iwrite.round);
}

static IEntry*
nextchunk(Index *ix, ISect *is, IEntry **pie, u64int *paddr, uint *pnbuf)
{
	u64int addr, naddr;
	uint nbuf;
	int bsize;
	IEntry *iefirst, *ie, **l;

	bsize = 1<<is->blocklog;
	iefirst = *pie;
	addr = is->blockbase + ((u64int)(hashbits(iefirst->score, 32) / ix->div - is->start) << is->blocklog);
	nbuf = 0;
	for(l=&iefirst->nextdirty; (ie=*l)!=nil; l=&(*l)->nextdirty){
		naddr = is->blockbase + ((u64int)(hashbits(ie->score, 32) / ix->div - is->start) << is->blocklog);
		if(naddr - addr >= Bufsize)
			break;
		nbuf = naddr-addr;
	}
	nbuf += bsize;

	*l = nil;
	*pie = ie;
	*paddr = addr;
	*pnbuf = nbuf;
	return iefirst;
}
	
static int
icachewritesect(Index *ix, ISect *is, u8int *buf)
{
	int err, h, bsize;
	u32int lo, hi;
	u64int addr, naddr;
	uint nbuf, off;
	DBlock *b;
	IBucket ib;
	IEntry *ie, *iedirty, **l, *chunk;

	lo = is->start * ix->div;
	if(TWID32/ix->div < is->stop)
		hi = TWID32;
	else
		hi = is->stop * ix->div - 1;

	trace(TraceProc, "icachewritesect enter %ud %ud %llud", lo, hi, iwrite.as.aa);

	iedirty = icachedirty(lo, hi, iwrite.as.aa);
	iedirty = iesort(iedirty);
	bsize = 1<<is->blocklog;
	err = 0;

	while(iedirty){
		sleep(icachesleeptime);
		trace(TraceProc, "icachewritesect nextchunk");
		chunk = nextchunk(ix, is, &iedirty, &addr, &nbuf);

		trace(TraceProc, "icachewritesect readpart 0x%llux+0x%ux", addr, nbuf);
		if(readpart(is->part, addr, buf, nbuf) < 0){
			// XXX
			fprint(2, "icachewriteproc readpart: %r\n");
			err  = -1;
			continue;
		}
		trace(TraceProc, "icachewritesect updatebuf");
		addstat(StatIsectReadBytes, nbuf);
		addstat(StatIsectRead, 1);

		for(l=&chunk; (ie=*l)!=nil; l=&ie->nextdirty){
		again:
			naddr = is->blockbase + ((u64int)(hashbits(ie->score, 32) / ix->div - is->start) << is->blocklog);
			off = naddr - addr;
			if(off+bsize > nbuf){
				fprint(2, "whoops! addr=0x%llux nbuf=%ud addr+nbuf=0x%llux naddr=0x%llux\n",
					addr, nbuf, addr+nbuf, naddr);
				assert(off+bsize <= nbuf);
			}
			unpackibucket(&ib, buf+off, is->bucketmagic);
			if(okibucket(&ib, is) < 0){
				fprint(2, "bad bucket XXX\n");
				goto skipit;
			}
			trace(TraceProc, "icachewritesect add %V at 0x%llux", ie->score, naddr);
			h = bucklook(ie->score, ie->ia.type, ib.data, ib.n);
			if(h & 1){
				h ^= 1;
				packientry(ie, &ib.data[h]);
			}else if(ib.n < is->buckmax){
				memmove(&ib.data[h+IEntrySize], &ib.data[h], ib.n*IEntrySize - h);
				ib.n++;
				packientry(ie, &ib.data[h]);
			}else{
				fprint(2, "bucket overflow XXX\n");
			skipit:
				err = -1;
				*l = ie->nextdirty;
				ie = *l;
				if(ie)
					goto again;
				else
					break;
			}
			packibucket(&ib, buf+off, is->bucketmagic);
			if((b = _getdblock(is->part, naddr, ORDWR, 0)) != nil){
				memmove(b->data, buf+off, bsize);
				putdblock(b);
			}
		}

		trace(TraceProc, "icachewritesect writepart", addr, nbuf);
		if(writepart(is->part, addr, buf, nbuf) < 0){
			// XXX
			fprint(2, "icachewriteproc writepart: %r\n");
			err = -1;
			continue;
		}
		addstat(StatIsectWriteBytes, nbuf);
		addstat(StatIsectWrite, 1);
		icacheclean(chunk);
	}

	trace(TraceProc, "icachewritesect done");
	return err;
}

static void
icachewriteproc(void *v)
{
	uint bsize;
	ISect *is;
	Index *ix;
	u8int *buf;

	ix = mainindex;
	is = v;
	threadsetname("icachewriteproc:%s", is->part->name);

	bsize = 1<<is->blocklog;
	buf = emalloc(Bufsize+bsize);
	buf = (u8int*)(((ulong)buf+bsize-1)&~(ulong)(bsize-1));

	for(;;){
		trace(TraceProc, "icachewriteproc recv");
		recv(is->writechan, 0);
		trace(TraceWork, "start");
		icachewritesect(ix, is, buf);
		trace(TraceProc, "icachewriteproc send");
		trace(TraceWork, "finish");
		send(is->writedonechan, 0);
	}
}

static void
icachewritecoord(void *v)
{
	int i;
	Index *ix;
	AState as;

	USED(v);

	threadsetname("icachewritecoord");

	ix = mainindex;
	iwrite.as = diskstate();

	for(;;){
		trace(TraceProc, "icachewritecoord sleep");
		waitforkick(&iwrite.round);
		trace(TraceWork, "start");
		as = diskstate();
		if(as.arena==iwrite.as.arena && as.aa==iwrite.as.aa){
			/* will not be able to do anything more than last flush - kick disk */
			trace(TraceProc, "icachewritecoord flush dcache");
			kickdcache();
			trace(TraceProc, "icachewritecoord flushed dcache");
		}
		iwrite.as = as;

		trace(TraceProc, "icachewritecoord start flush");
		if(iwrite.as.arena){
			for(i=0; i<ix->nsects; i++)
				send(ix->sects[i]->writechan, 0);
			if(ix->bloom)
				send(ix->bloom->writechan, 0);
		
			for(i=0; i<ix->nsects; i++)
				recv(ix->sects[i]->writedonechan, 0);
			if(ix->bloom)
				recv(ix->bloom->writedonechan, 0);

			trace(TraceProc, "icachewritecoord donewrite");
			setatailstate(&iwrite.as);
		}
		icacheclean(nil);	/* wake up anyone waiting */
		trace(TraceWork, "finish");
		addstat(StatIcacheFlush, 1);
	}
}

void
flushicache(void)
{
	trace(TraceProc, "flushicache enter");
	kickround(&iwrite.round, 1);
	trace(TraceProc, "flushicache exit");
}

void
kickicache(void)
{
	kickround(&iwrite.round, 0);
}

void
delaykickicache(void)
{
	delaykickround(&iwrite.round);
}

static IEntry*
iesort(IEntry *ie)
{
	int cmp;
	IEntry **l;
	IEntry *ie1, *ie2, *sorted;

	if(ie == nil || ie->nextdirty == nil)
		return ie;

	/* split the lists */
	ie1 = ie;
	ie2 = ie;
	if(ie2)
		ie2 = ie2->nextdirty;
	if(ie2)
		ie2 = ie2->nextdirty;
	while(ie1 && ie2){
		ie1 = ie1->nextdirty;
		ie2 = ie2->nextdirty;
		if(ie2)
			ie2 = ie2->nextdirty;
	}
	if(ie1){
		ie2 = ie1->nextdirty;
		ie1->nextdirty = nil;
	}

	/* sort the lists */
	ie1 = iesort(ie);
	ie2 = iesort(ie2);

	/* merge the lists */
	sorted = nil;
	l = &sorted;
	cmp = 0;
	while(ie1 || ie2){
		if(ie1 && ie2)
			cmp = scorecmp(ie1->score, ie2->score);
		if(ie1==nil || (ie2 && cmp > 0)){
			*l = ie2;
			l = &ie2->nextdirty;
			ie2 = ie2->nextdirty;
		}else{
			*l = ie1;
			l = &ie1->nextdirty;
			ie1 = ie1->nextdirty;
		}
	}
	*l = nil;
	return sorted;
}

