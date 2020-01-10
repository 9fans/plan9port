#include <u.h>
#include <libc.h>
#include <thread.h>
#include <sunrpc.h>
#include <nfs3.h>
#include <diskfs.h>
#include "ffs.h"

#define BADBNO	((u64int)~0ULL)

#define checkcg 0
#define debug 0

static int checkfsblk(Fsblk*);
static int checkcgblk(Cgblk*);
static Block *ffsblockread(Fsys*, u64int);
static int ffssync(Fsys*);
static void ffsclose(Fsys*);

static u64int ffsxfileblock(Fsys *fs, Nfs3Handle *h, u64int offset);
static Nfs3Status ffsroot(Fsys*, Nfs3Handle*);
static Nfs3Status ffsgetattr(Fsys*, SunAuthUnix *au, Nfs3Handle*, Nfs3Attr*);
static Nfs3Status ffslookup(Fsys*, SunAuthUnix *au, Nfs3Handle*, char*, Nfs3Handle*);
static Nfs3Status ffsreadfile(Fsys*, SunAuthUnix *au, Nfs3Handle*, u32int, u64int, uchar**, u32int*, u1int*);
static Nfs3Status ffsreadlink(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, char **link);
static Nfs3Status ffsreaddir(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, u32int, u64int, uchar**, u32int*, u1int*);
static Nfs3Status ffsaccess(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, u32int want, u32int *got, Nfs3Attr *attr);

Fsys*
fsysopenffs(Disk *disk)
{
	Ffs *fs;
	Fsys *fsys;

	fsys = emalloc(sizeof(Fsys));
	fs = emalloc(sizeof(Ffs));
	fs->disk = disk;
	fsys->priv = fs;
	fsys->type = "ffs";
	fsys->_readblock = ffsblockread;
	fsys->_sync = ffssync;
	fsys->_root = ffsroot;
	fsys->_getattr = ffsgetattr;
	fsys->_access = ffsaccess;
	fsys->_lookup = ffslookup;
	fsys->_readfile = ffsreadfile;
	fsys->_readlink = ffsreadlink;
	fsys->_readdir = ffsreaddir;
	fsys->_close = ffsclose;
	fsys->fileblock = ffsxfileblock;

	if(ffssync(fsys) < 0)
		goto error;

	return fsys;

error:
	ffsclose(fsys);
	return nil;
}

static Cgblk*
ffscylgrp(Ffs *fs, u32int i, Block **pb)
{
	Block *b;
	Cgblk *cg;

	if(i >= fs->ncg)
		return nil;

	b = diskread(fs->disk, fs->blocksize, (u64int)fs->cg[i].cgblkno*fs->blocksize);
	if(b == nil)
		return nil;
	cg = (Cgblk*)b->data;
	if(checkcgblk(cg) < 0){
fprint(2, "checkcgblk %d %lud: %r\n", i, (ulong)fs->cg[i].cgblkno);
		blockput(b);
		return nil;
	}
	*pb = b;
	return cg;
}

static int
ffssync(Fsys *fsys)
{
	int i;
	int off[] = { SBOFF, SBOFF2, SBOFFPIGGY };
	Block *b, *cgb;
	Cgblk *cgblk;
	Cylgrp *cg;
	Disk *disk;
	Ffs *fs;
	Fsblk *fsblk;

	fs = fsys->priv;
	disk = fs->disk;

	/*
	 * Read super block.
	 */
	b = nil;
	for(i=0; i<nelem(off); i++){
		if((b = diskread(disk, SBSIZE, off[i])) == nil)
			goto error;
		fsblk = (Fsblk*)b->data;
	//	fprint(2, "offset of magic: %ld\n", offsetof(Fsblk, magic));
		if((fs->ufs = checkfsblk(fsblk)) > 0)
			goto okay;
		blockput(b);
		b = nil;
	}
	goto error;

okay:
	fs->blocksize = fsblk->blocksize;
	fs->nblock = (fsblk->nfrag+fsblk->fragsperblock-1) / fsblk->fragsperblock;
	fs->fragsize = fsblk->fragsize;
	fs->fragspergroup = fsblk->fragspergroup;
	fs->fragsperblock = fsblk->fragsperblock;
	fs->inosperblock = fsblk->inosperblock;
	fs->inospergroup = fsblk->inospergroup;

	fs->nfrag = fsblk->nfrag;
	fs->ndfrag = fsblk->ndfrag;
	/*
	 * used to use
	 *	fs->blockspergroup = (u64int)fsblk->_cylspergroup *
	 *		fsblk->secspercyl * BYTESPERSEC / fsblk->blocksize;
	 * for UFS1, but this should work for both UFS1 and UFS2
	 */
	fs->blockspergroup = (u64int)fsblk->fragspergroup / fsblk->fragsperblock;
	fs->ncg = fsblk->ncg;

	fsys->blocksize = fs->blocksize;
	fsys->nblock = fs->nblock;

	if(debug) fprint(2, "ffs %lld %d-byte blocks, %d cylinder groups\n",
		fs->nblock, fs->blocksize, fs->ncg);
	if(debug) fprint(2, "\tinospergroup %d perblock %d blockspergroup %lld\n",
		fs->inospergroup, fs->inosperblock, fs->blockspergroup);

	if(fs->cg == nil)
		fs->cg = emalloc(fs->ncg*sizeof(Cylgrp));
	for(i=0; i<fs->ncg; i++){
		cg = &fs->cg[i];
		if(fs->ufs == 2)
			cg->bno = (u64int)fs->blockspergroup*i;
		else
			cg->bno = fs->blockspergroup*i + fsblk->_cgoffset * (i & ~fsblk->_cgmask);
		cg->cgblkno = cg->bno + fsblk->cfragno/fs->fragsperblock;
		cg->ibno = cg->bno + fsblk->ifragno/fs->fragsperblock;
		cg->dbno = cg->bno + fsblk->dfragno/fs->fragsperblock;

		if(checkcg){
			if((cgb = diskread(disk, fs->blocksize, (u64int)cg->cgblkno*fs->blocksize)) == nil)
				goto error;

			cgblk = (Cgblk*)cgb->data;
			if(checkcgblk(cgblk) < 0){
				blockput(cgb);
				goto error;
			}
			if(cgblk->nfrag % fs->fragsperblock && i != fs->ncg-1){
				werrstr("fractional number of blocks in non-last cylinder group %d", cgblk->nfrag);
				blockput(cgb);
				goto error;
			}
			/* cg->nfrag = cgblk->nfrag; */
			/* cg->nblock = (cgblk->nfrag+fs->fragsperblock-1) / fs->fragsperblock; */
			/* fprint(2, "cg #%d: cgblk %lud, %d blocks, %d inodes\n", cgblk->num, (ulong)cg->cgblkno, cg->nblock, cg->nino); */
		}
	}
	blockput(b);
	return 0;

error:
	blockput(b);
	return -1;
}

static void
ffsclose(Fsys *fsys)
{
	Ffs *fs;

	fs = fsys->priv;
	if(fs->cg)
		free(fs->cg);
	free(fs);
	free(fsys);
}

static int
checkfsblk(Fsblk *super)
{
// fprint(2, "ffs magic 0x%ux\n", super->magic);
	if(super->magic == FSMAGIC){
		super->time = super->_time;
		super->nfrag = super->_nfrag;
		super->ndfrag = super->_ndfrag;
		super->flags = super->_flags;
		return 1;
	}
	if(super->magic == FSMAGIC2){
		return 2;
	}

	werrstr("bad super block");
	return -1;
}

static int
checkcgblk(Cgblk *cg)
{
	if(cg->magic != CGMAGIC){
		werrstr("bad cylinder group block");
		return -1;
	}
	return 0;
}

/*
 * Read block #bno from the disk, zeroing unused data.
 * If there is no data whatsoever, it's okay to return nil.
 */
int nskipx;
static Block*
ffsblockread(Fsys *fsys, u64int bno)
{
	int i, o;
	u8int *fmap;
	int frag, fsize, avail;
	Block *b;
	Cgblk *cgblk;
	Ffs *fs;

	fs = fsys->priv;
	i = bno / fs->blockspergroup;
	o = bno % fs->blockspergroup;
	if(i >= fs->ncg)
		return nil;

	if((cgblk = ffscylgrp(fs, i, &b)) == nil)
		return nil;

	fmap = (u8int*)cgblk+cgblk->fmapoff;
	frag = fs->fragsperblock;
	switch(frag){
	default:
		sysfatal("bad frag");
	case 8:
		avail = fmap[o];
		break;
	case 4:
		avail = (fmap[o>>1] >> ((o&1)*4)) & 0xF;
		break;
	case 2:
		avail = (fmap[o>>2] >> ((o&3)*2)) & 0x3;
		break;
	case 1:
		avail = (fmap[o>>3] >> (o&7)) & 0x1;
		break;
	}
	blockput(b);

	if(avail == ((1<<frag)-1))
{
nskipx++;
		return nil;
}
	if((b = diskread(fs->disk, fs->blocksize, bno*fs->blocksize)) == nil){
		fprint(2, "diskread failed!!!\n");
		return nil;
	}

	fsize = fs->fragsize;
	for(i=0; i<frag; i++)
		if(avail & (1<<i))
			memset(b->data + fsize*i, 0, fsize);
	return b;
}

static Block*
ffsdatablock(Ffs *fs, u64int bno, int size)
{
	int fsize;
	u64int diskaddr;
	Block *b;

	if(bno == 0)
		return nil;

	fsize = size;
	if(fsize < fs->fragsize)
		fsize = fs->fragsize;

	if(bno >= fs->nfrag){
		fprint(2, "ffs: request for block %#lux; nfrag %#llux\n", (ulong)bno, fs->nfrag);
		return nil;
	}
	diskaddr = (u64int)bno*fs->fragsize;
	b = diskread(fs->disk, fsize, diskaddr);
	if(b == nil){
		fprint(2, "ffs: disk i/o at %#llux for %#ux: %r\n", diskaddr, fsize);
		return nil;
	}
	if(b->len < fsize){
		fprint(2, "ffs: disk i/o at %#llux for %#ux got %#ux\n", diskaddr, fsize,
			b->len);
		blockput(b);
		return nil;
	}

	return b;
}

static u64int
ifetch(Ffs *fs, u64int bno, u32int off)
{
	Block *b;

	if(bno == BADBNO)
		return BADBNO;
	b = ffsdatablock(fs, bno, fs->blocksize);
	if(b == nil)
		return BADBNO;
	if(fs->ufs == 2)
		bno = ((u64int*)b->data)[off];
	else
		bno = ((u32int*)b->data)[off];
	blockput(b);
	return bno;
}

static u64int
ffsfileblockno(Ffs *fs, Inode *ino, u64int bno)
{
	int ppb;

	if(bno < NDADDR){
		if(debug) fprint(2, "ffsfileblock %lud: direct %#lux\n", (ulong)bno, (ulong)ino->db[bno]);
		return ino->db[bno];
	}
	bno -= NDADDR;
	ppb = fs->blocksize/4;

	if(bno < ppb)	/* single indirect */
		return ifetch(fs, ino->ib[0], bno);
	bno -= ppb;

	if(bno < ppb*ppb)
		return ifetch(fs, ifetch(fs, ino->ib[1], bno/ppb), bno%ppb);
	bno -= ppb*ppb;

	if(bno/ppb/ppb/ppb == 0)	/* bno < ppb*ppb*ppb w/o overflow */
		return ifetch(fs, ifetch(fs, ifetch(fs, ino->ib[2], bno/ppb/ppb), (bno/ppb)%ppb), bno%ppb);

	fprint(2, "ffsfileblock %llud: way too big\n", bno+NDADDR+ppb+ppb*ppb);
	return BADBNO;
}

static Block*
ffsfileblock(Ffs *fs, Inode *ino, u64int bno, int size)
{
	u64int b;

	b = ffsfileblockno(fs, ino, bno);
	if(b == ~0)
		return nil;
	return ffsdatablock(fs, b, size);
}

/*
 * NFS handles are 4-byte inode number.
 */
static void
mkhandle(Nfs3Handle *h, u64int ino)
{
	h->h[0] = ino >> 24;
	h->h[1] = ino >> 16;
	h->h[2] = ino >> 8;
	h->h[3] = ino;
	h->len = 4;
}

static u32int
byte2u32(uchar *p)
{
	return (p[0]<<24) | (p[1]<<16) | (p[2]<<8) | p[3];
}

static u64int lastiaddr;	/* debugging */

static void
inode1to2(Inode1 *i1, Inode *i2)
{
	int i;

	memset(i2, 0, sizeof *i2);
	i2->mode = i1->mode;
	i2->nlink = i1->nlink;
	i2->size = i1->size;
	i2->atime = i1->atime;
	i2->atimensec = i1->atimensec;
	i2->mtime = i1->mtime;
	i2->mtimensec = i1->mtimensec;
	i2->ctime = i1->ctime;
	i2->ctimensec = i1->ctimensec;
	for(i=0; i<NDADDR; i++)
		i2->db[i] = i1->db[i];
	for(i=0; i<NIADDR; i++)
		i2->ib[i] = i1->ib[i];
	i2->flags = i1->flags;
	i2->nblock = i1->nblock;
	i2->gen = i1->gen;
	i2->uid = i1->uid;
	i2->gid = i1->gid;
}

static Nfs3Status
handle2ino(Ffs *fs, Nfs3Handle *h, u32int *pinum, Inode *ino)
{
	int i;
	u32int ioff;
	u32int inum;
	u64int iaddr;
	Block *b;
	Cylgrp *cg;
	Inode1 ino1;

	if(h->len != 4)
		return Nfs3ErrBadHandle;
	inum = byte2u32(h->h);
	if(pinum)
		*pinum = inum;
	if(debug) print("inum %d...", (int)inum);

	/* fetch inode from disk */
	i = inum / fs->inospergroup;
	ioff = inum % fs->inospergroup;
	if(debug)print("cg %d off %d...", i, (int)ioff);
	if(i >= fs->ncg)
		return Nfs3ErrBadHandle;
	cg = &fs->cg[i];

	if(debug) print("cg->ibno %lld ufs %d...", cg->ibno, fs->ufs);
	iaddr = (cg->ibno+ioff/fs->inosperblock)*(vlong)fs->blocksize;
	ioff = ioff%fs->inosperblock;
	if((b = diskread(fs->disk, fs->blocksize, iaddr)) == nil)
		return Nfs3ErrIo;
	if(fs->ufs == 2){
		*ino = ((Inode*)b->data)[ioff];
		lastiaddr = iaddr+ioff*sizeof(Inode);
	}else{
		ino1 = ((Inode1*)b->data)[ioff];
		inode1to2(&ino1, ino);
		lastiaddr = iaddr+ioff*sizeof(Inode1);
	}
	blockput(b);
	return Nfs3Ok;
}

static Nfs3Status
ffsroot(Fsys *fsys, Nfs3Handle *h)
{
	USED(fsys);
	mkhandle(h, 2);
	return Nfs3Ok;
}

static Nfs3Status
ino2attr(Ffs *fs, Inode *ino, u32int inum, Nfs3Attr *attr)
{
	u32int rdev;

	attr->type = -1;
	switch(ino->mode&IFMT){
	case IFIFO:
		attr->type = Nfs3FileFifo;
		break;
	case IFCHR:
		attr->type = Nfs3FileChar;
		break;
	case IFDIR:
		attr->type = Nfs3FileDir;
		break;
	case IFBLK:
		attr->type = Nfs3FileBlock;
		break;
	case IFREG:
		attr->type = Nfs3FileReg;
		break;
	case IFLNK:
		attr->type = Nfs3FileSymlink;
		break;
	case IFSOCK:
		attr->type = Nfs3FileSocket;
		break;
	case IFWHT:
	default:
		return Nfs3ErrBadHandle;
	}

	attr->mode = ino->mode&07777;
	attr->nlink = ino->nlink;
	attr->uid = ino->uid;
	attr->gid = ino->gid;
	attr->size = ino->size;
	attr->used = ino->nblock*fs->blocksize;
	if(attr->type==Nfs3FileBlock || attr->type==Nfs3FileChar){
		rdev = ino->db[0];
		attr->major = (rdev>>8)&0xFF;
		attr->minor = rdev & 0xFFFF00FF;
	}else{
		attr->major = 0;
		attr->minor = 0;
	}
	attr->fsid = 0;
	attr->fileid = inum;
	attr->atime.sec = ino->atime;
	attr->atime.nsec = ino->atimensec;
	attr->mtime.sec = ino->mtime;
	attr->mtime.nsec = ino->mtimensec;
	attr->ctime.sec = ino->ctime;
	attr->ctime.nsec = ino->ctimensec;
	return Nfs3Ok;
}

static int
ingroup(SunAuthUnix *au, uint gid)
{
	int i;

	for(i=0; i<au->ng; i++)
		if(au->g[i] == gid)
			return 1;
	return 0;
}

static Nfs3Status
inoperm(Inode *ino, SunAuthUnix *au, int need)
{
	int have;

	if(au == nil)
		return Nfs3Ok;

	have = ino->mode&0777;
	if(ino->uid == au->uid)
		have >>= 6;
	else if(ino->gid == au->gid || ingroup(au, ino->gid))
		have >>= 3;

	if((have&need) != need)
		return Nfs3ErrNotOwner;	/* really EPERM */
	return Nfs3Ok;
}

static Nfs3Status
ffsgetattr(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, Nfs3Attr *attr)
{
	Inode ino;
	u32int inum;
	Ffs *fs;
	Nfs3Status ok;

	fs = fsys->priv;
	if((ok = handle2ino(fs, h, &inum, &ino)) != Nfs3Ok)
		return ok;

	USED(au);	/* anyone can getattr */

	return ino2attr(fs, &ino, inum, attr);
}

static Nfs3Status
ffsaccess(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, u32int want, u32int *got, Nfs3Attr *attr)
{
	int have;
	Inode ino;
	u32int inum;
	Ffs *fs;
	Nfs3Status ok;

	fs = fsys->priv;
	if((ok = handle2ino(fs, h, &inum, &ino)) != Nfs3Ok)
		return ok;

	have = ino.mode&0777;
	if(ino.uid == au->uid)
		have >>= 6;
	else if(ino.gid == au->gid || ingroup(au, ino.gid))
		have >>= 3;

	*got = 0;
	if((want&Nfs3AccessRead) && (have&AREAD))
		*got |= Nfs3AccessRead;
	if((want&Nfs3AccessLookup) && (ino.mode&IFMT)==IFDIR && (have&AEXEC))
		*got |= Nfs3AccessLookup;
	if((want&Nfs3AccessExecute) && (ino.mode&IFMT)!=IFDIR && (have&AEXEC))
		*got |= Nfs3AccessExecute;

	return ino2attr(fs, &ino, inum, attr);
}

static Nfs3Status
ffslookup(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, char *name, Nfs3Handle *nh)
{
	u32int nblock;
	u32int i;
	uchar *p, *ep;
	Dirent *de;
	Inode ino;
	Block *b;
	Ffs *fs;
	Nfs3Status ok;
	int len, want;

	fs = fsys->priv;
	if((ok = handle2ino(fs, h, nil, &ino)) != Nfs3Ok)
		return ok;

	if((ino.mode&IFMT) != IFDIR)
		return Nfs3ErrNotDir;

	if((ok = inoperm(&ino, au, AEXEC)) != Nfs3Ok)
		return ok;

	len = strlen(name);
	nblock = (ino.size+fs->blocksize-1) / fs->blocksize;
	for(i=0; i<nblock; i++){
		if(i==nblock-1)
			want = ino.size % fs->blocksize;
		else
			want = fs->blocksize;
		b = ffsfileblock(fs, &ino, i, want);
		if(b == nil)
			continue;
		p = b->data;
		ep = p+b->len;
		while(p < ep){
			de = (Dirent*)p;
			if(de->reclen == 0){
				if(debug)
					fprint(2, "reclen 0 at offset %d of %d\n", (int)(p-b->data), b->len);
				break;
			}
			p += de->reclen;
			if(p > ep){
				if(debug)
					fprint(2, "bad len %d at offset %d of %d\n", de->reclen, (int)(p-b->data), b->len);
				break;
			}
			if(de->ino == 0)
				continue;
			if(4+2+2+de->namlen > de->reclen){
				if(debug)
					fprint(2, "bad namelen %d at offset %d of %d\n", de->namlen, (int)(p-b->data), b->len);
				break;
			}
			if(de->namlen == len && memcmp(de->name, name, len) == 0){
				mkhandle(nh, de->ino);
				blockput(b);
				return Nfs3Ok;
			}
		}
		blockput(b);
	}
	return Nfs3ErrNoEnt;
}

static Nfs3Status
ffsreaddir(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, u32int count, u64int cookie, uchar **pdata, u32int *pcount, u1int *peof)
{
	u32int nblock;
	u32int i;
	int off, done;
	uchar *data, *dp, *dep, *p, *ep, *ndp;
	Dirent *de;
	Inode ino;
	Block *b;
	Ffs *fs;
	Nfs3Status ok;
	Nfs3Entry e;
	int want;

	fs = fsys->priv;
	if((ok = handle2ino(fs, h, nil, &ino)) != Nfs3Ok)
		return ok;

	if((ino.mode&IFMT) != IFDIR)
		return Nfs3ErrNotDir;

	if((ok = inoperm(&ino, au, AREAD)) != Nfs3Ok)
		return ok;

	if(cookie >= ino.size){
		*peof = 1;
		*pcount = 0;
		*pdata = 0;
		return Nfs3Ok;
	}

	dp = malloc(count);
	data = dp;
	if(dp == nil)
		return Nfs3ErrNoMem;
	dep = dp+count;
	*peof = 0;
	nblock = (ino.size+fs->blocksize-1) / fs->blocksize;
	i = cookie/fs->blocksize;
	off = cookie%fs->blocksize;
	done = 0;
	for(; i<nblock && !done; i++){
		if(i==nblock-1)
			want = ino.size % fs->blocksize;
		else
			want = fs->blocksize;
		b = ffsfileblock(fs, &ino, i, want);
		if(b == nil)
			continue;
		p = b->data;
		ep = p+b->len;
		memset(&e, 0, sizeof e);
		while(p < ep){
			de = (Dirent*)p;
			if(de->reclen == 0){
				if(debug) fprint(2, "reclen 0 at offset %d of %d\n", (int)(p-b->data), b->len);
				break;
			}
			p += de->reclen;
			if(p > ep){
				if(debug) fprint(2, "reclen %d at offset %d of %d\n", de->reclen, (int)(p-b->data), b->len);
				break;
			}
			if(de->ino == 0){
				if(debug) fprint(2, "zero inode\n");
				continue;
			}
			if(4+2+2+de->namlen > de->reclen){
				if(debug) fprint(2, "bad namlen %d reclen %d at offset %d of %d\n", de->namlen, de->reclen, (int)(p-b->data), b->len);
				break;
			}
			if(de->name[de->namlen] != 0){
				if(debug) fprint(2, "bad name %d %.*s\n", de->namlen, de->namlen, de->name);
				continue;
			}
			if(debug) print("%s/%d ", de->name, (int)de->ino);
			if((uchar*)de - b->data < off)
				continue;
			e.fileid = de->ino;
			e.name = de->name;
			e.namelen = de->namlen;
			e.cookie = (u64int)i*fs->blocksize + (p - b->data);
			if(nfs3entrypack(dp, dep, &ndp, &e) < 0){
				done = 1;
				break;
			}
			dp = ndp;
		}
		off = 0;
		blockput(b);
	}
	if(i==nblock)
		*peof = 1;

	*pcount = dp - data;
	*pdata = data;
	return Nfs3Ok;
}

static u64int
ffsxfileblock(Fsys *fsys, Nfs3Handle *h, u64int offset)
{
	u64int bno;
	Inode ino;
	Nfs3Status ok;
	Ffs *fs;

	fs = fsys->priv;
	if((ok = handle2ino(fs, h, nil, &ino)) != Nfs3Ok){
		nfs3errstr(ok);
		return 0;
	}
	if(offset == 1)	/* clumsy hack for debugging */
		return lastiaddr;
	if(offset >= ino.size){
		werrstr("beyond end of file");
		return 0;
	}
	bno = offset/fs->blocksize;
	bno = ffsfileblockno(fs, &ino, bno);
	if(bno == ~0)
		return 0;
	return bno*(u64int)fs->fragsize;
}

static Nfs3Status
ffsreadfile(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, u32int count,
	u64int offset, uchar **pdata, u32int *pcount, u1int *peof)
{
	uchar *data;
	Block *b;
	Ffs *fs;
	int off, want, fragcount;
	Inode ino;
	Nfs3Status ok;

	fs = fsys->priv;
	if((ok = handle2ino(fs, h, nil, &ino)) != Nfs3Ok)
		return ok;

	if((ok = inoperm(&ino, au, AREAD)) != Nfs3Ok)
		return ok;

	if(offset >= ino.size){
		*pdata = 0;
		*pcount = 0;
		*peof = 1;
		return Nfs3Ok;
	}
	if(offset+count > ino.size)
		count = ino.size-offset;
	if(offset/fs->blocksize != (offset+count-1)/fs->blocksize)
		count = fs->blocksize - offset%fs->blocksize;

	data = malloc(count);
	if(data == nil)
		return Nfs3ErrNoMem;

	want = offset%fs->blocksize+count;
	if(want%fs->fragsize)
		want += fs->fragsize - want%fs->fragsize;

	b = ffsfileblock(fs, &ino, offset/fs->blocksize, want);
	if(b == nil){
		/* BUG: distinguish sparse file from I/O error */
		memset(data, 0, count);
	}else{
		off = offset%fs->blocksize;
		fragcount = count;	/* need signed variable */
		if(off+fragcount > b->len){
			fragcount = b->len - off;
			if(fragcount < 0)
				fragcount = 0;
		}
		if(fragcount > 0)
			memmove(data, b->data+off, fragcount);
		count = fragcount;
		blockput(b);
	}
	*peof = (offset+count == ino.size);
	*pcount = count;
	*pdata = data;
	return Nfs3Ok;
}

static Nfs3Status
ffsreadlink(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, char **link)
{
	Ffs *fs;
	Nfs3Status ok;
	int len;
	Inode ino;
	Block *b;

	fs = fsys->priv;
	if((ok = handle2ino(fs, h, nil, &ino)) != Nfs3Ok)
		return ok;
	if((ok = inoperm(&ino, au, AREAD)) != Nfs3Ok)
		return ok;

	if(ino.size > 1024)
		return Nfs3ErrIo;
	len = ino.size;

	if(ino.nblock != 0){
		/* assumes symlink fits in one block */
		b = ffsfileblock(fs, &ino, 0, len);
		if(b == nil)
			return Nfs3ErrIo;
		if(memchr(b->data, 0, len) != nil){
			blockput(b);
			return Nfs3ErrIo;
		}
		*link = malloc(len+1);
		if(*link == 0){
			blockput(b);
			return Nfs3ErrNoMem;
		}
		memmove(*link, b->data, len);
		(*link)[len] = 0;
		blockput(b);
		return Nfs3Ok;
	}

	if(len > sizeof ino.db + sizeof ino.ib)
		return Nfs3ErrIo;

	*link = malloc(len+1);
	if(*link == 0)
		return Nfs3ErrNoMem;
	memmove(*link, ino.db, ino.size);
	(*link)[len] = 0;
	return Nfs3Ok;
}
