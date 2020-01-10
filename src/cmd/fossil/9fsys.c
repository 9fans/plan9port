#include "stdinc.h"
#include <bio.h>
#include "dat.h"
#include "fns.h"
#include "9.h"

struct Fsys {
	QLock	lock;

	char*	name;		/* copy here & Fs to ease error reporting */
	char*	dev;
	char*	venti;

	Fs*	fs;
	VtConn* session;
	int	ref;

	int	noauth;
	int	noperm;
	int	wstatallow;

	Fsys*	next;
};

int mempcnt;			/* from fossil.c */

int	fsGetBlockSize(Fs *fs);

static struct {
	RWLock	lock;
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
		return vtstrdup(host);
	host = getenv("venti");
	if(host == nil)
		host = vtstrdup("$venti");
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

static VtConn *
myDial(char *host)
{
	prventihost(host);
	return vtdial(host);
}

static int
myRedial(VtConn *z, char *host)
{
	prventihost(host);
	return vtredial(z, host);
}

static Fsys*
_fsysGet(char* name)
{
	Fsys *fsys;

	if(name == nil || name[0] == '\0')
		name = "main";

	rlock(&sbox.lock);
	for(fsys = sbox.head; fsys != nil; fsys = fsys->next){
		if(strcmp(name, fsys->name) == 0){
			fsys->ref++;
			break;
		}
	}
	runlock(&sbox.lock);
	if(fsys == nil)
		werrstr(EFsysNotFound, name);
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

	rlock(&sbox.lock);
	for(fsys = sbox.head; fsys != nil; fsys = fsys->next){
		consPrint("\tfsys %s config %s\n", fsys->name, fsys->dev);
		if(fsys->venti && fsys->venti[0])
			consPrint("\tfsys %s venti %q\n", fsys->name,
				fsys->venti);
	}
	runlock(&sbox.lock);
	return 1;
}

Fsys*
fsysGet(char* name)
{
	Fsys *fsys;

	if((fsys = _fsysGet(name)) == nil)
		return nil;

	qlock(&fsys->lock);
	if(fsys->fs == nil){
		werrstr(EFsysNotOpen, fsys->name);
		qunlock(&fsys->lock);
		fsysPut(fsys);
		return nil;
	}
	qunlock(&fsys->lock);

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
	wlock(&sbox.lock);
	fsys->ref++;
	wunlock(&sbox.lock);

	return fsys;
}

void
fsysPut(Fsys* fsys)
{
	wlock(&sbox.lock);
	assert(fsys->ref > 0);
	fsys->ref--;
	wunlock(&sbox.lock);
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
	rlock(&fsys->fs->elk);
}

void
fsysFsRUnlock(Fsys* fsys)
{
	runlock(&fsys->fs->elk);
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

	wlock(&sbox.lock);
	for(fsys = sbox.head; fsys != nil; fsys = fsys->next){
		if(strcmp(fsys->name, name) != 0)
			continue;
		werrstr(EFsysExists, name);
		wunlock(&sbox.lock);
		return nil;
	}

	fsys = vtmallocz(sizeof(Fsys));
	fsys->name = vtstrdup(name);
	fsys->dev = vtstrdup(dev);

	fsys->ref = 1;

	if(sbox.tail != nil)
		sbox.tail->next = fsys;
	else
		sbox.head = fsys;
	sbox.tail = fsys;
	wunlock(&sbox.lock);

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
	vtfreeconn(fsys->session);
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

	rlock(&fsys->fs->elk);
	while(argc > 0){
		if((file = fileOpen(fsys->fs, argv[0])) == nil)
			consPrint("%s: %r\n", argv[0]);
		else{
			if(!fileRemove(file, uidadm))
				consPrint("%s: %r\n", argv[0]);
			fileDecRef(file);
		}
		argc--;
		argv++;
	}
	runlock(&fsys->fs->elk);

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

	rlock(&fsys->fs->elk);
	while(argc > 0){
		if(!fileClriPath(fsys->fs, argv[0], uidadm))
			consPrint("clri %s: %r\n", argv[0]);
		argc--;
		argv++;
	}
	runlock(&fsys->fs->elk);

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
	rlock(&fsys->fs->elk);

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
					rsleep(&bb->ioready);
				}
				break;
			}
			consPrint("blockWrite: %r\n");
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
	runlock(&fs->elk);

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
		werrstr("bad offset");
		return 0;
	}
	if(argc > 2)
		count = strtoul(argv[2], 0, 0);
	else
		count = 100000000;
	if(offset+count > fs->blockSize)
		count = fs->blockSize - count;

	rlock(&fs->elk);

	b = cacheLocal(fs->cache, PartData, addr, argc==4 ? OReadWrite : OReadOnly);
	if(b == nil){
		werrstr("cacheLocal %#ux: %r", addr);
		runlock(&fs->elk);
		return 0;
	}

	consPrint("\t%sblock %#ux %ud %ud %.*H\n",
		argc==4 ? "old: " : "", addr, offset, count, count, b->data+offset);

	if(argc == 4){
		s = argv[3];
		if(strlen(s) != 2*count){
			werrstr("bad data count");
			goto Out;
		}
		buf = vtmallocz(count);
		for(i = 0; i < count*2; i++){
			if(s[i] >= '0' && s[i] <= '9')
				c = s[i] - '0';
			else if(s[i] >= 'a' && s[i] <= 'f')
				c = s[i] - 'a' + 10;
			else if(s[i] >= 'A' && s[i] <= 'F')
				c = s[i] - 'A' + 10;
			else{
				werrstr("bad hex");
				vtfree(buf);
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
	runlock(&fs->elk);

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
	rlock(&fs->elk);
	while(argc > 0){
		addr = strtoul(argv[0], &p, 0);
		if(*p != '\0'){
			consPrint("bad address - '%ud'\n", addr);
			/* syntax error; let's stop */
			runlock(&fs->elk);
			return 0;
		}
		b = cacheLocal(fs->cache, PartData, addr, OReadOnly);
		if(b == nil){
			consPrint("loading %#ux: %r\n", addr);
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
				consPrint("freeing %#ux: %r\n", addr);
		}
		blockPut(b);
		argc--;
		argv++;
	}
	runlock(&fs->elk);

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
	rlock(&fsys->fs->elk);

	addr = strtoul(argv[0], 0, 0);
	b = cacheLocal(fs->cache, PartData, addr, argc==4 ? OReadWrite : OReadOnly);
	if(b == nil){
		werrstr("cacheLocal %#ux: %r", addr);
	Err:
		runlock(&fsys->fs->elk);
		return 0;
	}

	switch(ch){
	default:
		werrstr("clrep");
		goto Err;
	case 'e':
		if(b->l.type != BtDir){
			werrstr("wrong block type");
			goto Err;
		}
		sz = VtEntrySize;
		memset(&e, 0, sizeof e);
		entryPack(&e, zero, 0);
		break;
	case 'p':
		if(b->l.type == BtDir || b->l.type == BtData){
			werrstr("wrong block type");
			goto Err;
		}
		sz = VtScoreSize;
		memmove(zero, vtzeroscore, VtScoreSize);
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
	runlock(&fsys->fs->elk);

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
			consPrint("\tdeeRead %s/%s: %r\n", s, de.elem);
			break;
		}
		if(r == 0)
			break;
		if(de.mode & ModeSnapshot){
			if((ff = fileWalk(f, de.elem)) == nil)
				consPrint("\tcannot walk %s/%s: %r\n", s, de.elem);
			else{
				if(!fileGetSources(ff, &e, &ee))
					consPrint("\tcannot get sources for %s/%s: %r\n", s, de.elem);
				else if(e.snap != 0 && e.snap < elo){
					consPrint("\t%ud\tclri %s/%s\n", e.snap, s, de.elem);
					n++;
				}
				fileDecRef(ff);
			}
		}
		else if(de.mode & ModeDir){
			if((ff = fileWalk(f, de.elem)) == nil)
				consPrint("\tcannot walk %s/%s: %r\n", s, de.elem);
			else{
				t = smprint("%s/%s", s, de.elem);
				n += fsysEsearch1(ff, t, elo);
				vtfree(t);
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
		consPrint("\tfileGetDir %s failed: %r\n", path);
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

	rlock(&fs->elk);
	consPrint("\tlow %ud hi %ud\n", fs->elo, fs->ehi);
	if(low == ~(u32int)0){
		runlock(&fs->elk);
		return 1;
	}
	n = fsysEsearch(fsys->fs, "/archive", low);
	n += fsysEsearch(fsys->fs, "/snapshot", low);
	consPrint("\t%d snapshot%s found with epoch < %ud\n", n, n==1 ? "" : "s", low);
	runlock(&fs->elk);

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
		consPrint("\tfsEpochLow: %r\n");
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

	rlock(&fsys->fs->elk);
	path = vtstrdup(argv[0]);
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
		werrstr("create %s/%s: %r", p, elem);
		goto out;
	}

	if(!fileGetDir(file, &de)){
		werrstr("stat failed after create: %r");
		goto out1;
	}

	if(strcmp(de.gid, argv[2]) != 0){
		vtfree(de.gid);
		de.gid = vtstrdup(argv[2]);
		if(!fileSetDir(file, &de, argv[1])){
			werrstr("wstat failed after create: %r");
			goto out2;
		}
	}
	r = 1;

out2:
	deCleanup(&de);
out1:
	fileDecRef(file);
out:
	vtfree(path);
	runlock(&fsys->fs->elk);

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

	rlock(&fsys->fs->elk);
	for(i=0; i<argc; i++){
		if((f = fileOpen(fsys->fs, argv[i])) == nil){
			consPrint("%s: %r\n", argv[i]);
			continue;
		}
		if(!fileGetDir(f, &de)){
			consPrint("%s: %r\n", argv[i]);
			fileDecRef(f);
			continue;
		}
		fsysPrintStat("\t", argv[i], &de);
		deCleanup(&de);
		fileDecRef(f);
	}
	runlock(&fsys->fs->elk);
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

	rlock(&fsys->fs->elk);
	if((f = fileOpen(fsys->fs, argv[0])) == nil){
		werrstr("console wstat - walk - %r");
		runlock(&fsys->fs->elk);
		return 0;
	}
	if(!fileGetDir(f, &de)){
		werrstr("console wstat - stat - %r");
		fileDecRef(f);
		runlock(&fsys->fs->elk);
		return 0;
	}
	fsysPrintStat("\told: w", argv[0], &de);

	if(strcmp(argv[1], "-") != 0){
		if(!validFileName(argv[1])){
			werrstr("console wstat - bad elem");
			goto error;
		}
		vtfree(de.elem);
		de.elem = vtstrdup(argv[1]);
	}
	if(strcmp(argv[2], "-") != 0){
		if(!validUserName(argv[2])){
			werrstr("console wstat - bad uid");
			goto error;
		}
		vtfree(de.uid);
		de.uid = vtstrdup(argv[2]);
	}
	if(strcmp(argv[3], "-") != 0){
		if(!validUserName(argv[3])){
			werrstr("console wstat - bad gid");
			goto error;
		}
		vtfree(de.gid);
		de.gid = vtstrdup(argv[3]);
	}
	if(strcmp(argv[4], "-") != 0){
		if(!fsysParseMode(argv[4], &de.mode)){
			werrstr("console wstat - bad mode");
			goto error;
		}
	}
	if(strcmp(argv[5], "-") != 0){
		de.size = strtoull(argv[5], &p, 0);
		if(argv[5][0] == '\0' || *p != '\0' || (vlong)de.size < 0){
			werrstr("console wstat - bad length");
			goto error;
		}
	}

	if(!fileSetDir(f, &de, uidadm)){
		werrstr("console wstat - %r");
		goto error;
	}
	deCleanup(&de);

	if(!fileGetDir(f, &de)){
		werrstr("console wstat - stat2 - %r");
		goto error;
	}
	fsysPrintStat("\tnew: w", argv[0], &de);
	deCleanup(&de);
	fileDecRef(f);
	runlock(&fsys->fs->elk);

	return 1;

error:
	deCleanup(&de);	/* okay to do this twice */
	fileDecRef(f);
	runlock(&fsys->fs->elk);
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
		consPrint("%#ux setlabel: %r\n", b->addr);
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
	memmove(b->data+offset*VtScoreSize, vtzeroscore, VtScoreSize);
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

	qlock(&fsys->lock);
	if(host == nil)
		host = fsys->venti;
	else{
		vtfree(fsys->venti);
		if(host[0])
			fsys->venti = vtstrdup(host);
		else{
			host = nil;
			fsys->venti = nil;
		}
	}

	/* already open: do a redial */
	if(fsys->fs != nil){
		if(fsys->session == nil){
			werrstr("file system was opened with -V");
			r = 0;
			goto out;
		}
		r = 1;
		if(myRedial(fsys->session, host) < 0
		|| vtconnect(fsys->session) < 0)
			r = 0;
		goto out;
	}

	/* not yet open: try to dial */
	if(fsys->session)
		vtfreeconn(fsys->session);
	r = 1;
	if((fsys->session = myDial(host)) == nil
	|| vtconnect(fsys->session) < 0)
		r = 0;
out:
	qunlock(&fsys->lock);
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

	qlock(&fsys->lock);
	if(fsys->fs != nil){
		werrstr(EFsysBusy, fsys->name);
		qunlock(&fsys->lock);
		fsysPut(fsys);
		return 0;
	}

	if(noventi){
		if(fsys->session){
			vtfreeconn(fsys->session);
			fsys->session = nil;
		}
	}
	else if(fsys->session == nil){
		if(fsys->venti && fsys->venti[0])
			host = fsys->venti;
		else
			host = nil;

		if((fsys->session = myDial(host)) == nil
		|| vtconnect(fsys->session) < 0 && !noventi)
			fprint(2, "warning: connecting to venti: %r\n");
	}
	if((fsys->fs = fsOpen(fsys->dev, fsys->session, ncache, rflag)) == nil){
		werrstr("fsOpen: %r");
		qunlock(&fsys->lock);
		fsysPut(fsys);
		return 0;
	}
	fsys->fs->name = fsys->name;	/* for better error messages */
	fsys->noauth = noauth;
	fsys->noperm = noperm;
	fsys->wstatallow = wstatallow;
	fsys->fs->noatimeupd = noatimeupd;
	qunlock(&fsys->lock);
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

	wlock(&sbox.lock);
	fp = &sbox.head;
	for(fsys = *fp; fsys != nil; fsys = fsys->next){
		if(strcmp(fsys->name, name) == 0)
			break;
		fp = &fsys->next;
	}
	if(fsys == nil){
		werrstr(EFsysNotFound, name);
		wunlock(&sbox.lock);
		return 0;
	}
	if(fsys->ref != 0 || fsys->fs != nil){
		werrstr(EFsysBusy, fsys->name);
		wunlock(&sbox.lock);
		return 0;
	}
	*fp = fsys->next;
	wunlock(&sbox.lock);

	if(fsys->session != nil)
		vtfreeconn(fsys->session);
	if(fsys->venti != nil)
		vtfree(fsys->venti);
	if(fsys->dev != nil)
		vtfree(fsys->dev);
	if(fsys->name != nil)
		vtfree(fsys->name);
	vtfree(fsys);

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
		qlock(&fsys->lock);
		if(fsys->fs != nil){
			werrstr(EFsysBusy, fsys->name);
			qunlock(&fsys->lock);
			fsysPut(fsys);
			return 0;
		}
		vtfree(fsys->dev);
		fsys->dev = vtstrdup(part);
		qunlock(&fsys->lock);
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

	qlock(&fsys->lock);
	if(fsys->fs == nil){
		qunlock(&fsys->lock);
		werrstr(EFsysNotOpen, fsys->name);
		return 0;
	}

	if(fsys->fs->halted
	&& fsyscmd[i].f != fsysUnhalt && fsyscmd[i].f != fsysCheck){
		werrstr("file system %s is halted", fsys->name);
		qunlock(&fsys->lock);
		return 0;
	}

	r = (*fsyscmd[i].f)(fsys, argc, argv);
	qunlock(&fsys->lock);
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
		werrstr("unknown command - '%s'", argv[0]);
		return 0;
	}

	/* some commands want the name... */
	if(fsyscmd[i].f1 != nil){
		if(strcmp(name, FsysAll) == 0){
			werrstr("cannot use fsys %#q with %#q command", FsysAll, argv[0]);
			return 0;
		}
		return (*fsyscmd[i].f1)(name, argc, argv);
	}

	/* ... but most commands want the Fsys */
	if(strcmp(name, FsysAll) == 0){
		r = 1;
		rlock(&sbox.lock);
		for(fsys = sbox.head; fsys != nil; fsys = fsys->next){
			fsys->ref++;
			r = fsysXXX1(fsys, i, argc, argv) && r;
			fsys->ref--;
		}
		runlock(&sbox.lock);
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
		werrstr(EFsysNoCurrent, argv[0]);
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
		rlock(&sbox.lock);
		currfsysname = sbox.head->name;
		for(fsys = sbox.head; fsys != nil; fsys = fsys->next)
			consPrint("\t%s\n", fsys->name);
		runlock(&sbox.lock);
		return 1;
	}
	if(argc == 1){
		fsys = nil;
		if(strcmp(argv[0], FsysAll) != 0 && (fsys = fsysGet(argv[0])) == nil)
			return 0;
		sbox.curfsys = vtstrdup(argv[0]);
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
	fmtinstall('L', labelFmt);

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
