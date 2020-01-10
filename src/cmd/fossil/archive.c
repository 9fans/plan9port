/*
 * Archiver.  In charge of sending blocks to Venti.
 */

#include "stdinc.h"
#include "dat.h"
#include "fns.h"
#include "error.h"

#include "9.h"	/* for consPrint */

#define DEBUG 0

static void archThread(void*);

struct Arch
{
	int ref;
	uint blockSize;
	uint diskSize;
	Cache *c;
	Fs *fs;
	VtConn *z;

	QLock lk;
	Rendez starve;
	Rendez die;
};

Arch *
archInit(Cache *c, Disk *disk, Fs *fs, VtConn *z)
{
	Arch *a;

	a = vtmallocz(sizeof(Arch));

	a->c = c;
	a->z = z;
	a->fs = fs;
	a->blockSize = diskBlockSize(disk);
	a->starve.l = &a->lk;

	a->ref = 2;
	proccreate(archThread, a, STACK);

	return a;
}

void
archFree(Arch *a)
{
	/* kill slave */
	qlock(&a->lk);
	a->die.l = &a->lk;
	rwakeup(&a->starve);
	while(a->ref > 1)
		rsleep(&a->die);
	qunlock(&a->lk);
	vtfree(a);
}

static int
ventiSend(Arch *a, Block *b, uchar *data)
{
	uint n;
	uchar score[VtScoreSize];

	if(DEBUG > 1)
		fprint(2, "ventiSend: sending %#ux %L to venti\n", b->addr, &b->l);
	n = vtzerotruncate(vtType[b->l.type], data, a->blockSize);
	if(DEBUG > 1)
		fprint(2, "ventiSend: truncate %d to %d\n", a->blockSize, n);
	if(vtwrite(a->z, score, vtType[b->l.type], data, n) < 0){
		fprint(2, "ventiSend: vtwrite block %#ux failed: %r\n", b->addr);
		return 0;
	}
	if(vtsha1check(score, data, n) < 0){
		uchar score2[VtScoreSize];
		vtsha1(score2, data, n);
		fprint(2, "ventiSend: vtwrite block %#ux failed vtsha1check %V %V\n",
			b->addr, score, score2);
		return 0;
	}
	if(vtsync(a->z) < 0)
		return 0;
	return 1;
}

/*
 * parameters for recursion; there are so many,
 * and some only change occasionally.  this is
 * easier than spelling things out at each call.
 */
typedef struct Param Param;
struct Param
{
	/* these never change */
	uint snapEpoch;	/* epoch for snapshot being archived */
	uint blockSize;
	Cache *c;
	Arch *a;

	/* changes on every call */
	uint depth;

	/* statistics */
	uint nfixed;
	uint nsend;
	uint nvisit;
	uint nfailsend;
	uint maxdepth;
	uint nreclaim;
	uint nfake;
	uint nreal;

	/* these occasionally change (must save old values and put back) */
	uint dsize;
	uint psize;

	/* return value; avoids using stack space */
	Label l;
	uchar score[VtScoreSize];
};

static void
shaBlock(uchar score[VtScoreSize], Block *b, uchar *data, uint bsize)
{
	vtsha1(score, data, vtzerotruncate(vtType[b->l.type], data, bsize));
}

static uchar*
copyBlock(Block *b, u32int blockSize)
{
	uchar *data;

	data = vtmalloc(blockSize);
	if(data == nil)
		return nil;
	memmove(data, b->data, blockSize);
	return data;
}

/*
 * Walk over the block tree, archiving it to Venti.
 *
 * We don't archive the snapshots. Instead we zero the
 * entries in a temporary copy of the block and archive that.
 *
 * Return value is:
 *
 *	ArchFailure	some error occurred
 *	ArchSuccess	block and all children archived
 * 	ArchFaked	success, but block or children got copied
 */
enum
{
	ArchFailure,
	ArchSuccess,
	ArchFaked,
};
static int
archWalk(Param *p, u32int addr, uchar type, u32int tag)
{
	int ret, i, x, psize, dsize;
	uchar *data, score[VtScoreSize];
	Block *b;
	Label l;
	Entry *e;
	WalkPtr w;
	char err[ERRMAX];

	p->nvisit++;

	b = cacheLocalData(p->c, addr, type, tag, OReadWrite,0);
	if(b == nil){
		fprint(2, "archive(%ud, %#ux): cannot find block: %r\n", p->snapEpoch, addr);
		rerrstr(err, sizeof err);
		if(strcmp(err, ELabelMismatch) == 0){
			/* might as well plod on so we write _something_ to Venti */
			memmove(p->score, vtzeroscore, VtScoreSize);
			return ArchFaked;
		}
		return ArchFailure;
	}

	if(DEBUG) fprint(2, "%*sarchive(%ud, %#ux): block label %L\n",
		p->depth*2, "",  p->snapEpoch, b->addr, &b->l);
	p->depth++;
	if(p->depth > p->maxdepth)
		p->maxdepth = p->depth;

	data = b->data;
	if((b->l.state&BsVenti) == 0){
		initWalk(&w, b, b->l.type==BtDir ? p->dsize : p->psize);
		for(i=0; nextWalk(&w, score, &type, &tag, &e); i++){
			if(e){
				if(!(e->flags&VtEntryActive))
					continue;
				if((e->snap && !e->archive)
				|| (e->flags&VtEntryNoArchive)){
					if(0) fprint(2, "snap; faking %#ux\n", b->addr);
					if(data == b->data){
						data = copyBlock(b, p->blockSize);
						if(data == nil){
							ret = ArchFailure;
							goto Out;
						}
						w.data = data;
					}
					memmove(e->score, vtzeroscore, VtScoreSize);
					e->depth = 0;
					e->size = 0;
					e->tag = 0;
					e->flags &= ~VtEntryLocal;
					entryPack(e, data, w.n-1);
					continue;
				}
			}
			addr = globalToLocal(score);
			if(addr == NilBlock)
				continue;
			dsize = p->dsize;
			psize = p->psize;
			if(e){
				p->dsize= e->dsize;
				p->psize = e->psize;
			}
			qunlock(&b->lk);
			x = archWalk(p, addr, type, tag);
			qlock(&b->lk);
			if(e){
				p->dsize = dsize;
				p->psize = psize;
			}
			while(b->iostate != BioClean && b->iostate != BioDirty)
				rsleep(&b->ioready);
			switch(x){
			case ArchFailure:
				fprint(2, "archWalk %#ux failed; ptr is in %#ux offset %d\n",
					addr, b->addr, i);
				ret = ArchFailure;
				goto Out;
			case ArchFaked:
				/*
				 * When we're writing the entry for an archive directory
				 * (like /archive/2003/1215) then even if we've faked
				 * any data, record the score unconditionally.
				 * This way, we will always record the Venti score here.
				 * Otherwise, temporary data or corrupted file system
				 * would cause us to keep holding onto the on-disk
				 * copy of the archive.
				 */
				if(e==nil || !e->archive)
				if(data == b->data){
if(0) fprint(2, "faked %#ux, faking %#ux (%V)\n", addr, b->addr, p->score);
					data = copyBlock(b, p->blockSize);
					if(data == nil){
						ret = ArchFailure;
						goto Out;
					}
					w.data = data;
				}
				/* fall through */
if(0) fprint(2, "falling\n");
			case ArchSuccess:
				if(e){
					memmove(e->score, p->score, VtScoreSize);
					e->flags &= ~VtEntryLocal;
					entryPack(e, data, w.n-1);
				}else
					memmove(data+(w.n-1)*VtScoreSize, p->score, VtScoreSize);
				if(data == b->data){
					blockDirty(b);
					/*
					 * If b is in the active tree, then we need to note that we've
					 * just removed addr from the active tree (replacing it with the
					 * copy we just stored to Venti).  If addr is in other snapshots,
					 * this will close addr but not free it, since it has a non-empty
					 * epoch range.
					 *
					 * If b is in the active tree but has been copied (this can happen
					 * if we get killed at just the right moment), then we will
					 * mistakenly leak its kids.
					 *
					 * The children of an archive directory (e.g., /archive/2004/0604)
					 * are not treated as in the active tree.
					 */
					if((b->l.state&BsCopied)==0 && (e==nil || e->snap==0))
						blockRemoveLink(b, addr, p->l.type, p->l.tag, 0);
				}
				break;
			}
		}

		if(!ventiSend(p->a, b, data)){
			p->nfailsend++;
			ret = ArchFailure;
			goto Out;
		}
		p->nsend++;
		if(data != b->data)
			p->nfake++;
		if(data == b->data){	/* not faking it, so update state */
			p->nreal++;
			l = b->l;
			l.state |= BsVenti;
			if(!blockSetLabel(b, &l, 0)){
				ret = ArchFailure;
				goto Out;
			}
		}
	}

	shaBlock(p->score, b, data, p->blockSize);
if(0) fprint(2, "ventisend %V %p %p %p\n", p->score, data, b->data, w.data);
	ret = data!=b->data ? ArchFaked : ArchSuccess;
	p->l = b->l;
Out:
	if(data != b->data)
		vtfree(data);
	p->depth--;
	blockPut(b);
	return ret;
}

static void
archThread(void *v)
{
	Arch *a = v;
	Block *b;
	Param p;
	Super super;
	int ret;
	u32int addr;
	uchar rbuf[VtRootSize];
	VtRoot root;

	threadsetname("arch");

	for(;;){
		/* look for work */
		wlock(&a->fs->elk);
		b = superGet(a->c, &super);
		if(b == nil){
			wunlock(&a->fs->elk);
			fprint(2, "archThread: superGet: %r\n");
			sleep(60*1000);
			continue;
		}
		addr = super.next;
		if(addr != NilBlock && super.current == NilBlock){
			super.current = addr;
			super.next = NilBlock;
			superPack(&super, b->data);
			blockDirty(b);
		}else
			addr = super.current;
		blockPut(b);
		wunlock(&a->fs->elk);

		if(addr == NilBlock){
			/* wait for work */
			qlock(&a->lk);
			rsleep(&a->starve);
			if(a->die.l != nil)
				goto Done;
			qunlock(&a->lk);
			continue;
		}

sleep(10*1000);	/* window of opportunity to provoke races */

		/* do work */
		memset(&p, 0, sizeof p);
		p.blockSize = a->blockSize;
		p.dsize = 3*VtEntrySize;	/* root has three Entries */
		p.c = a->c;
		p.a = a;

		ret = archWalk(&p, addr, BtDir, RootTag);
		switch(ret){
		default:
			abort();
		case ArchFailure:
			fprint(2, "archiveBlock %#ux: %r\n", addr);
			sleep(60*1000);
			continue;
		case ArchSuccess:
		case ArchFaked:
			break;
		}

		if(0) fprint(2, "archiveSnapshot 0x%#ux: maxdepth %ud nfixed %ud"
			" send %ud nfailsend %ud nvisit %ud"
			" nreclaim %ud nfake %ud nreal %ud\n",
			addr, p.maxdepth, p.nfixed,
			p.nsend, p.nfailsend, p.nvisit,
			p.nreclaim, p.nfake, p.nreal);
		if(0) fprint(2, "archiveBlock %V (%ud)\n", p.score, p.blockSize);

		/* tie up vac root */
		memset(&root, 0, sizeof root);
		strecpy(root.type, root.type+sizeof root.type, "vac");
		strecpy(root.name, root.name+sizeof root.name, "fossil");
		memmove(root.score, p.score, VtScoreSize);
		memmove(root.prev, super.last, VtScoreSize);
		root.blocksize = a->blockSize;
		vtrootpack(&root, rbuf);
		if(vtwrite(a->z, p.score, VtRootType, rbuf, VtRootSize) < 0
		|| vtsha1check(p.score, rbuf, VtRootSize) < 0){
			fprint(2, "vtWriteBlock %#ux: %r\n", addr);
			sleep(60*1000);
			continue;
		}

		/* record success */
		wlock(&a->fs->elk);
		b = superGet(a->c, &super);
		if(b == nil){
			wunlock(&a->fs->elk);
			fprint(2, "archThread: superGet: %r\n");
			sleep(60*1000);
			continue;
		}
		super.current = NilBlock;
		memmove(super.last, p.score, VtScoreSize);
		superPack(&super, b->data);
		blockDirty(b);
		blockPut(b);
		wunlock(&a->fs->elk);

		consPrint("archive vac:%V\n", p.score);
	}

Done:
	a->ref--;
	rwakeup(&a->die);
	qunlock(&a->lk);
}

void
archKick(Arch *a)
{
	if(a == nil){
		fprint(2, "warning: archKick nil\n");
		return;
	}
	qlock(&a->lk);
	rwakeup(&a->starve);
	qunlock(&a->lk);
}
