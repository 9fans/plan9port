#include "stdinc.h"
#include <bio.h>
#include "dat.h"
#include "fns.h"
#include "9.h"

struct Fsys {
	VtLock* lock;

	char*	name;		/* copy here & Fs to ease error reporting */
	char*	dev;
	char*	venti;

	Fs*	fs;
	VtSession* session;
	int	ref;

	int	noauth;
	int	noperm;
	int	wstatallow;

	Fsys*	next;
};

int mempcnt;			/* from fossil.c */

int	fsGetBlockSize(Fs *fs);

static struct {
	VtLock*	lock;
	Fsys*	head;
	Fsys*	tail;

	char*	curfsys;
} sbox;

static char *_argv0;
#define argv0 _argv0

static char FsysAll[] = "all";

static char EFsysBusy[] = "fsys: '%s' busy";
static char EFsysExists[] = "fsys: '%s' already exists";
static char EFsysNoCurrent[] = "fsys: no current fsys";
static char EFsysNotFound[] = "fsys: '%s' not found";
static char EFsysNotOpen[] = "fsys: '%s' not open";

static char *
ventihost(char *host)
{
	if(host != nil)
		return vtStrDup(host);
	host = getenv("venti");
	if(host == nil)
		host = vtStrDup("$venti");
	return host;
}

static void
prventihost(char *host)
{
	char *vh;

	vh = ventihost(host);
	fprint(2, "%s: dialing venti at %s\n",
		argv0, netmkaddr(vh, 0, "venti"));
	free(vh);
}

static VtSession *
myDial(char *host, int canfail)
{
	prventihost(host);
	return vtDial(host, canfail);
}

static int
myRedial(VtSession *z, char *host)
{
	prventihost(host);
	return vtRedial(z, host);
}

static Fsys*
_fsysGet(char* name)
{
	Fsys *fsys;

	if(name == nil || name[0] == '\0')
		name = "main";

	vtRLock(sbox.lock);
	for(fsys = sbox.head; fsys != nil; fsys = fsys->next){
		if(strcmp(name, fsys->name) == 0){
			fsys->ref++;
			break;
		}
	}
	vtRUnlock(sbox.lock);
	if(fsys == nil)
		vtSetError(EFsysNotFound, name);
	return fsys;
}

static int
cmdPrintConfig(int argc, char* argv[])
{
	Fsys *fsys;
	char *usage = "usage: printconfig";

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND

	if(argc)
		return cliError(usage);

	vtRLock(sbox.lock);
	for(fsys = sbox.head; fsys != nil; fsys = fsys->next){
		consPrint("\tfsys %s config %s\n", fsys->name, fsys->dev);
		if(fsys->venti && fsys->venti[0])
			consPrint("\tfsys %s venti %q\n", fsys->name,
				fsys->venti);
	}
	vtRUnlock(sbox.lock);
	return 1;
}

Fsys*
fsysGet(char* name)
{
	Fsys *fsys;

	if((fsys = _fsysGet(name)) == nil)
		return nil;

	vtLock(fsys->lock);
	if(fsys->fs == nil){
		vtSetError(EFsysNotOpen, fsys->name);
		vtUnlock(fsys->lock);
		fsysPut(fsys);
		return nil;
	}
	vtUnlock(fsys->lock);

	return fsys;
}

char*
fsysGetName(Fsys* fsys)
{
	return fsys->name;
}

Fsys*
fsysIncRef(Fsys* fsys)
{
	vtLock(sbox.lock);
	fsys->ref++;
	vtUnlock(sbox.lock);

	return fsys;
}

void
fsysPut(Fsys* fsys)
{
	vtLock(sbox.lock);
	assert(fsys->ref > 0);
	fsys->ref--;
	vtUnlock(sbox.lock);
}

Fs*
fsysGetFs(Fsys* fsys)
{
	assert(fsys != nil && fsys->fs != nil);

	return fsys->fs;
}

void
fsysFsRlock(Fsys* fsys)
{
	vtRLock(fsys->fs->elk);
}

void
fsysFsRUnlock(Fsys* fsys)
{
	vtRUnlock(fsys->fs->elk);
}

int
fsysNoAuthCheck(Fsys* fsys)
{
	return fsys->noauth;
}

int
fsysNoPermCheck(Fsys* fsys)
{
	return fsys->noperm;
}

int
fsysWstatAllow(Fsys* fsys)
{
	return fsys->wstatallow;
}

static char modechars[] = "YUGalLdHSATs";
static ulong modebits[] = {
	ModeSticky,
	ModeSetUid,
	ModeSetGid,
	ModeAppend,
	ModeExclusive,
	ModeLink,
	ModeDir,
	ModeHidden,
	ModeSystem,
	ModeArchive,
	ModeTemporary,
	ModeSnapshot,
	0
};

char*
fsysModeString(ulong mode, char *buf)
{
	int i;
	char *p;

	p = buf;
	for(i=0; modebits[i]; i++)
		if(mode & modebits[i])
			*p++ = modechars[i];
	sprint(p, "%luo", mode&0777);
	return buf;
}

int
fsysParseMode(char* s, ulong* mode)
{
	ulong x, y;
	char *p;

	x = 0;
	for(; *s < '0' || *s > '9'; s++){
		if(*s == 0)
			return 0;
		p = strchr(modechars, *s);
		if(p == nil)
			return 0;
		x |= modebits[p-modechars];
	}
	y = strtoul(s, &p, 8);
	if(*p != '\0' || y > 0777)
		return 0;
	*mode = x|y;
	return 1;
}

File*
fsysGetRoot(Fsys* fsys, char* name)
{
	File *root, *sub;

	assert(fsys != nil && fsys->fs != nil);

	root = fsGetRoot(fsys->fs);
	if(name == nil || strcmp(name, "") == 0)
		return root;

	sub = fileWalk(root, name);
	fileDecRef(root);

	return sub;
}

static Fsys*
fsysAlloc(char* name, char* dev)
{
	Fsys *fsys;

	vtLock(sbox.lock);
	for(fsys = sbox.head; fsys != nil; fsys = fsys->next){
		if(strcmp(fsys->name, name) != 0)
			continue;
		vtSetError(EFsysExists, name);
		vtUnlock(sbox.lock);
		return nil;
	}

	fsys = vtMemAllocZ(sizeof(Fsys));
	fsys->lock = vtLockAlloc();
	fsys->name = vtStrDup(name);
	fsys->dev = vtStrDup(dev);

	fsys->ref = 1;

	if(sbox.tail != nil)
		sbox.tail->next = fsys;
	else
		sbox.head = fsys;
	sbox.tail = fsys;
	vtUnlock(sbox.lock);

	return fsys;
}

static int
fsysClose(Fsys* fsys, int argc, char* argv[])
{
	char *usage = "usage: [fsys name] close";

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND
	if(argc)
		return cliError(usage);

	return cliError("close isn't working yet; halt %s and then kill fossil",
		fsys->name);

	/*
	 * Oooh. This could be hard. What if fsys->ref != 1?
	 * Also, fsClose() either does the job or panics, can we
	 * gracefully detect it's still busy?
	 *
	 * More thought and care needed here.
	fsClose(fsys->fs);
	fsys->fs = nil;
	vtClose(fsys->session);
	fsys->session = nil;

	if(sbox.curfsys != nil && strcmp(fsys->name, sbox.curfsys) == 0){
		sbox.curfsys = nil;
		consPrompt(nil);
	}

	return 1;
	 */
}

static int
fsysVac(Fsys* fsys, int argc, char* argv[])
{
	uchar score[VtScoreSize];
	char *usage = "usage: [fsys name] vac path";

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND
	if(argc != 1)
		return cliError(usage);

	if(!fsVac(fsys->fs, argv[0], score))
		return 0;

	consPrint("vac:%V\n", score);
	return 1;
}

static int
fsysSnap(Fsys* fsys, int argc, char* argv[])
{
	int doarchive;
	char *usage = "usage: [fsys name] snap [-a] [-s /active] [-d /archive/yyyy/mmmm]";
	char *src, *dst;

	src = nil;
	dst = nil;
	doarchive = 0;
	ARGBEGIN{
	default:
		return cliError(usage);
	case 'a':
		doarchive = 1;
		break;
	case 'd':
		if((dst = ARGF()) == nil)
			return cliError(usage);
		break;
	case 's':
		if((src = ARGF()) == nil)
			return cliError(usage);
		break;
	}ARGEND
	if(argc)
		return cliError(usage);

	if(!fsSnapshot(fsys->fs, src, dst, doarchive))
		return 0;

	return 1;
}

static int
fsysSnapClean(Fsys *fsys, int argc, char* argv[])
{
	u32int arch, snap, life;
	char *usage = "usage: [fsys name] snapclean [maxminutes]\n";

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND

	if(argc > 1)
		return cliError(usage);
	if(argc == 1)
		life = atoi(argv[0]);
	else
		snapGetTimes(fsys->fs->snap, &arch, &snap, &life);

	fsSnapshotCleanup(fsys->fs, life);
	return 1;
}

static int
fsysSnapTime(Fsys* fsys, int argc, char* argv[])
{
	char buf[128], *x;
	int hh, mm, changed;
	u32int arch, snap, life;
	char *usage = "usage: [fsys name] snaptime [-a hhmm] [-s snapminutes] [-t maxminutes]";

	changed = 0;
	snapGetTimes(fsys->fs->snap, &arch, &snap, &life);
	ARGBEGIN{
	case 'a':
		changed = 1;
		x = ARGF();
		if(x == nil)
			return cliError(usage);
		if(strcmp(x, "none") == 0){
			arch = ~(u32int)0;
			break;
		}
		if(strlen(x) != 4 || strspn(x, "0123456789") != 4)
			return cliError(usage);
		hh = (x[0]-'0')*10 + x[1]-'0';
		mm = (x[2]-'0')*10 + x[3]-'0';
		if(hh >= 24 || mm >= 60)
			return cliError(usage);
		arch = hh*60+mm;
		break;
	case 's':
		changed = 1;
		x = ARGF();
		if(x == nil)
			return cliError(usage);
		if(strcmp(x, "none") == 0){
			snap = ~(u32int)0;
			break;
		}
		snap = atoi(x);
		break;
	case 't':
		changed = 1;
		x = ARGF();
		if(x == nil)
			return cliError(usage);
		if(strcmp(x, "none") == 0){
			life = ~(u32int)0;
			break;
		}
		life = atoi(x);
		break;
	default:
		return cliError(usage);
	}ARGEND
	if(argc > 0)
		return cliError(usage);

	if(changed){
		snapSetTimes(fsys->fs->snap, arch, snap, life);
		return 1;
	}
	snapGetTimes(fsys->fs->snap, &arch, &snap, &life);
	if(arch != ~(u32int)0)
		sprint(buf, "-a %02d%02d", arch/60, arch%60);
	else
		sprint(buf, "-a none");
	if(snap != ~(u32int)0)
		sprint(buf+strlen(buf), " -s %d", snap);
	else
		sprint(buf+strlen(buf), " -s none");
	if(life != ~(u32int)0)
		sprint(buf+strlen(buf), " -t %ud", life);
	else
		sprint(buf+strlen(buf), " -t none");
	consPrint("\tsnaptime %s\n", buf);
	return 1;
}

static int
fsysSync(Fsys* fsys, int argc, char* argv[])
{
	char *usage = "usage: [fsys name] sync";
	int n;
	
	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND
	if(argc > 0)
		return cliError(usage);

	n = cacheDirty(fsys->fs->cache);
	fsSync(fsys->fs);
	consPrint("\t%s sync: wrote %d blocks\n", fsys->name, n);
	return 1;
}

static int
fsysHalt(Fsys *fsys, int argc, char* argv[])
{
	char *usage = "usage: [fsys name] halt";

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND
	if(argc > 0)
		return cliError(usage);

	fsHalt(fsys->fs);
	return 1;
}

static int
fsysUnhalt(Fsys *fsys, int argc, char* argv[])
{
	char *usage = "usage: [fsys name] unhalt";

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND
	if(argc > 0)
		return cliError(usage);

	if(!fsys->fs->halted)
		return cliError("file system %s not halted", fsys->name);

	fsUnhalt(fsys->fs);
	return 1;
}

static int
fsysRemove(Fsys* fsys, int argc, char* argv[])
{
	File *file;
	char *usage = "usage: [fsys name] remove path ...";

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND
	if(argc == 0)
		return cliError(usage);

	vtRLock(fsys->fs->elk);
	while(argc > 0){
		if((file = fileOpen(fsys->fs, argv[0])) == nil)
			consPrint("%s: %R\n", argv[0]);
		else{
			if(!fileRemove(file, uidadm))
				consPrint("%s: %R\n", argv[0]);
			fileDecRef(file);
		}
		argc--;
		argv++;
	}
	vtRUnlock(fsys->fs->elk);

	return 1;
}

static int
fsysClri(Fsys* fsys, int argc, char* argv[])
{
	char *usage = "usage: [fsys name] clri path ...";

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND
	if(argc == 0)
		return cliError(usage);

	vtRLock(fsys->fs->elk);
	while(argc > 0){
		if(!fileClriPath(fsys->fs, argv[0], uidadm))
			consPrint("clri %s: %R\n", argv[0]);
		argc--;
		argv++;
	}
	vtRUnlock(fsys->fs->elk);

	return 1;
}

/*
 * Inspect and edit the labels for blocks on disk.
 */
static int
fsysLabel(Fsys* fsys, int argc, char* argv[])
{
	Fs *fs;
	Label l;
	int n, r;
	u32int addr;
	Block *b, *bb;
	char *usage = "usage: [fsys name] label addr [type state epoch epochClose tag]";

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND
	if(argc != 1 && argc != 6)
		return cliError(usage);

	r = 0;
	vtRLock(fsys->fs->elk);

	fs = fsys->fs;
	addr = strtoul(argv[0], 0, 0);
	b = cacheLocal(fs->cache, PartData, addr, OReadOnly);
	if(b == nil)
		goto Out0;

	l = b->l;
	consPrint("%slabel %#ux %ud %ud %ud %ud %#x\n",
		argc==6 ? "old: " : "", addr, l.type, l.state,
		l.epoch, l.epochClose, l.tag);

	if(argc == 6){
		if(strcmp(argv[1], "-") != 0)
			l.type = atoi(argv[1]);
		if(strcmp(argv[2], "-") != 0)
			l.state = atoi(argv[2]);
		if(strcmp(argv[3], "-") != 0)
			l.epoch = strtoul(argv[3], 0, 0);
		if(strcmp(argv[4], "-") != 0)
			l.epochClose = strtoul(argv[4], 0, 0);
		if(strcmp(argv[5], "-") != 0)
			l.tag = strtoul(argv[5], 0, 0);

		consPrint("new: label %#ux %ud %ud %ud %ud %#x\n",
			addr, l.type, l.state, l.epoch, l.epochClose, l.tag);
		bb = _blockSetLabel(b, &l);
		if(bb == nil)
			goto Out1;
		n = 0;
		for(;;){
			if(blockWrite(bb, Waitlock)){
				while(bb->iostate != BioClean){
					assert(bb->iostate == BioWriting);
					vtSleep(bb->ioready);
				}
				break;
			}
			consPrint("blockWrite: %R\n");
			if(n++ >= 5){
				consPrint("giving up\n");
				break;
			}
			sleep(5*1000);
		}
		blockPut(bb);
	}
	r = 1;
Out1:
	blockPut(b);
Out0:
	vtRUnlock(fs->elk);

	return r;
}

/*
 * Inspect and edit the blocks on disk.
 */
static int
fsysBlock(Fsys* fsys, int argc, char* argv[])
{
	Fs *fs;
	char *s;
	Block *b;
	uchar *buf;
	u32int addr;
	int c, count, i, offset;
	char *usage = "usage: [fsys name] block addr offset [count [data]]";

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND
	if(argc < 2 || argc > 4)
		return cliError(usage);

	fs = fsys->fs;
	addr = strtoul(argv[0], 0, 0);
	offset = strtoul(argv[1], 0, 0);
	if(offset < 0 || offset >= fs->blockSize){
		vtSetError("bad offset");
		return 0;
	}
	if(argc > 2)
		count = strtoul(argv[2], 0, 0);
	else
		count = 100000000;
	if(offset+count > fs->blockSize)
		count = fs->blockSize - count;

	vtRLock(fs->elk);

	b = cacheLocal(fs->cache, PartData, addr, argc==4 ? OReadWrite : OReadOnly);
	if(b == nil){
		vtSetError("cacheLocal %#ux: %R", addr);
		vtRUnlock(fs->elk);
		return 0;
	}

	consPrint("\t%sblock %#ux %ud %ud %.*H\n",
		argc==4 ? "old: " : "", addr, offset, count, count, b->data+offset);

	if(argc == 4){
		s = argv[3];
		if(strlen(s) != 2*count){
			vtSetError("bad data count");
			goto Out;
		}
		buf = vtMemAllocZ(count);
		for(i = 0; i < count*2; i++){
			if(s[i] >= '0' && s[i] <= '9')
				c = s[i] - '0';
			else if(s[i] >= 'a' && s[i] <= 'f')
				c = s[i] - 'a' + 10;
			else if(s[i] >= 'A' && s[i] <= 'F')
				c = s[i] - 'A' + 10;
			else{
				vtSetError("bad hex");
				vtMemFree(buf);
				goto Out;
			}
			if((i & 1) == 0)
				c <<= 4;
			buf[i>>1] |= c;
		}
		memmove(b->data+offset, buf, count);
		consPrint("\tnew: block %#ux %ud %ud %.*H\n",
			addr, offset, count, count, b->data+offset);
		blockDirty(b);
	}

Out:
	blockPut(b);
	vtRUnlock(fs->elk);

	return 1;
}

/*
 * Free a disk block.
 */
static int
fsysBfree(Fsys* fsys, int argc, char* argv[])
{
	Fs *fs;
	Label l;
	char *p;
	Block *b;
	u32int addr;
	char *usage = "usage: [fsys name] bfree addr ...";

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND
	if(argc == 0)
		return cliError(usage);

	fs = fsys->fs;
	vtRLock(fs->elk);
	while(argc > 0){
		addr = strtoul(argv[0], &p, 0);
		if(*p != '\0'){
			consPrint("bad address - '%ud'\n", addr);
			/* syntax error; let's stop */
			vtRUnlock(fs->elk);
			return 0;
		}
		b = cacheLocal(fs->cache, PartData, addr, OReadOnly);
		if(b == nil){
			consPrint("loading %#ux: %R\n", addr);
			continue;
		}
		l = b->l;
		if(l.state == BsFree)
			consPrint("%#ux is already free\n", addr);
		else{
			consPrint("label %#ux %ud %ud %ud %ud %#x\n",
				addr, l.type, l.state, l.epoch, l.epochClose, l.tag);
			l.state = BsFree;
			l.type = BtMax;
			l.tag = 0;
			l.epoch = 0;
			l.epochClose = 0;
			if(!blockSetLabel(b, &l, 0))
				consPrint("freeing %#ux: %R\n", addr);
		}
		blockPut(b);
		argc--;
		argv++;
	}
	vtRUnlock(fs->elk);

	return 1;
}

static int
fsysDf(Fsys *fsys, int argc, char* argv[])
{
	char *usage = "usage: [fsys name] df";
	u32int used, tot, bsize;
	Fs *fs;

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND
	if(argc != 0)
		return cliError(usage);

	fs = fsys->fs;
	cacheCountUsed(fs->cache, fs->elo, &used, &tot, &bsize);
	consPrint("\t%s: %,llud used + %,llud free = %,llud (%.1f%% used)\n",
		fsys->name, used*(vlong)bsize, (tot-used)*(vlong)bsize,
		tot*(vlong)bsize, used*100.0/tot);
	return 1;
}

/*
 * Zero an entry or a pointer.
 */
static int
fsysClrep(Fsys* fsys, int argc, char* argv[], int ch)
{
	Fs *fs;
	Entry e;
	Block *b;
	u32int addr;
	int i, max, offset, sz;
	uchar zero[VtEntrySize];
	char *usage = "usage: [fsys name] clr%c addr offset ...";

	ARGBEGIN{
	default:
		return cliError(usage, ch);
	}ARGEND
	if(argc < 2)
		return cliError(usage, ch);

	fs = fsys->fs;
	vtRLock(fsys->fs->elk);

	addr = strtoul(argv[0], 0, 0);
	b = cacheLocal(fs->cache, PartData, addr, argc==4 ? OReadWrite : OReadOnly);
	if(b == nil){
		vtSetError("cacheLocal %#ux: %R", addr);
	Err:
		vtRUnlock(fsys->fs->elk);
		return 0;
	}

	switch(ch){
	default:
		vtSetError("clrep");
		goto Err;
	case 'e':
		if(b->l.type != BtDir){
			vtSetError("wrong block type");
			goto Err;
		}
		sz = VtEntrySize;
		memset(&e, 0, sizeof e);
		entryPack(&e, zero, 0);
		break;
	case 'p':
		if(b->l.type == BtDir || b->l.type == BtData){
			vtSetError("wrong block type");
			goto Err;
		}
		sz = VtScoreSize;
		memmove(zero, vtZeroScore, VtScoreSize);
		break;
	}
	max = fs->blockSize/sz;

	for(i = 1; i < argc; i++){
		offset = atoi(argv[i]);
		if(offset >= max){
			consPrint("\toffset %d too large (>= %d)\n", i, max);
			continue;
		}
		consPrint("\tblock %#ux %d %d %.*H\n", addr, offset*sz, sz, sz, b->data+offset*sz);
		memmove(b->data+offset*sz, zero, sz);
	}
	blockDirty(b);
	blockPut(b);
	vtRUnlock(fsys->fs->elk);

	return 1;
}

static int
fsysClre(Fsys* fsys, int argc, char* argv[])
{
	return fsysClrep(fsys, argc, argv, 'e');
}

static int
fsysClrp(Fsys* fsys, int argc, char* argv[])
{
	return fsysClrep(fsys, argc, argv, 'p');
}

static int
fsysEsearch1(File* f, char* s, u32int elo)
{
	int n, r;
	DirEntry de;
	DirEntryEnum *dee;
	File *ff;
	Entry e, ee;
	char *t;

	dee = deeOpen(f);
	if(dee == nil)
		return 0;

	n = 0;
	for(;;){
		r = deeRead(dee, &de);
		if(r < 0){
			consPrint("\tdeeRead %s/%s: %R\n", s, de.elem);
			break;
		}
		if(r == 0)
			break;
		if(de.mode & ModeSnapshot){
			if((ff = fileWalk(f, de.elem)) == nil)
				consPrint("\tcannot walk %s/%s: %R\n", s, de.elem);
			else{
				if(!fileGetSources(ff, &e, &ee))
					consPrint("\tcannot get sources for %s/%s: %R\n", s, de.elem);
				else if(e.snap != 0 && e.snap < elo){
					consPrint("\t%ud\tclri %s/%s\n", e.snap, s, de.elem);
					n++;
				}
				fileDecRef(ff);
			}
		}
		else if(de.mode & ModeDir){
			if((ff = fileWalk(f, de.elem)) == nil)
				consPrint("\tcannot walk %s/%s: %R\n", s, de.elem);
			else{
				t = smprint("%s/%s", s, de.elem);
				n += fsysEsearch1(ff, t, elo);
				vtMemFree(t);
				fileDecRef(ff);
			}
		}
		deCleanup(&de);
		if(r < 0)
			break;
	}
	deeClose(dee);

	return n;
}

static int
fsysEsearch(Fs* fs, char* path, u32int elo)
{
	int n;
	File *f;
	DirEntry de;

	f = fileOpen(fs, path);
	if(f == nil)
		return 0;
	if(!fileGetDir(f, &de)){
		consPrint("\tfileGetDir %s failed: %R\n", path);
		fileDecRef(f);
		return 0;
	}
	if((de.mode & ModeDir) == 0){
		fileDecRef(f);
		deCleanup(&de);
		return 0;
	}
	deCleanup(&de);
	n = fsysEsearch1(f, path, elo);
	fileDecRef(f);
	return n;
}

static int
fsysEpoch(Fsys* fsys, int argc, char* argv[])
{
	Fs *fs;
	int force, n, remove;
	u32int low, old;
	char *usage = "usage: [fsys name] epoch [[-ry] low]";

	force = 0;
	remove = 0;
	ARGBEGIN{
	case 'y':
		force = 1;
		break;
	case 'r':
		remove = 1;
		break;
	default:
		return cliError(usage);
	}ARGEND
	if(argc > 1)
		return cliError(usage);
	if(argc > 0)
		low = strtoul(argv[0], 0, 0);
	else
		low = ~(u32int)0;

	if(low == 0)
		return cliError("low epoch cannot be zero");

	fs = fsys->fs;

	vtRLock(fs->elk);
	consPrint("\tlow %ud hi %ud\n", fs->elo, fs->ehi);
	if(low == ~(u32int)0){
		vtRUnlock(fs->elk);
		return 1;
	}
	n = fsysEsearch(fsys->fs, "/archive", low);
	n += fsysEsearch(fsys->fs, "/snapshot", low);
	consPrint("\t%d snapshot%s found with epoch < %ud\n", n, n==1 ? "" : "s", low);
	vtRUnlock(fs->elk);

	/*
	 * There's a small race here -- a new snapshot with epoch < low might
	 * get introduced now that we unlocked fs->elk.  Low has to
	 * be <= fs->ehi.  Of course, in order for this to happen low has
	 * to be equal to the current fs->ehi _and_ a snapshot has to
	 * run right now.  This is a small enough window that I don't care.
	 */
	if(n != 0 && !force){
		consPrint("\tnot setting low epoch\n");
		return 1;
	}
	old = fs->elo;
	if(!fsEpochLow(fs, low))
		consPrint("\tfsEpochLow: %R\n");
	else{
		consPrint("\told: epoch%s %ud\n", force ? " -y" : "", old);
		consPrint("\tnew: epoch%s %ud\n", force ? " -y" : "", fs->elo);
		if(fs->elo < low)
			consPrint("\twarning: new low epoch < old low epoch\n");
		if(force && remove)
			fsSnapshotRemove(fs);
	}

	return 1;
}

static int
fsysCreate(Fsys* fsys, int argc, char* argv[])
{
	int r;
	ulong mode;
	char *elem, *p, *path;
	char *usage = "usage: [fsys name] create path uid gid perm";
	DirEntry de;
	File *file, *parent;

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND
	if(argc != 4)
		return cliError(usage);

	if(!fsysParseMode(argv[3], &mode))
		return cliError(usage);
	if(mode&ModeSnapshot)
		return cliError("create - cannot create with snapshot bit set");

	if(strcmp(argv[1], uidnoworld) == 0)
		return cliError("permission denied");

	vtRLock(fsys->fs->elk);
	path = vtStrDup(argv[0]);
	if((p = strrchr(path, '/')) != nil){
		*p++ = '\0';
		elem = p;
		p = path;
		if(*p == '\0')
			p = "/";
	}
	else{
		p = "/";
		elem = path;
	}

	r = 0;
	if((parent = fileOpen(fsys->fs, p)) == nil)
		goto out;

	file = fileCreate(parent, elem, mode, argv[1]);
	fileDecRef(parent);
	if(file == nil){
		vtSetError("create %s/%s: %R", p, elem);
		goto out;
	}

	if(!fileGetDir(file, &de)){
		vtSetError("stat failed after create: %R");
		goto out1;
	}

	if(strcmp(de.gid, argv[2]) != 0){
		vtMemFree(de.gid);
		de.gid = vtStrDup(argv[2]);
		if(!fileSetDir(file, &de, argv[1])){
			vtSetError("wstat failed after create: %R");
			goto out2;
		}
	}
	r = 1;

out2:
	deCleanup(&de);
out1:
	fileDecRef(file);
out:
	vtMemFree(path);
	vtRUnlock(fsys->fs->elk);

	return r;
}

static void
fsysPrintStat(char *prefix, char *file, DirEntry *de)
{
	char buf[64];

	if(prefix == nil)
		prefix = "";
	consPrint("%sstat %q %q %q %q %s %llud\n", prefix,
		file, de->elem, de->uid, de->gid, fsysModeString(de->mode, buf), de->size);
}

static int
fsysStat(Fsys* fsys, int argc, char* argv[])
{
	int i;
	File *f;
	DirEntry de;
	char *usage = "usage: [fsys name] stat files...";

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND

	if(argc == 0)
		return cliError(usage);

	vtRLock(fsys->fs->elk);
	for(i=0; i<argc; i++){
		if((f = fileOpen(fsys->fs, argv[i])) == nil){
			consPrint("%s: %R\n", argv[i]);
			continue;
		}
		if(!fileGetDir(f, &de)){
			consPrint("%s: %R\n", argv[i]);
			fileDecRef(f);
			continue;
		}
		fsysPrintStat("\t", argv[i], &de);
		deCleanup(&de);
		fileDecRef(f);
	}
	vtRUnlock(fsys->fs->elk);
	return 1;
}

static int
fsysWstat(Fsys *fsys, int argc, char* argv[])
{
	File *f;
	char *p;
	DirEntry de;
	char *usage = "usage: [fsys name] wstat file elem uid gid mode length\n"
		"\tuse - for any field to mean don't change";

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND

	if(argc != 6)
		return cliError(usage);

	vtRLock(fsys->fs->elk);
	if((f = fileOpen(fsys->fs, argv[0])) == nil){
		vtSetError("console wstat - walk - %R");
		vtRUnlock(fsys->fs->elk);
		return 0;
	}
	if(!fileGetDir(f, &de)){
		vtSetError("console wstat - stat - %R");
		fileDecRef(f);
		vtRUnlock(fsys->fs->elk);
		return 0;
	}
	fsysPrintStat("\told: w", argv[0], &de);

	if(strcmp(argv[1], "-") != 0){
		if(!validFileName(argv[1])){
			vtSetError("console wstat - bad elem");
			goto error;
		}
		vtMemFree(de.elem);
		de.elem = vtStrDup(argv[1]);
	}
	if(strcmp(argv[2], "-") != 0){
		if(!validUserName(argv[2])){
			vtSetError("console wstat - bad uid");
			goto error;
		}
		vtMemFree(de.uid);
		de.uid = vtStrDup(argv[2]);
	}
	if(strcmp(argv[3], "-") != 0){
		if(!validUserName(argv[3])){
			vtSetError("console wstat - bad gid");
			goto error;
		}
		vtMemFree(de.gid);
		de.gid = vtStrDup(argv[3]);
	}
	if(strcmp(argv[4], "-") != 0){
		if(!fsysParseMode(argv[4], &de.mode)){
			vtSetError("console wstat - bad mode");
			goto error;
		}
	}
	if(strcmp(argv[5], "-") != 0){
		de.size = strtoull(argv[5], &p, 0);
		if(argv[5][0] == '\0' || *p != '\0' || (vlong)de.size < 0){
			vtSetError("console wstat - bad length");
			goto error;
		}
	}

	if(!fileSetDir(f, &de, uidadm)){
		vtSetError("console wstat - %R");
		goto error;
	}
	deCleanup(&de);

	if(!fileGetDir(f, &de)){
		vtSetError("console wstat - stat2 - %R");
		goto error;
	}
	fsysPrintStat("\tnew: w", argv[0], &de);
	deCleanup(&de);
	fileDecRef(f);
	vtRUnlock(fsys->fs->elk);

	return 1;

error:
	deCleanup(&de);	/* okay to do this twice */
	fileDecRef(f);
	vtRUnlock(fsys->fs->elk);
	return 0;
}

static void
fsckClri(Fsck *fsck, char *name, MetaBlock *mb, int i, Block *b)
{
	USED(name);

	if((fsck->flags&DoClri) == 0)
		return;

	mbDelete(mb, i);
	mbPack(mb);
	blockDirty(b);	
}

static void
fsckClose(Fsck *fsck, Block *b, u32int epoch)
{
	Label l;

	if((fsck->flags&DoClose) == 0)
		return;
	l = b->l;
	if(l.state == BsFree || (l.state&BsClosed)){
		consPrint("%#ux is already closed\n", b->addr);
		return;
	}
	if(epoch){	
		l.state |= BsClosed;
		l.epochClose = epoch;
	}else
		l.state = BsFree;
		
	if(!blockSetLabel(b, &l, 0))
		consPrint("%#ux setlabel: %R\n", b->addr);
}

static void
fsckClre(Fsck *fsck, Block *b, int offset)
{
	Entry e;

	if((fsck->flags&DoClre) == 0)
		return;
	if(offset<0 || offset*VtEntrySize >= fsck->bsize){
		consPrint("bad clre\n");
		return;
	}
	memset(&e, 0, sizeof e);
	entryPack(&e, b->data, offset);
	blockDirty(b);
}

static void
fsckClrp(Fsck *fsck, Block *b, int offset)
{
	if((fsck->flags&DoClrp) == 0)
		return;
	if(offset<0 || offset*VtScoreSize >= fsck->bsize){
		consPrint("bad clre\n");
		return;
	}
	memmove(b->data+offset*VtScoreSize, vtZeroScore, VtScoreSize);
	blockDirty(b);
}

static int
fsysCheck(Fsys *fsys, int argc, char *argv[])
{
	int i, halting;
	char *usage = "usage: [fsys name] check [-v] [options]";
	Fsck fsck;
	Block *b;
	Super super;

	memset(&fsck, 0, sizeof fsck);
	fsck.fs = fsys->fs;
	fsck.clri = fsckClri;
	fsck.clre = fsckClre;
	fsck.clrp = fsckClrp;
	fsck.close = fsckClose;
	fsck.print = consPrint;

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND

	for(i=0; i<argc; i++){
		if(strcmp(argv[i], "pblock") == 0)
			fsck.printblocks = 1;
		else if(strcmp(argv[i], "pdir") == 0)
			fsck.printdirs = 1;
		else if(strcmp(argv[i], "pfile") == 0)
			fsck.printfiles = 1;
		else if(strcmp(argv[i], "bclose") == 0)
			fsck.flags |= DoClose;
		else if(strcmp(argv[i], "clri") == 0)
			fsck.flags |= DoClri;
		else if(strcmp(argv[i], "clre") == 0)
			fsck.flags |= DoClre;
		else if(strcmp(argv[i], "clrp") == 0)
			fsck.flags |= DoClrp;
		else if(strcmp(argv[i], "fix") == 0)
			fsck.flags |= DoClose|DoClri|DoClre|DoClrp;
		else if(strcmp(argv[i], "venti") == 0)
			fsck.useventi = 1;
		else if(strcmp(argv[i], "snapshot") == 0)
			fsck.walksnapshots = 1;
		else{
			consPrint("unknown option '%s'\n", argv[i]);
			return cliError(usage);
		}
	}

	halting = fsys->fs->halted==0;
	if(halting)
		fsHalt(fsys->fs);
	if(fsys->fs->arch){
		b = superGet(fsys->fs->cache, &super);
		if(b == nil){
			consPrint("could not load super block\n");
			goto Out;
		}
		blockPut(b);
		if(super.current != NilBlock){
			consPrint("cannot check fs while archiver is running; "
				"wait for it to finish\n");
			goto Out;
		}
	}
	fsCheck(&fsck);
	consPrint("fsck: %d clri, %d clre, %d clrp, %d bclose\n",
		fsck.nclri, fsck.nclre, fsck.nclrp, fsck.nclose);
Out:
	if(halting)
		fsUnhalt(fsys->fs);
	return 1;
}

static int
fsysVenti(char* name, int argc, char* argv[])
{
	int r;
	char *host;
	char *usage = "usage: [fsys name] venti [address]";
	Fsys *fsys;

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND

	if(argc == 0)
		host = nil;
	else if(argc == 1)
		host = argv[0];
	else
		return cliError(usage);

	if((fsys = _fsysGet(name)) == nil)
		return 0;

	vtLock(fsys->lock);
	if(host == nil)
		host = fsys->venti;
	else{
		vtMemFree(fsys->venti);
		if(host[0])
			fsys->venti = vtStrDup(host);
		else{
			host = nil;
			fsys->venti = nil;
		}
	}

	/* already open: do a redial */
	if(fsys->fs != nil){
		if(fsys->session == nil){
			vtSetError("file system was opened with -V");
			r = 0;
			goto out;
		}
		r = 1;
		if(!myRedial(fsys->session, host)
		|| !vtConnect(fsys->session, 0))
			r = 0;
		goto out;
	}

	/* not yet open: try to dial */
	if(fsys->session)
		vtClose(fsys->session);
	r = 1;
	if((fsys->session = myDial(host, 0)) == nil
	|| !vtConnect(fsys->session, 0))
		r = 0;
out:
	vtUnlock(fsys->lock);
	fsysPut(fsys);
	return r;
}

static ulong
freemem(void)
{
	int nf, pgsize = 0;
	uvlong size, userpgs = 0, userused = 0;
	char *ln, *sl;
	char *fields[2];
	Biobuf *bp;

	size = 64*1024*1024;
	bp = Bopen("#c/swap", OREAD);
	if (bp != nil) {
		while ((ln = Brdline(bp, '\n')) != nil) {
			ln[Blinelen(bp)-1] = '\0';
			nf = tokenize(ln, fields, nelem(fields));
			if (nf != 2)
				continue;
			if (strcmp(fields[1], "pagesize") == 0)
				pgsize = atoi(fields[0]);
			else if (strcmp(fields[1], "user") == 0) {
				sl = strchr(fields[0], '/');
				if (sl == nil)
					continue;
				userpgs = atoll(sl+1);
				userused = atoll(fields[0]);
			}
		}
		Bterm(bp);
		if (pgsize > 0 && userpgs > 0)
			size = (userpgs - userused) * pgsize;
	}
	/* cap it to keep the size within 32 bits */
	if (size >= 3840UL * 1024 * 1024)
		size = 3840UL * 1024 * 1024;
	return size;
}

static int
fsysOpen(char* name, int argc, char* argv[])
{
	char *p, *host;
	Fsys *fsys;
	int noauth, noventi, noperm, rflag, wstatallow, noatimeupd;
	long ncache;
	char *usage = "usage: fsys name open [-APVWr] [-c ncache]";

	ncache = 1000;
	noauth = noperm = wstatallow = noventi = noatimeupd = 0;
	rflag = OReadWrite;

	ARGBEGIN{
	default:
		return cliError(usage);
	case 'A':
		noauth = 1;
		break;
	case 'P':
		noperm = 1;
		break;
	case 'V':
		noventi = 1;
		break;
	case 'W':
		wstatallow = 1;
		break;
	case 'a':
		noatimeupd = 1;
		break;
	case 'c':
		p = ARGF();
		if(p == nil)
			return cliError(usage);
		ncache = strtol(argv[0], &p, 0);
		if(ncache <= 0 || p == argv[0] || *p != '\0')
			return cliError(usage);
		break;
	case 'r':
		rflag = OReadOnly;
		break;
	}ARGEND
	if(argc)
		return cliError(usage);

	if((fsys = _fsysGet(name)) == nil)
		return 0;

	/* automatic memory sizing? */
	if(mempcnt > 0) {
		/* TODO: 8K is a hack; use the actual block size */
		ncache = (((vlong)freemem() * mempcnt) / 100) / (8*1024);
		if (ncache < 100)
			ncache = 100;
	}

	vtLock(fsys->lock);
	if(fsys->fs != nil){
		vtSetError(EFsysBusy, fsys->name);
		vtUnlock(fsys->lock);
		fsysPut(fsys);
		return 0;
	}

	if(noventi){
		if(fsys->session){
			vtClose(fsys->session);
			fsys->session = nil;
		}
	}
	else if(fsys->session == nil){
		if(fsys->venti && fsys->venti[0])
			host = fsys->venti;
		else
			host = nil;
		fsys->session = myDial(host, 1);
		if(!vtConnect(fsys->session, nil) && !noventi)
			fprint(2, "warning: connecting to venti: %R\n");
	}
	if((fsys->fs = fsOpen(fsys->dev, fsys->session, ncache, rflag)) == nil){
		vtSetError("fsOpen: %R");
		vtUnlock(fsys->lock);
		fsysPut(fsys);
		return 0;
	}
	fsys->fs->name = fsys->name;	/* for better error messages */
	fsys->noauth = noauth;
	fsys->noperm = noperm;
	fsys->wstatallow = wstatallow;
	fsys->fs->noatimeupd = noatimeupd;
	vtUnlock(fsys->lock);
	fsysPut(fsys);

	if(strcmp(name, "main") == 0)
		usersFileRead(nil);

	return 1;
}

static int
fsysUnconfig(char* name, int argc, char* argv[])
{
	Fsys *fsys, **fp;
	char *usage = "usage: fsys name unconfig";

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND
	if(argc)
		return cliError(usage);

	vtLock(sbox.lock);
	fp = &sbox.head;
	for(fsys = *fp; fsys != nil; fsys = fsys->next){
		if(strcmp(fsys->name, name) == 0)
			break;
		fp = &fsys->next;
	}
	if(fsys == nil){
		vtSetError(EFsysNotFound, name);
		vtUnlock(sbox.lock);
		return 0;
	}
	if(fsys->ref != 0 || fsys->fs != nil){
		vtSetError(EFsysBusy, fsys->name);
		vtUnlock(sbox.lock);
		return 0;
	}
	*fp = fsys->next;
	vtUnlock(sbox.lock);

	if(fsys->session != nil){
		vtClose(fsys->session);
		vtFree(fsys->session);
	}
	if(fsys->venti != nil)
		vtMemFree(fsys->venti);
	if(fsys->dev != nil)
		vtMemFree(fsys->dev);
	if(fsys->name != nil)
		vtMemFree(fsys->name);
	vtMemFree(fsys);

	return 1;
}

static int
fsysConfig(char* name, int argc, char* argv[])
{
	Fsys *fsys;
	char *part;
	char *usage = "usage: fsys name config [dev]";

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND
	if(argc > 1)
		return cliError(usage);

	if(argc == 0)
		part = foptname;
	else
		part = argv[0];

	if((fsys = _fsysGet(part)) != nil){
		vtLock(fsys->lock);
		if(fsys->fs != nil){
			vtSetError(EFsysBusy, fsys->name);
			vtUnlock(fsys->lock);
			fsysPut(fsys);
			return 0;
		}
		vtMemFree(fsys->dev);
		fsys->dev = vtStrDup(part);
		vtUnlock(fsys->lock);
	}
	else if((fsys = fsysAlloc(name, part)) == nil)
		return 0;

	fsysPut(fsys);
	return 1;
}

static struct {
	char*	cmd;
	int	(*f)(Fsys*, int, char**);
	int	(*f1)(char*, int, char**);
} fsyscmd[] = {
	{ "close",	fsysClose, },
	{ "config",	nil, fsysConfig, },
	{ "open",	nil, fsysOpen, },
	{ "unconfig",	nil, fsysUnconfig, },
	{ "venti",	nil, fsysVenti, },

	{ "bfree",	fsysBfree, },
	{ "block",	fsysBlock, },
	{ "check",	fsysCheck, },
	{ "clre",	fsysClre, },
	{ "clri",	fsysClri, },
	{ "clrp",	fsysClrp, },
	{ "create",	fsysCreate, },
	{ "df",		fsysDf, },
	{ "epoch",	fsysEpoch, },
	{ "halt",	fsysHalt, },
	{ "label",	fsysLabel, },
	{ "remove",	fsysRemove, },
	{ "snap",	fsysSnap, },
	{ "snaptime",	fsysSnapTime, },
	{ "snapclean",	fsysSnapClean, },
	{ "stat",	fsysStat, },
	{ "sync",	fsysSync, },
	{ "unhalt",	fsysUnhalt, },
	{ "wstat",	fsysWstat, },
	{ "vac",	fsysVac, },

	{ nil,		nil, },
};

static int
fsysXXX1(Fsys *fsys, int i, int argc, char* argv[])
{
	int r;

	vtLock(fsys->lock);
	if(fsys->fs == nil){
		vtUnlock(fsys->lock);
		vtSetError(EFsysNotOpen, fsys->name);
		return 0;
	}

	if(fsys->fs->halted
	&& fsyscmd[i].f != fsysUnhalt && fsyscmd[i].f != fsysCheck){
		vtSetError("file system %s is halted", fsys->name);
		vtUnlock(fsys->lock);
		return 0;
	}

	r = (*fsyscmd[i].f)(fsys, argc, argv);
	vtUnlock(fsys->lock);
	return r;
}

static int
fsysXXX(char* name, int argc, char* argv[])
{
	int i, r;
	Fsys *fsys;

	for(i = 0; fsyscmd[i].cmd != nil; i++){
		if(strcmp(fsyscmd[i].cmd, argv[0]) == 0)
			break;
	}

	if(fsyscmd[i].cmd == nil){
		vtSetError("unknown command - '%s'", argv[0]);
		return 0;
	}

	/* some commands want the name... */
	if(fsyscmd[i].f1 != nil){
		if(strcmp(name, FsysAll) == 0){
			vtSetError("cannot use fsys %#q with %#q command", FsysAll, argv[0]);
			return 0;
		}
		return (*fsyscmd[i].f1)(name, argc, argv);
	}

	/* ... but most commands want the Fsys */
	if(strcmp(name, FsysAll) == 0){
		r = 1;
		vtRLock(sbox.lock);
		for(fsys = sbox.head; fsys != nil; fsys = fsys->next){
			fsys->ref++;
			r = fsysXXX1(fsys, i, argc, argv) && r;
			fsys->ref--;
		}
		vtRUnlock(sbox.lock);
	}else{
		if((fsys = _fsysGet(name)) == nil)
			return 0;
		r = fsysXXX1(fsys, i, argc, argv);
		fsysPut(fsys);
	}
	return r;
}

static int
cmdFsysXXX(int argc, char* argv[])
{
	char *name;

	if((name = sbox.curfsys) == nil){
		vtSetError(EFsysNoCurrent, argv[0]);
		return 0;
	}

	return fsysXXX(name, argc, argv);
}

static int
cmdFsys(int argc, char* argv[])
{
	Fsys *fsys;
	char *usage = "usage: fsys [name ...]";

	ARGBEGIN{
	default:
		return cliError(usage);
	}ARGEND

	if(argc == 0){
		vtRLock(sbox.lock);
		currfsysname = sbox.head->name;
		for(fsys = sbox.head; fsys != nil; fsys = fsys->next)
			consPrint("\t%s\n", fsys->name);
		vtRUnlock(sbox.lock);
		return 1;
	}
	if(argc == 1){
		fsys = nil;
		if(strcmp(argv[0], FsysAll) != 0 && (fsys = fsysGet(argv[0])) == nil)
			return 0;
		sbox.curfsys = vtStrDup(argv[0]);
		consPrompt(sbox.curfsys);
		if(fsys)
			fsysPut(fsys);
		return 1;
	}

	return fsysXXX(argv[0], argc-1, argv+1);
}

int
fsysInit(void)
{
	int i;

	fmtinstall('H', encodefmt);
	fmtinstall('V', scoreFmt);
	fmtinstall('R', vtErrFmt);
	fmtinstall('L', labelFmt);

	sbox.lock = vtLockAlloc();

	cliAddCmd("fsys", cmdFsys);
	for(i = 0; fsyscmd[i].cmd != nil; i++){
		if(fsyscmd[i].f != nil)
			cliAddCmd(fsyscmd[i].cmd, cmdFsysXXX);
	}
	/* the venti cmd is special: the fs can be either open or closed */
	cliAddCmd("venti", cmdFsysXXX);
	cliAddCmd("printconfig", cmdPrintConfig);

	return 1;
}
