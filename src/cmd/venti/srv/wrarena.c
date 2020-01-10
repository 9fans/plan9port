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
int verbose;

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
	fprint(2, "usage: wrarena [-o fileoffset] [-h host] arenafile [clumpoffset]\n");
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
		if(verbose)
			print("%V\n", zcl.cl.info.score);
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
	int i;
	u64int a, aa, e;
	uchar score[VtScoreSize];
	Clump cl;
	ClumpInfo ci;
	ZBlock *lump;
	ZClump zcl;

	fprint(2, "wrarena: copying %s to venti\n", arena->name);
	printarena(2, arena);

	a = arena->base;
	e = arena->base + arena->size;
	if(offset != ~(u64int)0) {
		if(offset >= e - a)
			sysfatal("bad offset %#llx >= %#llx", offset, e - a);
		aa = offset;
	} else
		aa = 0;

	i = 0;
	for(a = 0; maxwrites != 0 && i < arena->memstats.clumps;
	    a += ClumpSize + ci.size){
		if(readclumpinfo(arena, i++, &ci) < 0)
			break;
		if(a < aa || ci.type == VtCorruptType){
			if(ci.type == VtCorruptType)
				fprint(2, "%s: corrupt clump read at %#llx: +%d\n",
					argv0, a, ClumpSize+ci.size);
			continue;
		}
		lump = loadclump(arena, a, 0, &cl, score, 0);
		if(lump == nil) {
			fprint(2, "clump %#llx failed to read: %r\n", a);
			continue;
		}
		if(!fast && cl.info.type != VtCorruptType) {
			scoremem(score, lump->data, cl.info.uncsize);
			if(scorecmp(cl.info.score, score) != 0) {
				fprint(2, "clump %#llx has mismatched score\n",
					a);
				break;
			}
			if(vttypevalid(cl.info.type) < 0) {
				fprint(2, "clump %#llx has bad type %d\n",
					a, cl.info.type);
				break;
			}
		}
		if(z && cl.info.type != VtCorruptType){
			zcl.cl = cl;
			zcl.lump = lump;
			zcl.aa = a;
			send(c, &zcl);
		}else
			freezblock(lump);
		if(maxwrites > 0)
			--maxwrites;
	}
	if(a > aa)
		aa = a;
	if(haveaoffset)
		print("end offset %#llx\n", aa);
}

void
threadmain(int argc, char *argv[])
{
	int i;
	char *file;
	Arena *arena;
	ArenaPart *ap;
	u64int offset, aoffset;
	Part *part;
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
	case 'v':
		verbose = 1;
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

	ventifmtinstall();

	statsinit();

	part = initpart(file, OREAD);
	if(part == nil)
		sysfatal("can't open file %s: %r", file);

	// Try as arena partition.
	arena = nil;
	ap = initarenapart(part);
	if(ap != nil)
		goto loaded;

	if(readpart(part, aoffset, buf, sizeof buf) < 0)
		sysfatal("can't read file %s: %r", file);

	if(unpackarenahead(&head, buf) < 0)
		sysfatal("corrupted arena header: %r");

	if(aoffset+head.size > part->size)
		sysfatal("arena is truncated: want %llud bytes have %llud",
			head.size, part->size);

	partblocksize(part, head.blocksize);

	arena = initarena(part, aoffset, head.size, head.blocksize);
	if(arena == nil)
		sysfatal("initarena: %r");

loaded:
	z = nil;
	if(host==nil || strcmp(host, "/dev/null") != 0){
		z = vtdial(host);
		if(z == nil)
			sysfatal("could not connect to server: %r");
		if(vtconnect(z) < 0)
			sysfatal("vtconnect: %r");
	}

	print("%T starting to send data\n");
	c = chancreate(sizeof(ZClump), 0);
	for(i=0; i<12; i++)
		vtproc(vtsendthread, nil);

	initdcache(8 * MaxDiskBlock);

	if(ap != nil) {
		for(i=0; i<ap->narenas; i++)
			rdarena(ap->arenas[i], 0);
	} else
		rdarena(arena, offset);

	memset(&zerocl, 0, sizeof zerocl);
	for(i=0; i<12; i++)
		send(c, &zerocl);
	if(vtsync(z) < 0)
		sysfatal("executing sync: %r");
	if(z){
		vthangup(z);
	}
	print("%T sent all data\n");

	threadexitsall(0);
}
