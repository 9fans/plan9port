#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <fs.h>

char *addr;

void
usage(void)
{
	fprint(2, "usage: 9p [-a address] cmd args...\n");
	fprint(2, "possible cmds:\n");
	fprint(2, "	read name\n");
	fprint(2, "	write name\n");
	fprint(2, "	stat name\n");
//	fprint(2, "	ls name\n");
	fprint(2, "without -a, name elem/path means /path on server unix!$ns/elem\n");
	exits("usage");
}

void xread(int, char**);
void xwrite(int, char**);
void xstat(int, char**);
void xls(int, char**);

struct {
	char *s;
	void (*f)(int, char**);
} cmds[] = {
	"read", xread,
	"write", xwrite,
	"stat", xstat,
//	"ls", xls,
};

void
main(int argc, char **argv)
{
	char *cmd;
	int i;

	ARGBEGIN{
	case 'a':
		addr = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND

	if(argc < 1)
		usage();

	cmd = argv[0];
	for(i=0; i<nelem(cmds); i++){
		if(strcmp(cmds[i].s, cmd) == 0){
			cmds[i].f(argc, argv);
			exits(0);
		}
	}
	usage();	
}

Fsys*
xparse(char *name, char **path)
{
	int fd;
	char *ns;
	char *p;
	Fsys *fs;

	if(addr == nil){
		p = strchr(name, '/');
		if(p == nil)
			p = name+strlen(name);
		else
			*p++ = 0;
		*path = p;
		if(*name == 0)
			usage();
		ns = getenv("ns");
		if(ns == nil)
			sysfatal("ns not set");
		addr = smprint("unix!%s/%s", ns, name);
		if(addr == nil)
			sysfatal("out of memory");
	}else
		*path = name;

	fprint(2, "dial %s...", addr);
	if((fd = dial(addr, nil, nil, nil)) < 0)
		sysfatal("dial: %r");
	if((fs = fsmount(fd)) == nil)
		sysfatal("fsmount: %r");
	return fs;
}

Fid*
xwalk(char *name)
{
	Fid *fid;
	Fsys *fs;

	fs = xparse(name, &name);
	fid = fswalk(fsroot(fs), name);
	if(fid == nil)
		sysfatal("fswalk %s: %r", name);
	return fid;
}

Fid*
xopen(char *name, int mode)
{
	Fid *fid;
	Fsys *fs;

	fs = xparse(name, &name);
	fid = fsopen(fs, name, mode);
	if(fid == nil)
		sysfatal("fsopen %s: %r", name);
	return fid;
}

void
xread(int argc, char **argv)
{
	char buf[1024];
	int n;
	Fid *fid;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	fid = xopen(argv[0], OREAD);
	while((n = fsread(fid, buf, sizeof buf)) > 0)
		write(1, buf, n);
	if(n < 0)
		sysfatal("read error: %r");
	exits(0);	
}

void
xwrite(int argc, char **argv)
{
	char buf[1024];
	int n;
	Fid *fid;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	fid = xopen(argv[0], OWRITE|OTRUNC);
	while((n = read(0, buf, sizeof buf)) > 0)
		if(fswrite(fid, buf, n) != n)
			sysfatal("write error: %r");
	if(n < 0)
		sysfatal("read error: %r");
	exits(0);	
}

void
xstat(int argc, char **argv)
{
	Dir *d;
	Fid *fid;

	ARGBEGIN{
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	fid = xwalk(argv[0]);
	if((d = fsdirfstat(fid)) < 0)
		sysfatal("dirfstat: %r");
	fmtinstall('D', dirfmt);
	fmtinstall('M', dirmodefmt);
	print("%D\n", d);
	exits(0);
}
