#include "stdinc.h"
#include "dat.h"
#include "fns.h"

void
usage(void)
{
	fprint(2, "usage: fmtindex [-a] config\n");
	threadexitsall(0);
}

void
threadmain(int argc, char *argv[])
{
	Config conf;
	Index *ix;
	ArenaPart *ap;
	Arena **arenas;
	AMap *amap;
	u64int addr;
	char *file;
	u32int i, j, n, narenas;
	int add;

	ventifmtinstall();
	statsinit();

	add = 0;
	ARGBEGIN{
	case 'a':
		add = 1;
		break;
	default:
		usage();
		break;
	}ARGEND

	if(argc != 1)
		usage();

	file = argv[0];

	if(runconfig(file, &conf) < 0)
		sysfatal("can't initialize config %s: %r", file);
	if(conf.index == nil)
		sysfatal("no index specified in %s", file);
	if(nameok(conf.index) < 0)
		sysfatal("illegal index name %s", conf.index);

	narenas = 0;
	for(i = 0; i < conf.naparts; i++){
		ap = conf.aparts[i];
		narenas += ap->narenas;
	}

	if(add){
		ix = initindex(conf.index, conf.sects, conf.nsects);
		if(ix == nil)
			sysfatal("can't initialize index %s: %r", conf.index);
	}else{
		ix = newindex(conf.index, conf.sects, conf.nsects);
		if(ix == nil)
			sysfatal("can't create new index %s: %r", conf.index);

		n = 0;
		for(i = 0; i < ix->nsects; i++)
			n += ix->sects[i]->blocks;

		if(0) fprint(2, "using %ud buckets of %ud; div=%d\n", ix->buckets, n, ix->div);
	}
	amap = MKNZ(AMap, narenas);
	arenas = MKNZ(Arena*, narenas);

	addr = IndexBase;
	n = 0;
	for(i = 0; i < conf.naparts; i++){
		ap = conf.aparts[i];
		for(j = 0; j < ap->narenas; j++){
			if(n >= narenas)
				sysfatal("too few slots in index's arena set");

			arenas[n] = ap->arenas[j];
			if(n < ix->narenas){
				if(arenas[n] != ix->arenas[n])
					sysfatal("mismatched arenas %s and %s at slot %d",
						arenas[n]->name, ix->arenas[n]->name, n);
				amap[n] = ix->amap[n];
				if(amap[n].start != addr)
					sysfatal("mis-located arena %s in index %s", arenas[n]->name, ix->name);
				addr = amap[n].stop;
			}else{
				amap[n].start = addr;
				addr += ap->arenas[j]->size;
				amap[n].stop = addr;
				namecp(amap[n].name, ap->arenas[j]->name);
				if(0) fprint(2, "add arena %s at [%lld,%lld)\n",
					amap[n].name, amap[n].start, amap[n].stop);
			}

			n++;
		}
	}
	if(0){
		fprint(2, "configured index=%s with arenas=%d and storage=%lld\n",
			ix->name, n, addr - IndexBase);
		fprint(2, "\tbuckets=%d\n",
			ix->buckets);
	}
	fprint(2, "fmtindex: %,d arenas, %,d index buckets, %,lld bytes storage\n",
		n, ix->buckets, addr-IndexBase);

	ix->amap = amap;
	ix->arenas = arenas;
	ix->narenas = narenas;

	if(wbindex(ix) < 0)
		fprint(2, "can't write back arena partition header for %s: %r\n", file);

	threadexitsall(0);
}
