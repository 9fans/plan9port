#include "stdinc.h"
#include <bio.h>
#include "dat.h"
#include "fns.h"

Biobuf bout;
Fsck fsck;

static void
usage(void)
{
	fprint(2, "usage: %s [-c cachesize] [-h host] file\n", argv0);
	threadexitsall("usage");
}

#pragma	varargck	argpos	flprint	1

static int
flprint(char *fmt, ...)
{
	int n;
	va_list arg;

	va_start(arg, fmt);
	n = Bvprint(&bout, fmt, arg);
	va_end(arg);
	return n;
}

static void
flclre(Fsck *chk, Block *b, int o)
{
	USED(chk);
	Bprint(&bout, "# clre 0x%ux %d\n", b->addr, o);
}

static void
flclrp(Fsck *chk, Block *b, int o)
{
	USED(chk);
	Bprint(&bout, "# clrp 0x%ux %d\n", b->addr, o);
}

static void
flclri(Fsck *chk, char *name, MetaBlock *mb, int i, Block *b)
{
	USED(chk);
	USED(mb);
	USED(i);
	USED(b);
	Bprint(&bout, "# clri %s\n", name);
}

static void
flclose(Fsck *chk, Block *b, u32int epoch)
{
	USED(chk);
	Bprint(&bout, "# bclose 0x%ux %ud\n", b->addr, epoch);
}

void
threadmain(int argc, char *argv[])
{
	int csize = 1000;
	VtConn *z;
	char *host = nil;

	fsck.useventi = 1;
	Binit(&bout, 1, OWRITE);
	ARGBEGIN{
	default:
		usage();
	case 'c':
		csize = atoi(ARGF());
		if(csize <= 0)
			usage();
		break;
	case 'f':
		fsck.useventi = 0;
		break;
	case 'h':
		host = ARGF();
		break;
	case 'v':
		fsck.printdirs = 1;
		break;
	}ARGEND;

	if(argc != 1)
		usage();

	fmtinstall('L', labelFmt);
	fmtinstall('V', scoreFmt);

	/*
	 * Connect to Venti.
	 */
	z = vtdial(host);
	if(z == nil){
		if(fsck.useventi)
			sysfatal("could not connect to server: %r");
	}else if(vtconnect(z) < 0)
		sysfatal("vtconnect: %r");

	/*
	 * Initialize file system.
	 */
	fsck.fs = fsOpen(argv[0], z, csize, OReadOnly);
	if(fsck.fs == nil)
		sysfatal("could not open file system: %r");

	fsck.print = flprint;
	fsck.clre = flclre;
	fsck.clrp = flclrp;
	fsck.close = flclose;
	fsck.clri = flclri;

	fsCheck(&fsck);

	threadexitsall(0);
}
