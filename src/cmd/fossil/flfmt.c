#include "stdinc.h"
#include "dat.h"
#include "fns.h"
#include "flfmt9660.h"

#define blockWrite _blockWrite	/* hack */

static void usage(void);
static u64int fdsize(int fd);
static void partition(int fd, int bsize, Header *h);
static u64int unittoull(char *s);
static u32int blockAlloc(int type, u32int tag);
static void blockRead(int part, u32int addr);
static void blockWrite(int part, u32int addr);
static void superInit(char *label, u32int root, uchar[VtScoreSize]);
static void rootMetaInit(Entry *e);
static u32int rootInit(Entry *e);
static void topLevel(char *name);
static int parseScore(uchar[VtScoreSize], char*);
static u32int ventiRoot(char*, char*);
static VtSession *z;

#define TWID64	((u64int)~(u64int)0)

Disk *disk;
Fs *fs;
uchar *buf;
int bsize = 8*1024;
u64int qid = 1;
int iso9660off;
char *iso9660file;

int
confirm(char *msg)
{
	char buf[100];
	int n;

	fprint(2, "%s [y/n]: ", msg);
	n = read(0, buf, sizeof buf - 1);
	if(n <= 0)
		return 0;
	if(buf[0] == 'y')
		return 1;
	return 0;
}

void
main(int argc, char *argv[])
{
	int fd, force;
	Header h;
	ulong bn;
	Entry e;
	char *label = "vfs";
	char *host = nil;
	char *score = nil;
	u32int root;
	Dir *d;

	force = 0;
	ARGBEGIN{
	default:
		usage();
	case 'b':
		bsize = unittoull(EARGF(usage()));
		if(bsize == ~0)
			usage();
		break;
	case 'h':
		host = EARGF(usage());
		break;
	case 'i':
		iso9660file = EARGF(usage());
		iso9660off = atoi(EARGF(usage()));
		break;
	case 'l':
		label = EARGF(usage());
		break;
	case 'v':
		score = EARGF(usage());
		break;

	/*
	 * This is -y instead of -f because flchk has a
	 * (frequently used) -f option.  I type flfmt instead
	 * of flchk all the time, and want to make it hard
	 * to reformat my file system accidentally.
	 */
	case 'y':
		force = 1;
		break;
	}ARGEND

	if(argc != 1)
		usage();

	if(iso9660file && score)
		vtFatal("cannot use -i with -v");

	vtAttach();

	fmtinstall('V', scoreFmt);
	fmtinstall('R', vtErrFmt);
	fmtinstall('L', labelFmt);

	fd = open(argv[0], ORDWR);
	if(fd < 0)
		vtFatal("could not open file: %s: %r", argv[0]);

	buf = vtMemAllocZ(bsize);
	if(pread(fd, buf, bsize, HeaderOffset) != bsize)
		vtFatal("could not read fs header block: %r");

	if(headerUnpack(&h, buf) && !force
	&& !confirm("fs header block already exists; are you sure?"))
		goto Out;

	if((d = dirfstat(fd)) == nil)
		vtFatal("dirfstat: %r");

	if(d->type == 'M' && !force
	&& !confirm("fs file is mounted via devmnt (is not a kernel device); are you sure?"))
		goto Out;

	partition(fd, bsize, &h);
	headerPack(&h, buf);
	if(pwrite(fd, buf, bsize, HeaderOffset) < bsize)
		vtFatal("could not write fs header: %r");

	disk = diskAlloc(fd);
	if(disk == nil)
		vtFatal("could not open disk: %r");

	if(iso9660file)
		iso9660init(fd, &h, iso9660file, iso9660off);

	/* zero labels */
	memset(buf, 0, bsize);
	for(bn = 0; bn < diskSize(disk, PartLabel); bn++)
		blockWrite(PartLabel, bn);

	if(iso9660file)
		iso9660labels(disk, buf, blockWrite);

	if(score)
		root = ventiRoot(host, score);
	else{
		rootMetaInit(&e);
		root = rootInit(&e);
	}

	superInit(label, root, vtZeroScore);
	diskFree(disk);

	if(score == nil)
		topLevel(argv[0]);

Out:
	vtDetach();
	exits(0);
}

static u64int
fdsize(int fd)
{
	Dir *dir;
	u64int size;

	dir = dirfstat(fd);
	if(dir == nil)
		vtFatal("could not stat file: %r");
	size = dir->length;
	free(dir);
	return size;
}

static void
usage(void)
{
	fprint(2, "usage: %s [-b blocksize] [-h host] [-i file offset] "
		"[-l label] [-v score] [-y] file\n", argv0);
	exits("usage");
}

static void
partition(int fd, int bsize, Header *h)
{
	ulong nblock, ndata, nlabel;
	ulong lpb;

	if(bsize % 512 != 0)
		sysfatal("block size must be a multiple of 512 bytes");
	if(bsize > VtMaxLumpSize)
		sysfatal("block size must be less than %d", VtMaxLumpSize);

	memset(h, 0, sizeof(*h));
	h->blockSize = bsize;

	lpb = bsize/LabelSize;

	nblock = fdsize(fd)/bsize;

	/* sanity check */
	if(nblock < (HeaderOffset*10)/bsize)
		vtFatal("file too small");

	h->super = (HeaderOffset + 2*bsize)/bsize;
	h->label = h->super + 1;
	ndata = ((u64int)lpb)*(nblock - h->label)/(lpb+1);
	nlabel = (ndata + lpb - 1)/lpb;
	h->data = h->label + nlabel;
	h->end = h->data + ndata;

}

static u32int
tagGen(void)
{
	u32int tag;

	for(;;){
		tag = lrand();
		if(tag > RootTag)
			break;
	}
	return tag;
}

static void
entryInit(Entry *e)
{
	e->gen = 0;
	e->dsize = bsize;
	e->psize = bsize/VtEntrySize*VtEntrySize;
	e->flags = VtEntryActive;
	e->depth = 0;
	e->size = 0;
	memmove(e->score, vtZeroScore, VtScoreSize);
	e->tag = tagGen();
	e->snap = 0;
	e->archive = 0;
}

static void
rootMetaInit(Entry *e)
{
	u32int addr;
	u32int tag;
	DirEntry de;
	MetaBlock mb;
	MetaEntry me;

	memset(&de, 0, sizeof(de));
	de.elem = vtStrDup("root");
	de.entry = 0;
	de.gen = 0;
	de.mentry = 1;
	de.mgen = 0;
	de.size = 0;
	de.qid = qid++;
	de.uid = vtStrDup("adm");
	de.gid = vtStrDup("adm");
	de.mid = vtStrDup("adm");
	de.mtime = time(0);
	de.mcount = 0;
	de.ctime = time(0);
	de.atime = time(0);
	de.mode = ModeDir | 0555;

	tag = tagGen();
	addr = blockAlloc(BtData, tag);

	/* build up meta block */
	memset(buf, 0, bsize);
	mbInit(&mb, buf, bsize, bsize/100);
	me.size = deSize(&de);
	me.p = mbAlloc(&mb, me.size);
	assert(me.p != nil);
	dePack(&de, &me);
	mbInsert(&mb, 0, &me);
	mbPack(&mb);
	blockWrite(PartData, addr);
	deCleanup(&de);

	/* build up entry for meta block */
	entryInit(e);
	e->flags |= VtEntryLocal;
 	e->size = bsize;
	e->tag = tag;
	localToGlobal(addr, e->score);
}

static u32int
rootInit(Entry *e)
{
	ulong addr;
	u32int tag;

	tag = tagGen();

	addr = blockAlloc(BtDir, tag);
	memset(buf, 0, bsize);

	/* root meta data is in the third entry */
	entryPack(e, buf, 2);

	entryInit(e);
	e->flags |= VtEntryDir;
	entryPack(e, buf, 0);

	entryInit(e);
	entryPack(e, buf, 1);

	blockWrite(PartData, addr);

	entryInit(e);
	e->flags |= VtEntryLocal|VtEntryDir;
 	e->size = VtEntrySize*3;
	e->tag = tag;
	localToGlobal(addr, e->score);

	addr = blockAlloc(BtDir, RootTag);
	memset(buf, 0, bsize);
	entryPack(e, buf, 0);

	blockWrite(PartData, addr);

	return addr;
}


static u32int
blockAlloc(int type, u32int tag)
{
	static u32int addr;
	Label l;
	int lpb;

	lpb = bsize/LabelSize;

	blockRead(PartLabel, addr/lpb);
	if(!labelUnpack(&l, buf, addr % lpb))
		vtFatal("bad label: %r");
	if(l.state != BsFree)
		vtFatal("want to allocate block already in use");
	l.epoch = 1;
	l.epochClose = ~(u32int)0;
	l.type = type;
	l.state = BsAlloc;
	l.tag = tag;
	labelPack(&l, buf, addr % lpb);
	blockWrite(PartLabel, addr/lpb);
	return addr++;
}

static void
superInit(char *label, u32int root, uchar score[VtScoreSize])
{
	Super s;

	memset(buf, 0, bsize);
	memset(&s, 0, sizeof(s));
	s.version = SuperVersion;
	s.epochLow = 1;
	s.epochHigh = 1;
	s.qid = qid;
	s.active = root;
	s.next = NilBlock;
	s.current = NilBlock;
	strecpy(s.name, s.name+sizeof(s.name), label);
	memmove(s.last, score, VtScoreSize);

	superPack(&s, buf);
	blockWrite(PartSuper, 0);
}

static u64int
unittoull(char *s)
{
	char *es;
	u64int n;

	if(s == nil)
		return TWID64;
	n = strtoul(s, &es, 0);
	if(*es == 'k' || *es == 'K'){
		n *= 1024;
		es++;
	}else if(*es == 'm' || *es == 'M'){
		n *= 1024*1024;
		es++;
	}else if(*es == 'g' || *es == 'G'){
		n *= 1024*1024*1024;
		es++;
	}
	if(*es != '\0')
		return TWID64;
	return n;
}

static void
blockRead(int part, u32int addr)
{
	if(!diskReadRaw(disk, part, addr, buf))
		vtFatal("read failed: %r");
}

static void
blockWrite(int part, u32int addr)
{
	if(!diskWriteRaw(disk, part, addr, buf))
		vtFatal("write failed: %r");
}

static void
addFile(File *root, char *name, uint mode)
{
	File *f;

	f = fileCreate(root, name, mode | ModeDir, "adm");
	if(f == nil)
		vtFatal("could not create file: %s: %r", name);
	fileDecRef(f);
}

static void
topLevel(char *name)
{
	Fs *fs;
	File *root;

	/* ok, now we can open as a fs */
	fs = fsOpen(name, z, 100, OReadWrite);
	if(fs == nil)
		vtFatal("could not open file system: %r");
	vtRLock(fs->elk);
	root = fsGetRoot(fs);
	if(root == nil)
		vtFatal("could not open root: %r");
	addFile(root, "active", 0555);
	addFile(root, "archive", 0555);
	addFile(root, "snapshot", 0555);
	fileDecRef(root);
	if(iso9660file)
		iso9660copy(fs);
	vtRUnlock(fs->elk);
	fsClose(fs);
}

static int
ventiRead(uchar score[VtScoreSize], int type)
{
	int n;

	n = vtRead(z, score, type, buf, bsize);
	if(n < 0)
		vtFatal("ventiRead %V (%d) failed: %R", score, type);
	vtZeroExtend(type, buf, n, bsize);
	return n;
}

static u32int
ventiRoot(char *host, char *s)
{
	int i, n;
	uchar score[VtScoreSize];
	u32int addr, tag;
	DirEntry de;
	MetaBlock mb;
	MetaEntry me;
	Entry e;
	VtRoot root;

	if(!parseScore(score, s))
		vtFatal("bad score '%s'", s);

	if((z = vtDial(host, 0)) == nil
	|| !vtConnect(z, nil))
		vtFatal("connect to venti: %R");

	tag = tagGen();
	addr = blockAlloc(BtDir, tag);

	ventiRead(score, VtRootType);
	if(!vtRootUnpack(&root, buf))
		vtFatal("corrupted root: vtRootUnpack");
	n = ventiRead(root.score, VtDirType);

	/*
	 * Fossil's vac archives start with an extra layer of source,
	 * but vac's don't.
	 */
	if(n <= 2*VtEntrySize){
		if(!entryUnpack(&e, buf, 0))
			vtFatal("bad root: top entry");
		n = ventiRead(e.score, VtDirType);
	}

	/*
	 * There should be three root sources (and nothing else) here.
	 */
	for(i=0; i<3; i++){
		if(!entryUnpack(&e, buf, i)
		|| !(e.flags&VtEntryActive)
		|| e.psize < 256
		|| e.dsize < 256)
			vtFatal("bad root: entry %d", i);
		fprint(2, "%V\n", e.score);
	}
	if(n > 3*VtEntrySize)
		vtFatal("bad root: entry count");

	blockWrite(PartData, addr);

	/*
	 * Maximum qid is recorded in root's msource, entry #2 (conveniently in e).
	 */
	ventiRead(e.score, VtDataType);
	if(!mbUnpack(&mb, buf, bsize))
		vtFatal("bad root: mbUnpack");
	meUnpack(&me, &mb, 0);
	if(!deUnpack(&de, &me))
		vtFatal("bad root: dirUnpack");
	if(!de.qidSpace)
		vtFatal("bad root: no qidSpace");
	qid = de.qidMax;

	/*
	 * Recreate the top layer of source.
	 */
	entryInit(&e);
	e.flags |= VtEntryLocal|VtEntryDir;
	e.size = VtEntrySize*3;
	e.tag = tag;
	localToGlobal(addr, e.score);

	addr = blockAlloc(BtDir, RootTag);
	memset(buf, 0, bsize);
	entryPack(&e, buf, 0);
	blockWrite(PartData, addr);

	return addr;
}

static int
parseScore(uchar *score, char *buf)
{
	int i, c;

	memset(score, 0, VtScoreSize);

	if(strlen(buf) < VtScoreSize*2)
		return 0;
	for(i=0; i<VtScoreSize*2; i++){
		if(buf[i] >= '0' && buf[i] <= '9')
			c = buf[i] - '0';
		else if(buf[i] >= 'a' && buf[i] <= 'f')
			c = buf[i] - 'a' + 10;
		else if(buf[i] >= 'A' && buf[i] <= 'F')
			c = buf[i] - 'A' + 10;
		else
			return 0;

		if((i & 1) == 0)
			c <<= 4;

		score[i>>1] |= c;
	}
	return 1;
}
