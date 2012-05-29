#include <u.h>
#include <libc.h>
#include <venti.h>
#include <libsec.h>
#include <thread.h>

enum
{
	// XXX What to do here?
	VtMaxLumpSize = 65535,
};

int chatty;

void
usage(void)
{
	fprint(2, "usage: readfile [-v] [-h host] score\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	int n;
	uchar score[VtScoreSize];
	uchar *buf;
	char *host, *type;
	vlong off;
	VtEntry e;
	VtRoot root;
	VtCache *c;
	VtConn *z;
	VtFile *f;

	quotefmtinstall();
	fmtinstall('F', vtfcallfmt);
	fmtinstall('V', vtscorefmt);

	host = nil;
	ARGBEGIN{
	case 'V':
		chattyventi++;
		break;
	case 'h':
		host = EARGF(usage());
		break;
	case 'v':
		chatty++;
		break;
	default:
		usage();
		break;
	}ARGEND

	if(argc != 1)
		usage();

	type = nil;
	if(vtparsescore(argv[0], &type, score) < 0)
		sysfatal("could not parse score '%s': %r", argv[0]);
	if(type == nil || strcmp(type, "file") != 0)
		sysfatal("bad score - not file:...");

	buf = vtmallocz(VtMaxLumpSize);

	z = vtdial(host);
	if(z == nil)
		sysfatal("could not connect to server: %r");

	if(vtconnect(z) < 0)
		sysfatal("vtconnect: %r");

	// root block ...
	n = vtread(z, score, VtRootType, buf, VtMaxLumpSize);
	if(n < 0)
		sysfatal("could not read root %V: %r", score);
	if(n != VtRootSize)
		sysfatal("root block %V is wrong size %d != %d", score, n, VtRootSize);
	if(vtrootunpack(&root, buf) < 0)
		sysfatal("unpacking root block %V: %r", score);
	if(strcmp(root.type, "file") != 0)
		sysfatal("bad root type %q (not 'file')", root.type);
	if(chatty)
		fprint(2, "%V: %q %q %V %d %V\n",
			score, root.name, root.type,
			root.score, root.blocksize, root.prev);

	// ... points at entry block
	n = vtread(z, root.score, VtDirType, buf, VtMaxLumpSize);
	if(n < 0)
		sysfatal("could not read entry %V: %r", root.score);
	if(n != VtEntrySize)
		sysfatal("dir block %V is wrong size %d != %d", root.score, n, VtEntrySize);
	if(vtentryunpack(&e, buf, 0) < 0)
		sysfatal("unpacking dir block %V: %r", root.score);
	if((e.type&VtTypeBaseMask) != VtDataType)
		sysfatal("not a single file");

	// open and read file
	c = vtcachealloc(z, root.blocksize*32);
	if(c == nil)
		sysfatal("vtcachealloc: %r");
	f = vtfileopenroot(c, &e);
	if(f == nil)
		sysfatal("vtfileopenroot: %r");
	off = 0;
	vtfilelock(f, VtOREAD);
	while((n = vtfileread(f, buf, VtMaxLumpSize, off)) > 0){
		write(1, buf, n);
		off += n;
	}
	threadexitsall(0);
}
