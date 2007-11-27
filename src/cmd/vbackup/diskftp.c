#include <u.h>
#include <libc.h>
#include <thread.h>
#include <sunrpc.h>
#include <nfs3.h>
#include <diskfs.h>

int debug;

void
usage(void)
{
	fprint(2, "usage: fsview fspartition cmd\n");
	fprint(2, "cmd is:\n");
	fprint(2, "\tcat file\n");
	fprint(2, "\tls dir\n");
	fprint(2, "\tstat file\n");
	threadexitsall("usage");
}

void
printattr(Nfs3Attr *attr)
{
	Fmt fmt;
	char buf[256];

	fmtfdinit(&fmt, 2, buf, sizeof buf);
	nfs3attrprint(&fmt, attr);
	fmtfdflush(&fmt);
	fprint(2, "\n");
}

char buf[8192];

void
x(int ok)
{
	if(ok != Nfs3Ok){
		nfs3errstr(ok);
		sysfatal("%r");
	}
}

void
threadmain(int argc, char **argv)
{
	char *p, *q;
	u32int n;
	Disk *disk;
	Fsys *fsys;
	Nfs3Handle h;
	SunAuthUnix au;
	Nfs3Attr attr;
	u64int offset;
	u1int eof;
	uchar *data;
	char *link;

	ARGBEGIN{
	case 'd':
		debug = 1;
		break;
	default:
		usage();
	}ARGEND

	if(argc != 3)
		usage();

	if((disk = diskopenfile(argv[0])) == nil)
		sysfatal("diskopen: %r");
	if((disk = diskcache(disk, 16384, 16)) == nil)
		sysfatal("diskcache: %r");
	if((fsys = fsysopen(disk)) == nil)
		sysfatal("fsysopen: %r");

	allowall = 1;
	memset(&au, 0, sizeof au);

	/* walk */
	if(debug) fprint(2, "get root...");
	x(fsysroot(fsys, &h));
	p = argv[2];
	while(*p){
		while(*p == '/')
			p++;
		if(*p == 0)
			break;
		q = strchr(p, '/');
		if(q){
			*q = 0;
			q++;
		}else
			q = "";
		if(debug) fprint(2, "walk %s...", p);
		x(fsyslookup(fsys, &au, &h, p, &h));
		p = q;
	}

	if(debug) fprint(2, "getattr...");
	x(fsysgetattr(fsys, &au, &h, &attr));
	printattr(&attr);

	/* do the op */
	if(strcmp(argv[1], "cat") == 0){
		switch(attr.type){
		case Nfs3FileReg:
		case Nfs3FileDir:
			offset = 0;
			for(;;){
				x(fsysreadfile(fsys, &au, &h, sizeof buf, offset, &data, &n, &eof));
				if(n){
					write(1, data, n);
					free(data);
					offset += n;
				}
				if(eof)
					break;
			}
			break;
		case Nfs3FileSymlink:
			x(fsysreadlink(fsys, &au, &h, &link));
			print("%s\n", link);
			break;
		default:
			print("cannot cat: not file, not link\n");
			break;
		}
	}else if(strcmp(argv[1], "ls") == 0){
		/* not implemented */
	}else if(strcmp(argv[1], "stat") == 0){
		/* already done */
	}
	threadexitsall(nil);
}
