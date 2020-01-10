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
int minicachesleeptime = 0;

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

static u64int
ie2diskaddr(Index *ix, ISect *is, IEntry *ie)
{
	u64int bucket, addr;

	bucket = hashbits(ie->score, 32)/ix->div;
	addr = is->blockbase + ((bucket - is->start) << is->blocklog);
	return addr;
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
	addr = ie2diskaddr(ix, is, iefirst);
	nbuf = 0;
	for(l = &iefirst->nextdirty; (ie = *l) != nil; l = &(*l)->nextdirty){
		naddr = ie2diskaddr(ix, is, ie);
		if(naddr - addr >= Bufsize)
			break;
		nbuf = naddr - addr;
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
	int err, i, werr, h, bsize, t;
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

	trace(TraceProc, "icachewritesect enter %ud %ud %llud",
		lo, hi, iwrite.as.aa);

	iedirty = icachedirty(lo, hi, iwrite.as.aa);
	iedirty = iesort(iedirty);
	bsize = 1 << is->blocklog;
	err = 0;

	while(iedirty){
		disksched();
		while((t = icachesleeptime) == SleepForever){
			sleep(1000);
			disksched();
		}
		if(t < minicachesleeptime)
			t = minicachesleeptime;
		if(t > 0)
			sleep(t);
		trace(TraceProc, "icachewritesect nextchunk");
		chunk = nextchunk(ix, is, &iedirty, &addr, &nbuf);

		trace(TraceProc, "icachewritesect readpart 0x%llux+0x%ux",
			addr, nbuf);
		if(readpart(is->part, addr, buf, nbuf) < 0){
			fprint(2, "%s: part %s addr 0x%llux: icachewritesect "
				"readpart: %r\n", argv0, is->part->name, addr);
			err  = -1;
			continue;
		}
		trace(TraceProc, "icachewritesect updatebuf");
		addstat(StatIsectReadBytes, nbuf);
		addstat(StatIsectRead, 1);

		for(l=&chunk; (ie=*l)!=nil; l=&ie->nextdirty){
again:
			naddr = ie2diskaddr(ix, is, ie);
			off = naddr - addr;
			if(off+bsize > nbuf){
				fprint(2, "%s: whoops! addr=0x%llux nbuf=%ud "
					"addr+nbuf=0x%llux naddr=0x%llux\n",
					argv0, addr, nbuf, addr+nbuf, naddr);
				assert(off+bsize <= nbuf);
			}
			unpackibucket(&ib, buf+off, is->bucketmagic);
			if(okibucket(&ib, is) < 0){
				fprint(2, "%s: bad bucket XXX\n", argv0);
				goto skipit;
			}
			trace(TraceProc, "icachewritesect add %V at 0x%llux",
				ie->score, naddr);
			h = bucklook(ie->score, ie->ia.type, ib.data, ib.n);
			if(h & 1){
				h ^= 1;
				packientry(ie, &ib.data[h]);
			}else if(ib.n < is->buckmax){
				memmove(&ib.data[h + IEntrySize], &ib.data[h],
					ib.n*IEntrySize - h);
				ib.n++;
				packientry(ie, &ib.data[h]);
			}else{
				fprint(2, "%s: bucket overflow XXX\n", argv0);
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
		}

		diskaccess(1);

		trace(TraceProc, "icachewritesect writepart", addr, nbuf);
		werr = 0;
		if(writepart(is->part, addr, buf, nbuf) < 0 || flushpart(is->part) < 0)
			werr = -1;

		for(i=0; i<nbuf; i+=bsize){
			if((b = _getdblock(is->part, addr+i, ORDWR, 0)) != nil){
				memmove(b->data, buf+i, bsize);
				putdblock(b);
			}
		}

		if(werr < 0){
			fprint(2, "%s: part %s addr 0x%llux: icachewritesect "
				"writepart: %r\n", argv0, is->part->name, addr);
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
	int ret;
	uint bsize;
	ISect *is;
	Index *ix;
	u8int *buf;

	ix = mainindex;
	is = v;
	threadsetname("icachewriteproc:%s", is->part->name);

	bsize = 1<<is->blocklog;
	buf = emalloc(Bufsize+bsize);
	buf = (u8int*)(((uintptr)buf+bsize-1)&~(uintptr)(bsize-1));

	for(;;){
		trace(TraceProc, "icachewriteproc recv");
		recv(is->writechan, 0);
		trace(TraceWork, "start");
		ret = icachewritesect(ix, is, buf);
		trace(TraceProc, "icachewriteproc send");
		trace(TraceWork, "finish");
		sendul(is->writedonechan, ret);
	}
}

static void
icachewritecoord(void *v)
{
	int i, err;
	Index *ix;
	AState as;

	USED(v);

	threadsetname("icachewritecoord");

	ix = mainindex;
	iwrite.as = icachestate();

	for(;;){
		trace(TraceProc, "icachewritecoord sleep");
		waitforkick(&iwrite.round);
		trace(TraceWork, "start");
		as = icachestate();
		if(as.arena==iwrite.as.arena && as.aa==iwrite.as.aa){
			/* will not be able to do anything more than last flush - kick disk */
			trace(TraceProc, "icachewritecoord kick dcache");
			kickdcache();
			trace(TraceProc, "icachewritecoord kicked dcache");
			goto SkipWork;	/* won't do anything; don't bother rewriting bloom filter */
		}
		iwrite.as = as;

		trace(TraceProc, "icachewritecoord start flush");
		if(iwrite.as.arena){
			for(i=0; i<ix->nsects; i++)
				send(ix->sects[i]->writechan, 0);
			if(ix->bloom)
				send(ix->bloom->writechan, 0);

			err = 0;
			for(i=0; i<ix->nsects; i++)
				err |= recvul(ix->sects[i]->writedonechan);
			if(ix->bloom)
				err |= recvul(ix->bloom->writedonechan);

			trace(TraceProc, "icachewritecoord donewrite err=%d", err);
			if(err == 0){
				setatailstate(&iwrite.as);
			}
		}
	SkipWork:
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
