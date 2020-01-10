#include <u.h>
#include <libc.h>
#include <venti.h>
#include <libsec.h>
#include <thread.h>

enum
{
	Blocksize = 8192
};

int chatty;

void
usage(void)
{
	fprint(2, "usage: writefile [-v] [-h host] < data\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char *argv[])
{
	int n;
	uchar score[VtScoreSize];
	uchar *buf;
	char *host;
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

	if(argc != 0)
		usage();

	buf = vtmallocz(Blocksize);

	z = vtdial(host);
	if(z == nil)
		sysfatal("could not connect to server: %r");

	if(vtconnect(z) < 0)
		sysfatal("vtconnect: %r");

	// write file
	c = vtcachealloc(z, Blocksize*32);
	if(c == nil)
		sysfatal("vtcachealloc: %r");
	f = vtfilecreateroot(c, Blocksize, Blocksize, VtDataType);
	if(f == nil)
		sysfatal("vtfilecreateroot: %r");
	off = 0;
	vtfilelock(f, VtOWRITE);
	while((n = read(0, buf, Blocksize)) > 0){
		if(vtfilewrite(f, buf, n, off) != n)
			sysfatal("vtfilewrite: %r");
		off += n;
		if(vtfileflushbefore(f, off) < 0)
			sysfatal("vtfileflushbefore: %r");
	}
	if(vtfileflush(f) < 0)
		sysfatal("vtfileflush: %r");
	if(vtfilegetentry(f, &e) < 0)
		sysfatal("vtfilegetentry: %r");
	vtfileunlock(f);

	// write directory entry
	memset(&root, 0, sizeof root);
	vtentrypack(&e, buf, 0);
	if(vtwrite(z, root.score, VtDirType, buf, VtEntrySize) < 0)
		sysfatal("vtwrite dir: %r");

	// write root
	strcpy(root.name, "data");
	strcpy(root.type, "file");
	root.blocksize = Blocksize;
	vtrootpack(&root, buf);
	if(vtwrite(z, score, VtRootType, buf, VtRootSize) < 0)
		sysfatal("vtwrite root: %r");

	print("file:%V\n", score);
	threadexitsall(0);
}
