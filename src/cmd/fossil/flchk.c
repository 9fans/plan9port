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
	exits("usage");
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
flclre(Fsck*, Block *b, int o)
{
	Bprint(&bout, "# clre 0x%ux %d\n", b->addr, o);
}

static void
flclrp(Fsck*, Block *b, int o)
{
	Bprint(&bout, "# clrp 0x%ux %d\n", b->addr, o);
}

static void
flclri(Fsck*, char *name, MetaBlock*, int, Block*)
{
	Bprint(&bout, "# clri %s\n", name);
}

static void
flclose(Fsck*, Block *b, u32int epoch)
{
	Bprint(&bout, "# bclose 0x%ux %ud\n", b->addr, epoch);
}

void
main(int argc, char *argv[])
{
	int csize = 1000;
	VtSession *z;
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

	vtAttach();

	fmtinstall('L', labelFmt);
	fmtinstall('V', scoreFmt);
	fmtinstall('R', vtErrFmt);

	/*
	 * Connect to Venti.
	 */
	z = vtDial(host, 0);
	if(z == nil){
		if(fsck.useventi)
			vtFatal("could not connect to server: %s", vtGetError());
	}else if(!vtConnect(z, 0))
		vtFatal("vtConnect: %s", vtGetError());

	/*
	 * Initialize file system.
	 */
	fsck.fs = fsOpen(argv[0], z, csize, OReadOnly);
	if(fsck.fs == nil)
		vtFatal("could not open file system: %R");

	fsck.print = flprint;
	fsck.clre = flclre;
	fsck.clrp = flclrp;
	fsck.close = flclose;
	fsck.clri = flclri;

	fsCheck(&fsck);

	exits(0);
}

