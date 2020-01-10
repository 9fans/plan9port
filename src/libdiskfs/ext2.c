#include <u.h>
#include <libc.h>
#include <thread.h>
#include <sunrpc.h>
#include <nfs3.h>
#include <diskfs.h>
#include "ext2.h"

static void parsedirent(Dirent*, uchar*);
static void parseinode(Inode*, uchar*);
static void parsegroup(Group*, uchar*);
static void parsesuper(Super*, uchar*);

#define debug 0

static int ext2sync(Fsys*);
static void ext2close(Fsys*);
static Block* ext2blockread(Fsys*, u64int);

static Nfs3Status ext2root(Fsys*, Nfs3Handle*);
static Nfs3Status ext2getattr(Fsys*, SunAuthUnix *au, Nfs3Handle*, Nfs3Attr*);
static Nfs3Status ext2lookup(Fsys*, SunAuthUnix *au, Nfs3Handle*, char*, Nfs3Handle*);
static Nfs3Status ext2readfile(Fsys*, SunAuthUnix *au, Nfs3Handle*, u32int, u64int, uchar**, u32int*, u1int*);
static Nfs3Status ext2readlink(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, char **link);
static Nfs3Status ext2readdir(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, u32int, u64int, uchar**, u32int*, u1int*);
static Nfs3Status ext2access(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, u32int want, u32int *got, Nfs3Attr *attr);

Fsys*
fsysopenext2(Disk *disk)
{
	Ext2 *fs;
	Fsys *fsys;

	fsys = emalloc(sizeof(Fsys));
	fs = emalloc(sizeof(Ext2));
	fs->disk = disk;
	fsys->priv = fs;
	fs->fsys = fsys;
	fsys->type = "ext2";
	fsys->_readblock = ext2blockread;
	fsys->_sync = ext2sync;
	fsys->_root = ext2root;
	fsys->_getattr = ext2getattr;
	fsys->_access = ext2access;
	fsys->_lookup = ext2lookup;
	fsys->_readfile = ext2readfile;
	fsys->_readlink = ext2readlink;
	fsys->_readdir = ext2readdir;
	fsys->_close = ext2close;

	if(ext2sync(fsys) < 0)
		goto error;

	return fsys;

error:
	ext2close(fsys);
	return nil;
}

static void
ext2close(Fsys *fsys)
{
	Ext2 *fs;

	fs = fsys->priv;
	free(fs);
	free(fsys);
}

static int
ext2group(Ext2 *fs, u32int i, Group *g)
{
	Block *b;
	u64int addr;

	if(i >= fs->ngroup)
		return -1;

	addr = fs->groupaddr + i/fs->descperblock;
	b = diskread(fs->disk, fs->blocksize, addr*fs->blocksize);
	if(b == nil)
		return -1;
	parsegroup(g, b->data+i%fs->descperblock*GroupSize);
	blockput(b);
	return 0;
}

static Block*
ext2blockread(Fsys *fsys, u64int vbno)
{
	Block *bitb;
	Group g;
	uchar *bits;
	u32int bno, boff, bitblock;
	u64int bitpos;
	Ext2 *fs;

	fs = fsys->priv;
	if(vbno >= fs->nblock)
		return nil;
	bno = vbno;
	if(bno != vbno)
		return nil;

/*
	if(bno < fs->firstblock)
		return diskread(fs->disk, fs->blocksize, (u64int)bno*fs->blocksize);
*/
	if(bno < fs->firstblock)
		return nil;

	bno -= fs->firstblock;
	if(ext2group(fs, bno/fs->blockspergroup, &g) < 0){
		if(debug)
			fprint(2, "loading group: %r...");
		return nil;
	}
/*
	if(debug)
		fprint(2, "ext2 group %d: bitblock=%ud inodebitblock=%ud inodeaddr=%ud freeblocks=%ud freeinodes=%ud useddirs=%ud\n",
			(int)(bno/fs->blockspergroup),
			g.bitblock,
			g.inodebitblock,
			g.inodeaddr,
			g.freeblockscount,
			g.freeinodescount,
			g.useddirscount);
	if(debug)
		fprint(2, "group %d bitblock=%d...", bno/fs->blockspergroup, g.bitblock);
*/
	bitblock = g.bitblock;
	bitpos = (u64int)bitblock*fs->blocksize;

	if((bitb = diskread(fs->disk, fs->blocksize, bitpos)) == nil){
		if(debug)
			fprint(2, "loading bitblock: %r...");
		return nil;
	}
	bits = bitb->data;
	boff = bno%fs->blockspergroup;
	if((bits[boff>>3] & (1<<(boff&7))) == 0){
		if(debug)
			fprint(2, "block %d not allocated in group %d: bitblock %d/%lld bits[%d] = %#x\n",
				boff, bno/fs->blockspergroup,
				(int)bitblock,
				bitpos,
				boff>>3,
				bits[boff>>3]);
		blockput(bitb);
		return nil;
	}
	blockput(bitb);

	bno += fs->firstblock;
	return diskread(fs->disk, fs->blocksize, (u64int)bno*fs->blocksize);
}

static Block*
ext2datablock(Ext2 *fs, u32int bno, int size)
{
	USED(size);
	return ext2blockread(fs->fsys, bno);
}

static Block*
ext2fileblock(Ext2 *fs, Inode *ino, u32int bno, int size)
{
	int ppb;
	Block *b;
	u32int *a;
	u32int obno, pbno;

	obno = bno;
	if(bno < NDIRBLOCKS){
		if(debug)
			fprint(2, "fileblock %d -> %d...",
				bno, ino->block[bno]);
		return ext2datablock(fs, ino->block[bno], size);
	}
	bno -= NDIRBLOCKS;
	ppb = fs->blocksize/4;

	/* one indirect */
	if(bno < ppb){
		b = ext2datablock(fs, ino->block[INDBLOCK], fs->blocksize);
		if(b == nil)
			return nil;
		a = (u32int*)b->data;
		bno = a[bno];
		blockput(b);
		return ext2datablock(fs, bno, size);
	}
	bno -= ppb;

	/* one double indirect */
	if(bno < ppb*ppb){
		b = ext2datablock(fs, ino->block[DINDBLOCK], fs->blocksize);
		if(b == nil)
			return nil;
		a = (u32int*)b->data;
		pbno = a[bno/ppb];
		bno = bno%ppb;
		blockput(b);
		b = ext2datablock(fs, pbno, fs->blocksize);
		if(b == nil)
			return nil;
		a = (u32int*)b->data;
		bno = a[bno];
		blockput(b);
		return ext2datablock(fs, bno, size);
	}
	bno -= ppb*ppb;

	/* one triple indirect */
	if(bno < ppb*ppb*ppb){
		b = ext2datablock(fs, ino->block[TINDBLOCK], fs->blocksize);
		if(b == nil)
			return nil;
		a = (u32int*)b->data;
		pbno = a[bno/(ppb*ppb)];
		bno = bno%(ppb*ppb);
		blockput(b);
		b = ext2datablock(fs, pbno, fs->blocksize);
		if(b == nil)
			return nil;
		a = (u32int*)b->data;
		pbno = a[bno/ppb];
		bno = bno%ppb;
		blockput(b);
		b = ext2datablock(fs, pbno, fs->blocksize);
		if(b == nil)
			return nil;
		a = (u32int*)b->data;
		bno = a[bno];
		blockput(b);
		return ext2datablock(fs, bno, size);
	}

	fprint(2, "ext2fileblock %ud: too big\n", obno);
	return nil;
}

static int
checksuper(Super *super)
{
	if(super->magic != SUPERMAGIC){
		werrstr("bad magic 0x%ux wanted 0x%ux", super->magic, SUPERMAGIC);
		return -1;
	}
	return 0;
}

static int
ext2sync(Fsys *fsys)
{
	int i;
	Group g;
	Block *b;
	Super super;
	Ext2 *fs;
	Disk *disk;

	fs = fsys->priv;
	disk = fs->disk;
	if((b = diskread(disk, SBSIZE, SBOFF)) == nil)
		return -1;
	parsesuper(&super, b->data);
	blockput(b);
	if(checksuper(&super) < 0)
		return -1;
	fs->blocksize = MINBLOCKSIZE<<super.logblocksize;
	fs->nblock = super.nblock;
	fs->ngroup = (super.nblock+super.blockspergroup-1)
		/ super.blockspergroup;
	fs->inospergroup = super.inospergroup;
	fs->blockspergroup = super.blockspergroup;
	if(super.revlevel >= 1)
		fs->inosize = super.inosize;
	else
		fs->inosize = 128;
	fs->inosperblock = fs->blocksize / fs->inosize;
	if(fs->blocksize == SBOFF)
		fs->groupaddr = 2;
	else
		fs->groupaddr = 1;
	fs->descperblock = fs->blocksize / GroupSize;
	fs->firstblock = super.firstdatablock;

	fsys->blocksize = fs->blocksize;
	fsys->nblock = fs->nblock;
	if(debug) fprint(2, "ext2 %d %d-byte blocks, first data block %d, %d groups of %d\n",
		fs->nblock, fs->blocksize, fs->firstblock, fs->ngroup, fs->blockspergroup);

	if(0){
		for(i=0; i<fs->ngroup; i++)
			if(ext2group(fs, i, &g) >= 0)
				fprint(2, "grp %d: bitblock=%d\n", i, g.bitblock);
	}
	return 0;
}

static void
mkhandle(Nfs3Handle *h, u64int ino)
{
	h->h[0] = ino>>24;
	h->h[1] = ino>>16;
	h->h[2] = ino>>8;
	h->h[3] = ino;
	h->len = 4;
}

static u32int
byte2u32(uchar *p)
{
	return (p[0]<<24) | (p[1]<<16) | (p[2]<<8) | p[3];
}

static Nfs3Status
handle2ino(Ext2 *fs, Nfs3Handle *h, u32int *pinum, Inode *ino)
{
	int i;
	uint ioff;
	u32int inum;
	u32int addr;
	Block *b;
	Group g;

	if(h->len != 4)
		return Nfs3ErrBadHandle;
	inum = byte2u32(h->h);
	if(pinum)
		*pinum = inum;
	i = (inum-1) / fs->inospergroup;
	if(i >= fs->ngroup)
		return Nfs3ErrBadHandle;
	ioff = (inum-1) % fs->inospergroup;
	if(ext2group(fs, i, &g) < 0)
		return Nfs3ErrIo;
	addr = g.inodeaddr + ioff/fs->inosperblock;
	if((b = diskread(fs->disk, fs->blocksize, (u64int)addr*fs->blocksize)) == nil)
		return Nfs3ErrIo;
	parseinode(ino, b->data+fs->inosize*(ioff%fs->inosperblock));
	blockput(b);
	return Nfs3Ok;
}

static Nfs3Status
ext2root(Fsys *fsys, Nfs3Handle *h)
{
	USED(fsys);
	mkhandle(h, ROOTINODE);
	return Nfs3Ok;
}

static u64int
inosize(Inode* ino)
{
	u64int size;

	size = ino->size;
	if((ino->mode&IFMT)==IFREG)
		size |= (u64int)ino->diracl << 32;
	return size;
}

static Nfs3Status
ino2attr(Ext2 *fs, Inode *ino, u32int inum, Nfs3Attr *attr)
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
	attr->size = inosize(ino);
	attr->used = (u64int)ino->nblock*fs->blocksize;
	if(attr->type==Nfs3FileBlock || attr->type==Nfs3FileChar){
		rdev = ino->block[0];
		attr->major = (rdev>>8)&0xFF;
		attr->minor = rdev & 0xFFFF00FF;
	}else{
		attr->major = 0;
		attr->minor = 0;
	}
	attr->fsid = 0;
	attr->fileid = inum;
	attr->atime.sec = ino->atime;
	attr->atime.nsec = 0;
	attr->mtime.sec = ino->mtime;
	attr->mtime.nsec = 0;
	attr->ctime.sec = ino->ctime;
	attr->ctime.nsec = 0;
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

	if(allowall)
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
ext2getattr(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, Nfs3Attr *attr)
{
	Inode ino;
	u32int inum;
	Ext2 *fs;
	Nfs3Status ok;

	fs = fsys->priv;
	if((ok = handle2ino(fs, h, &inum, &ino)) != Nfs3Ok)
		return ok;

	USED(au);	/* anyone can getattr */
	return ino2attr(fs, &ino, inum, attr);
}

static Nfs3Status
ext2access(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, u32int want, u32int *got, Nfs3Attr *attr)
{
	int have;
	Inode ino;
	u32int inum;
	Ext2 *fs;
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
ext2lookup(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, char *name, Nfs3Handle *nh)
{
	u32int nblock;
	u32int i;
	uchar *p, *ep;
	Dirent de;
	Inode ino;
	Block *b;
	Ext2 *fs;
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
	if(debug) fprint(2, "%d blocks in dir...", nblock);
	for(i=0; i<nblock; i++){
		if(i==nblock-1)
			want = ino.size % fs->blocksize;
		else
			want = fs->blocksize;
		b = ext2fileblock(fs, &ino, i, want);
		if(b == nil){
			if(debug) fprint(2, "empty block...");
			continue;
		}
		p = b->data;
		ep = p+b->len;
		while(p < ep){
			parsedirent(&de, p);
			if(de.reclen == 0){
				if(debug)
					fprint(2, "reclen 0 at offset %d of %d\n", (int)(p-b->data), b->len);
				break;
			}
			p += de.reclen;
			if(p > ep){
				if(debug)
					fprint(2, "bad len %d at offset %d of %d\n", de.reclen, (int)(p-b->data), b->len);
				break;
			}
			if(de.ino == 0)
				continue;
			if(4+2+2+de.namlen > de.reclen){
				if(debug)
					fprint(2, "bad namelen %d at offset %d of %d\n", de.namlen, (int)(p-b->data), b->len);
				break;
			}
			if(de.namlen == len && memcmp(de.name, name, len) == 0){
				mkhandle(nh, de.ino);
				blockput(b);
				return Nfs3Ok;
			}
		}
		blockput(b);
	}
	return Nfs3ErrNoEnt;
}

static Nfs3Status
ext2readdir(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, u32int count, u64int cookie, uchar **pdata, u32int *pcount, u1int *peof)
{
	u32int nblock;
	u32int i;
	int off, outofspace;
	uchar *data, *dp, *dep, *p, *ep, *ndp;
	Dirent de;
	Inode ino;
	Block *b;
	Ext2 *fs;
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

	if(debug) print("readdir cookie %#llux ino.size %#llux\n",
		(u64int)cookie, (u64int)ino.size);

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
	outofspace = 0;
	for(; i<nblock && !outofspace; i++, off=0){
		if(i==nblock-1)
			want = ino.size % fs->blocksize;
		else
			want = fs->blocksize;
		b = ext2fileblock(fs, &ino, i, want);
		if(b == nil)
			continue;
		p = b->data;
		ep = p+b->len;
		memset(&e, 0, sizeof e);
		while(p < ep){
			parsedirent(&de, p);
			if(de.reclen == 0){
				if(debug) fprint(2, "reclen 0 at offset %d of %d\n", (int)(p-b->data), b->len);
				break;
			}
			p += de.reclen;
			if(p > ep){
				if(debug) fprint(2, "reclen %d at offset %d of %d\n", de.reclen, (int)(p-b->data), b->len);
				break;
			}
			if(de.ino == 0){
				if(debug) fprint(2, "zero inode\n");
				continue;
			}
			if(4+2+2+de.namlen > de.reclen){
				if(debug) fprint(2, "bad namlen %d reclen %d at offset %d of %d\n", de.namlen, de.reclen, (int)(p-b->data), b->len);
				break;
			}
			if(debug) print("%.*s/%d ", de.namlen, de.name, (int)de.ino);
			if(p-de.reclen - b->data < off)
				continue;
			e.fileid = de.ino;
			e.name = de.name;
			e.namelen = de.namlen;
			e.cookie = (u64int)i*fs->blocksize + (p - b->data);
			if(debug) print("%.*s %#llux\n", utfnlen(e.name, e.namelen), e.name, (u64int)e.cookie);
			if(nfs3entrypack(dp, dep, &ndp, &e) < 0){
				outofspace = 1;
				break;
			}
			dp = ndp;
		}
		blockput(b);
	}
	if(i==nblock && !outofspace)
		*peof = 1;

	*pcount = dp - data;
	*pdata = data;
	return Nfs3Ok;
}

static Nfs3Status
ext2readfile(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, u32int count,
	u64int offset, uchar **pdata, u32int *pcount, u1int *peof)
{
	uchar *data;
	Block *b;
	Ext2 *fs;
	int skip1, tot, want, fragcount;
	Inode ino;
	Nfs3Status ok;
	u64int size;

	fs = fsys->priv;
	if((ok = handle2ino(fs, h, nil, &ino)) != Nfs3Ok)
		return ok;

	if((ok = inoperm(&ino, au, AREAD)) != Nfs3Ok)
		return ok;

	size = inosize(&ino);
	if(offset >= size){
		*pdata = 0;
		*pcount = 0;
		*peof = 1;
		return Nfs3Ok;
	}
	if(offset+count > size)
		count = size-offset;

	data = malloc(count);
	if(data == nil)
		return Nfs3ErrNoMem;
	memset(data, 0, count);

	skip1 = offset%fs->blocksize;
	offset -= skip1;
	want = skip1+count;

	/*
	 * have to read multiple blocks if we get asked for a big read.
	 * Linux NFS client assumes that if you ask for 8k and only get 4k
	 * back, the remaining 4k is zeros.
	 */
	for(tot=0; tot<want; tot+=fragcount){
		b = ext2fileblock(fs, &ino, (offset+tot)/fs->blocksize, fs->blocksize);
		fragcount = fs->blocksize;
		if(b == nil)
			continue;
		if(tot+fragcount > want)
			fragcount = want - tot;
		if(tot == 0)
			memmove(data, b->data+skip1, fragcount-skip1);
		else
			memmove(data+tot-skip1, b->data, fragcount);
		blockput(b);
	}
	count = tot - skip1;

	*peof = (offset+count == size);
	*pcount = count;
	*pdata = data;
	return Nfs3Ok;
}

static Nfs3Status
ext2readlink(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, char **link)
{
	Ext2 *fs;
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
		/* BUG: assumes symlink fits in one block */
		b = ext2fileblock(fs, &ino, 0, len);
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

	if(len > sizeof ino.block)
		return Nfs3ErrIo;

	*link = malloc(len+1);
	if(*link == 0)
		return Nfs3ErrNoMem;
	memmove(*link, ino.block, ino.size);
	(*link)[len] = 0;
	return Nfs3Ok;
}

/*
 * Ext2 is always little-endian, even on big-endian machines.
 */

static u32int
l32(uchar *p)
{
	return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
}

static u16int
l16(uchar *p)
{
	return p[0] | (p[1]<<8);
}

static u8int
l8(uchar *p)
{
	return p[0];
}

static void
parsedirent(Dirent *de, uchar *p)
{
	de->ino = l32(p);
	de->reclen = l16(p+4);
	de->namlen = l8(p+6);
	/* 1 byte pad */
	de->name = (char*)p+8;
}

static void
parseinode(Inode *ino, uchar *p)
{
	int i;

	ino->mode = l16(p);
	ino->uid = l16(p+2);
	ino->size = l32(p+4);
	ino->atime = l32(p+8);
	ino->ctime = l32(p+12);
	ino->mtime = l32(p+16);
	ino->dtime = l32(p+20);
	ino->gid = l16(p+24);
	ino->nlink = l16(p+26);
	ino->nblock = l32(p+28);
	ino->flags = l32(p+32);
	/* 4 byte osd1 */
	for(i=0; i<NBLOCKS; i++)
		ino->block[i] = l32(p+40+i*4);
	ino->version = l32(p+100);
	ino->fileacl = l32(p+104);
	ino->diracl = l32(p+108);
	ino->faddr = l32(p+112);
	/* 12 byte osd2 */
}

static void
parsegroup(Group *g, uchar *p)
{
	g->bitblock = l32(p);
	g->inodebitblock = l32(p+4);
	g->inodeaddr = l32(p+8);
	g->freeblockscount = l16(p+12);
	g->freeinodescount = l16(p+14);
	g->useddirscount = l16(p+16);
	/* 2 byte pad */
	/* 12 byte reserved */
}

static void
parsesuper(Super *s, uchar *p)
{
	s->ninode = l32(p);
	s->nblock = l32(p+4);
	s->rblockcount = l32(p+8);
	s->freeblockcount = l32(p+12);
	s->freeinodecount = l32(p+16);
	s->firstdatablock = l32(p+20);
	s->logblocksize = l32(p+24);
	s->logfragsize = l32(p+28);
	s->blockspergroup = l32(p+32);
	s->fragpergroup = l32(p+36);
	s->inospergroup = l32(p+40);
	s->mtime = l32(p+44);
	s->wtime = l32(p+48);
	s->mntcount = l16(p+52);
	s->maxmntcount = l16(p+54);
	s->magic = l16(p+56);
	s->state = l16(p+58);
	s->errors = l16(p+60);
	/* 2 byte pad */
	s->lastcheck = l32(p+64);
	s->checkinterval = l32(p+68);
	s->creatoros = l32(p+72);
	s->revlevel = l32(p+76);
	s->defresuid = l16(p+80);
	s->defresgid = l16(p+82);
	s->firstino = l32(p+84);
	s->inosize = l32(p+88);
	s->blockgroupnr = l16(p+60);
	/* 932 byte reserved */
}
