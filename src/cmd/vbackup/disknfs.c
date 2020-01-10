#include <u.h>
#include <libc.h>
#include <ip.h>
#include <thread.h>
#include <sunrpc.h>
#include <nfs3.h>
#include <diskfs.h>
#include "nfs3srv.h"

Disk *disk;
Fsys *fsys;

void
usage(void)
{
	fprint(2, "usage: disknfs [-RTr] disk\n");
	threadexitsall("usage");
}

extern int _threaddebuglevel;

void
threadmain(int argc, char **argv)
{
	char *addr;
	SunSrv *srv;
	Channel *nfs3chan;
	Channel *mountchan;
	Nfs3Handle h;

	fmtinstall('B', sunrpcfmt);
	fmtinstall('C', suncallfmt);
	fmtinstall('H', encodefmt);
	fmtinstall('I', eipfmt);
	sunfmtinstall(&nfs3prog);
	sunfmtinstall(&nfsmount3prog);

	srv = sunsrv();
	addr = "*";

	ARGBEGIN{
	default:
		usage();
	case 'L':
		if(srv->localonly == 0)
			srv->localonly = 1;
		else
			srv->localparanoia = 1;
		break;
	case 'R':
		srv->chatty++;
		break;
	case 'T':
		_threaddebuglevel = 0xFFFFFFFF;
		break;
	case 'r':
		srv->alwaysreject++;
		break;
	}ARGEND

	if(argc != 1 && argc != 2)
		usage();

	if((disk = diskopenfile(argv[0])) == nil)
		sysfatal("diskopen: %r");
	if((disk = diskcache(disk, 16384, 256)) == nil)
		sysfatal("diskcache: %r");

	if((fsys = fsysopen(disk)) == nil)
		sysfatal("fsysopen: %r");

	nfs3chan = chancreate(sizeof(SunMsg*), 0);
	mountchan = chancreate(sizeof(SunMsg*), 0);

	if(argc > 1)
		addr = argv[1];
	addr = netmkaddr(addr, "udp", "2049");

	if(sunsrvudp(srv, addr) < 0)
		sysfatal("starting server: %r");

	sunsrvthreadcreate(srv, nfs3proc, nfs3chan);
	sunsrvthreadcreate(srv, mount3proc, mountchan);

	sunsrvprog(srv, &nfs3prog, nfs3chan);
	sunsrvprog(srv, &nfsmount3prog, mountchan);
	fsgetroot(&h);

	print("vmount0 -h %.*H %s /mnt\n", h.len, h.h, addr);

	threadexits(nil);
}

void
fsgetroot(Nfs3Handle *h)
{
	fsysroot(fsys, h);
}

Nfs3Status
fsgetattr(SunAuthUnix *au, Nfs3Handle *h, Nfs3Attr *attr)
{
	return fsysgetattr(fsys, au, h, attr);
}

Nfs3Status
fslookup(SunAuthUnix *au, Nfs3Handle *h, char *name, Nfs3Handle *nh)
{
	return fsyslookup(fsys, au, h, name, nh);
}

Nfs3Status
fsaccess(SunAuthUnix *au, Nfs3Handle *h, u32int want, u32int *got, Nfs3Attr *attr)
{
	return fsysaccess(fsys, au, h, want, got, attr);
}

Nfs3Status
fsreadlink(SunAuthUnix *au, Nfs3Handle *h, char **link)
{
	return fsysreadlink(fsys, au, h, link);
}

Nfs3Status
fsreadfile(SunAuthUnix *au, Nfs3Handle *h, u32int count, u64int offset, uchar **data, u32int *pcount, u1int *peof)
{
	return fsysreadfile(fsys, au, h, count, offset, data, pcount, peof);
}

Nfs3Status
fsreaddir(SunAuthUnix *au, Nfs3Handle *h, u32int count, u64int cookie, uchar **data, u32int *pcount, u1int *peof)
{
	return fsysreaddir(fsys, au, h, count, cookie, data, pcount, peof);
}
