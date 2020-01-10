#include <u.h>
#include <libc.h>
#include <diskfs.h>
#include <venti.h>

extern void vtlibthread(void);

typedef struct DiskVenti DiskVenti;
struct DiskVenti
{
	Disk disk;
	VtEntry e;
	VtCache *c;
};

extern int nfilereads;
extern void _nfilereads_darwin_sucks(void);

/*
 * This part is like file.c but doesn't require storing the root block
 * in the cache permanently and doesn't care about locking since
 * all the blocks are read-only.  Perhaps at some point this functionality
 * should go into libvac in some form.
 */
static int
vtfileindices(VtEntry *e, u32int bn, int *index)
{
	int i, np;

	memset(index, 0, VtPointerDepth*sizeof(int));

	np = e->psize/VtScoreSize;
	for(i=0; bn > 0; i++){
		if(i >= VtPointerDepth){
			werrstr("bad block number %lud", (ulong)bn);
			return -1;
		}
		index[i] = bn % np;
		bn /= np;
	}
	return i;
}

VtBlock *_vtfileblock(VtCache*, VtEntry*, u32int);	/* avoid auto-inline by putting later in file */
static void
diskventiblockput(Block *b)
{
	vtblockput(b->priv);
	free(b);
}

static Block*
diskventiread(Disk *dd, u32int len, u64int offset)
{
	DiskVenti *d = (DiskVenti*)dd;
	VtBlock *vb;
	Block *b;
	int frag;

nfilereads++;
	vb = _vtfileblock(d->c, &d->e, offset/d->e.dsize);
	if(vb == nil)
		return nil;

	b = mallocz(sizeof(Block), 1);
	if(b == nil){
		vtblockput(vb);
		return nil;
	}

	b->priv = vb;
	b->_close = diskventiblockput;
	frag = offset%d->e.dsize;
	b->data = (uchar*)vb->data + frag;
	b->len = d->e.dsize - frag;
	if(b->len > len)
		b->len = len;
	return b;
}

VtBlock*
_vtfileblock(VtCache *c, VtEntry *e, u32int bn)
{
	VtBlock *b;
	int i, d, index[VtPointerDepth+1], t;
	uchar score[VtScoreSize];

	i = vtfileindices(e, bn, index);
	if(i < 0)
		return nil;
	d = (e->type&VtTypeDepthMask);
	if(i > d){
		werrstr("bad address %d > %d (%x %x)", i, d, e->type, e->flags);
		return nil;
	}

/*fprint(2, "vtread %V\n", e->score); */
	b = vtcacheglobal(c, e->score, e->type, d == 0 ? e->dsize : e->psize);
	for(i=d-1; i>=0 && b; i--){
		t = VtDataType+i;
/*fprint(2, "vtread %V\n", b->data+index[i]*VtScoreSize); */
		memmove(score, b->data+index[i]*VtScoreSize, VtScoreSize);
		vtblockput(b);
		b = vtcacheglobal(c, score, t, i == 0 ? e->dsize : e->psize);
	}
	return b;
}

static void
diskventiclose(Disk *dd)
{
	DiskVenti *d = (DiskVenti*)dd;
	free(d);
}

Disk*
diskopenventi(VtCache *c, uchar score[VtScoreSize])
{
	DiskVenti *d;
	VtEntry e;
	VtRoot root;
	VtBlock *b;

	if((b = vtcacheglobal(c, score, VtRootType, VtRootSize)) == nil)
		goto Err;
	if(vtrootunpack(&root, b->data) < 0)
		goto Err;
	if(root.blocksize < 512 || (root.blocksize&(root.blocksize-1))){
		werrstr("bad blocksize %d", root.blocksize);
		goto Err;
	}
	vtblockput(b);

	if((b = vtcacheglobal(c, root.score, VtDirType, VtEntrySize)) == nil)
		goto Err;
	if(vtentryunpack(&e, b->data, 0) < 0)
		goto Err;
	vtblockput(b);
	b = nil;
	if((e.type&VtTypeBaseMask) != VtDataType){
		werrstr("not a single file");
		goto Err;
	}

	d = mallocz(sizeof(DiskVenti), 1);
	if(d == nil)
		goto Err;

	d->disk._read = diskventiread;
	d->disk._close = diskventiclose;
	d->e = e;
	d->c = c;
	return &d->disk;

Err:
	if(b)
		vtblockput(b);

	_nfilereads_darwin_sucks();  /* force Darwin ld to pull in file.o */
	return nil;
}
