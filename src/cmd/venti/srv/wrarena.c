#include "stdinc.h"
#include "dat.h"
#include "fns.h"

QLock godot;
char *host;
int readonly = 1;	/* for part.c */
int mainstacksize = 256*1024;
Channel *c;
VtConn *z;
int fast;	/* and a bit unsafe; only for benchmarking */
int haveaoffset;
int maxwrites = -1;

typedef struct ZClump ZClump;
struct ZClump
{
	ZBlock *lump;
	Clump cl;
	u64int aa;
};

void
usage(void)
{
	fprint(2, "usage: wrarena [-h host] arenafile [offset]\n");
	threadexitsall("usage");
}

void
vtsendthread(void *v)
{
	ZClump zcl;

	USED(v);
	while(recv(c, &zcl) == 1){
		if(zcl.lump == nil)
			break;
		if(vtwrite(z, zcl.cl.info.score, zcl.cl.info.type, zcl.lump->data, zcl.cl.info.uncsize) < 0)
			sysfatal("failed writing clump %llud: %r", zcl.aa);
		freezblock(zcl.lump);
	}
	/*
	 * All the send threads try to exit right when
	 * threadmain is calling threadexitsall.  
	 * Either libthread or the Linux NPTL pthreads library
	 * can't handle this condition (I suspect NPTL but have
	 * not confirmed this) and we get a seg fault in exit.
	 * I spent a day tracking this down with no success,
	 * so we're going to work around it instead by just
	 * sitting here and waiting for the threadexitsall to
	 * take effect.
	 */
	qlock(&godot);
}

static void
rdarena(Arena *arena, u64int offset)
{
	u64int a, aa, e;
	u32int magic;
	Clump cl;
	uchar score[VtScoreSize];
	ZBlock *lump;
	ZClump zcl;

	fprint(2, "wrarena: copying %s to venti\n", arena->name);
	printarena(2, arena);

	a = arena->base;
	e = arena->base + arena->size;
	if(offset != ~(u64int)0) {
		if(offset >= e-a)
			sysfatal("bad offset %llud >= %llud\n",
				offset, e-a);
		aa = offset;
	} else
		aa = 0;

	if(maxwrites != 0)
	for(; aa < e; aa += ClumpSize+cl.info.size) {
		magic = clumpmagic(arena, aa);
		if(magic == ClumpFreeMagic)
			break;
		if(magic != arena->clumpmagic) {
			if(0) fprint(2, "illegal clump magic number %#8.8ux offset %llud\n",
				magic, aa);
			break;
		}
		lump = loadclump(arena, aa, 0, &cl, score, 0);
		if(lump == nil) {
			fprint(2, "clump %llud failed to read: %r\n", aa);
			break;
		}
		if(!fast && cl.info.type != VtCorruptType) {
			scoremem(score, lump->data, cl.info.uncsize);
			if(scorecmp(cl.info.score, score) != 0) {
				fprint(2, "clump %llud has mismatched score\n", aa);
				break;
			}
			if(vttypevalid(cl.info.type) < 0) {
				fprint(2, "clump %llud has bad type %d\n", aa, cl.info.type);
				break;
			}
		}
		if(z && cl.info.type != VtCorruptType){
			zcl.cl = cl;
			zcl.lump = lump;
			zcl.aa = aa;
			send(c, &zcl);
		}else
			freezblock(lump);
		if(maxwrites>0 && --maxwrites == 0)
			break;
	}
	if(haveaoffset)
		print("end offset %llud\n", aa);
}

void
threadmain(int argc, char *argv[])
{
	int i;
	char *file;
	Arena *arena;
	u64int offset, aoffset;
	Part *part;
	Dir *d;
	uchar buf[8192];
	ArenaHead head;
	ZClump zerocl;

	qlock(&godot);
	aoffset = 0;
	ARGBEGIN{
	case 'f':
		fast = 1;
		ventidoublechecksha1 = 0;
		break;
	case 'h':
		host = EARGF(usage());
		break;
	case 'o':
		haveaoffset = 1;
		aoffset = strtoull(EARGF(usage()), 0, 0);
		break;
	case 'M':
		maxwrites = atoi(EARGF(usage()));
		break;
	default:
		usage();
		break;
	}ARGEND

	offset = ~(u64int)0;
	switch(argc) {
	default:
		usage();
	case 2:
		offset = strtoull(argv[1], 0, 0);
		/* fall through */
	case 1:
		file = argv[0];
	}

	fmtinstall('V', vtscorefmt);

	statsinit();

	if((d = dirstat(file)) == nil)
		sysfatal("can't stat file %s: %r", file);

	part = initpart(file, OREAD);
	if(part == nil)
		sysfatal("can't open file %s: %r", file);
	if(readpart(part, aoffset, buf, sizeof buf) < 0)
		sysfatal("can't read file %s: %r", file);

	if(unpackarenahead(&head, buf) < 0)
		sysfatal("corrupted arena header: %r");

	if(aoffset+head.size > d->length)
		sysfatal("arena is truncated: want %llud bytes have %llud\n",
			head.size, d->length);

	partblocksize(part, head.blocksize);
	initdcache(8 * MaxDiskBlock);

	arena = initarena(part, aoffset, head.size, head.blocksize);
	if(arena == nil)
		sysfatal("initarena: %r");

	z = nil;
	if(host==nil || strcmp(host, "/dev/null") != 0){
		z = vtdial(host);
		if(z == nil)
			sysfatal("could not connect to server: %r");
		if(vtconnect(z) < 0)
			sysfatal("vtconnect: %r");
	}
	
	c = chancreate(sizeof(ZClump), 0);
	for(i=0; i<12; i++)
		vtproc(vtsendthread, nil);

	rdarena(arena, offset);
		if(vtsync(z) < 0)
			sysfatal("executing sync: %r");

	memset(&zerocl, 0, sizeof zerocl);
	for(i=0; i<12; i++)
		send(c, &zerocl);
	if(z){
		vthangup(z);
	}
	threadexitsall(0);
}
