#include "stdinc.h"
#include "dat.h"
#include "fns.h"

#include "whack.h"

int debug;
int mainstacksize = 256*1024;

static void	ventiserver(char *vaddr);

void
threadmain(int argc, char *argv[])
{
	char *config, *haddr, *vaddr;
	u32int mem, icmem, bcmem, minbcmem;

	vaddr = "tcp!*!venti";
	haddr = nil;
	config = nil;
	mem = 0xffffffffUL;
	icmem = 0;
	bcmem = 0;
	ARGBEGIN{
	case 'a':
		vaddr = ARGF();
		if(vaddr == nil)
			goto usage;
		break;
	case 'B':
		bcmem = unittoull(ARGF());
		break;
	case 'c':
		config = ARGF();
		if(config == nil)
			goto usage;
		break;
	case 'C':
		mem = unittoull(ARGF());
		break;
	case 'd':
		debug = 1;
		break;
	case 'h':
		haddr = ARGF();
		if(haddr == nil)
			goto usage;
		break;
	case 'I':
		icmem = unittoull(ARGF());
		break;
	case 'w':
		queuewrites = 1;
		break;
	default:
		goto usage;
	}ARGEND

print("whack %d\n", sizeof(Whack));

	if(argc){
  usage:
		fprint(2, "usage: venti [-dw] [-a ventiaddress] [-h httpaddress] [-c config] [-C cachesize] [-I icachesize] [-B blockcachesize]\n");
		threadexitsall("usage");
	}

	fmtinstall('V', vtscorefmt);
	fmtinstall('H', encodefmt);
	fmtinstall('F', vtfcallfmt);

	if(config == nil)
		config = "venti.conf";


	if(initarenasum() < 0)
		fprint(2, "warning: can't initialize arena summing process: %r");

	if(initventi(config) < 0)
		sysfatal("can't init server: %r");

	if(mem == 0xffffffffUL)
		mem = 1 * 1024 * 1024;
	fprint(2, "initialize %d bytes of lump cache for %d lumps\n",
		mem, mem / (8 * 1024));
	initlumpcache(mem, mem / (8 * 1024));

	icmem = u64log2(icmem / (sizeof(IEntry)+sizeof(IEntry*)) / ICacheDepth);
	if(icmem < 4)
		icmem = 4;
	fprint(2, "initialize %d bytes of index cache for %d index entries\n",
		(sizeof(IEntry)+sizeof(IEntry*)) * (1 << icmem) * ICacheDepth,
		(1 << icmem) * ICacheDepth);
	initicache(icmem, ICacheDepth);

	/*
	 * need a block for every arena and every process
	 */
	minbcmem = maxblocksize * 
		(mainindex->narenas + mainindex->nsects*4 + 16);
	if(bcmem < minbcmem)
		bcmem = minbcmem;

	fprint(2, "initialize %d bytes of disk block cache\n", bcmem);
	initdcache(bcmem);

	fprint(2, "sync arenas and index...\n");
	if(syncindex(mainindex, 1) < 0)
		sysfatal("can't sync server: %r");

	if(queuewrites){
		fprint(2, "initialize write queue...\n");
		if(initlumpqueues(mainindex->nsects) < 0){
			fprint(2, "can't initialize lump queues,"
				" disabling write queueing: %r");
			queuewrites = 0;
		}
	}

	if(haddr){
		fprint(2, "starting http server at %s\n", haddr);
		if(httpdinit(haddr) < 0)
			fprint(2, "warning: can't start http server: %r");
	}

	ventiserver(vaddr);
	threadexitsall(0);
}

static void
vtrerror(VtReq *r, char *error)
{
	r->rx.type = VtRerror;
	r->rx.error = estrdup(error);
}

static void
ventiserver(char *addr)
{
	Packet *p;
	VtReq *r;
	VtSrv *s;

	s = vtlisten(addr);
	if(s == nil)
		sysfatal("can't announce %s: %r", addr);

	while((r = vtgetreq(s)) != nil){
		r->rx.type = r->tx.type+1;
		print("req (arenas[0]=%p sects[0]=%p) %F\n",
			mainindex->arenas[0], mainindex->sects[0], &r->tx);
		switch(r->tx.type){
		default:
			vtrerror(r, "unknown request");
			break;
		case VtTread:
			if((r->rx.data = readlump(r->tx.score, r->tx.dtype, r->tx.count)) == nil)
				vtrerror(r, gerrstr());
			break;
		case VtTwrite:
			p = r->tx.data;
			r->tx.data = nil;
			if(writelump(p, r->rx.score, r->tx.dtype, 0) < 0)	
				vtrerror(r, gerrstr());
			break;
		case VtTsync:
			queueflush();
			break;
		}
		vtrespond(r);
	}
}

