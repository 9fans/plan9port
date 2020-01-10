#include <u.h>
#include <libc.h>
#include <bio.h>
#include <thread.h>
#include <venti.h>
#include <sunrpc.h>
#include <nfs3.h>
#include <diskfs.h>

uchar *buf;
uint bufsize;
Nfs3Handle cwd, root;
Biobuf bin, bout;
char pwd[1000];
Fsys *fsys;
SunAuthUnix *auth;
VtConn *z;
VtCache *c;
Disk *disk;

char *cmdhelp(int, char**);
char *cmdcd(int, char**);
char *cmdpwd(int, char**);
char *cmdls(int, char**);
char *cmdget(int, char**);
char *cmdblock(int, char**);
char *cmddisk(int, char**);

typedef struct Cmd Cmd;
struct Cmd
{
	char *s;
	char *(*fn)(int, char**);
	char *help;
};

Cmd cmdtab[] =
{
	"cd", cmdcd, "cd dir - change directory",
	"ls", cmdls, "ls [-d] path... - list file",
	"get", cmdget, "get path [lpath] - copy file to local directory",
	"pwd", cmdpwd, "pwd - print working directory",
	"help", cmdhelp, "help - print usage summaries",
	"block", cmdblock, "block path offset - print disk offset of path's byte offset",
	"disk", cmddisk, "disk offset count - dump disk contents"
};

char*
ebuf(void)
{
	static char buf[ERRMAX];

	rerrstr(buf, sizeof buf);
	return buf;
}

static char*
estrdup(char *s)
{
	char *t;

	t = emalloc(strlen(s)+1);
	strcpy(t, s);
	return t;
}

char*
walk(char *path, Nfs3Handle *ph)
{
	char *p, *q;
	Nfs3Handle h;
	Nfs3Status ok;

	path = estrdup(path); /* writable */
	if(path[0] == '/')
		h = root;
	else
		h = cwd;
	for(p=path; *p; p=q){
		q = strchr(p, '/');
		if(q == nil)
			q = p+strlen(p);
		else
			*q++ = 0;
		if(*p == 0)
			continue;
		if((ok = fsyslookup(fsys, auth, &h, p, &h)) != Nfs3Ok){
			nfs3errstr(ok);
			free(path);
			return ebuf();
		}
	}
	*ph = h;
	free(path);
	return nil;
}

char*
cmdhelp(int argc, char **argv)
{
	int i;

	for(i=0; i<nelem(cmdtab); i++)
		print("%s\n", cmdtab[i].help);
	return nil;
}

char*
cmdcd(int argc, char **argv)
{
	char *err;
	Nfs3Attr attr;
	Nfs3Status ok;
	Nfs3Handle h;

	if(argc != 2)
		return "usage: cd dir";

	if((err = walk(argv[1], &h)) != nil)
		return err;
	if((ok = fsysgetattr(fsys, auth, &h, &attr)) != Nfs3Ok){
		nfs3errstr(ok);
		fprint(2, "%s: %r\n", argv[1]);
		return nil;
	}
	if(attr.type != Nfs3FileDir)
		return "not a directory";
	if(argv[1][0] == '/')
		pwd[0] = 0;
	strcat(pwd, "/");
	strcat(pwd, argv[1]);
	cleanname(pwd);
	cwd = h;
	print("%s\n", pwd);
	return nil;
}

char*
cmdpwd(int argc, char **argv)
{
	if(argc != 1)
		return "usage: pwd";

	print("%s\n", pwd);
	return nil;
}

/*
 * XXX maybe make a list of these in memory and then print them nicer
 */
void
ls(char *dir, char *elem, Nfs3Attr *attr)
{
	char c;

	c = ' ';	/* use attr->type */
	Bprint(&bout, "%s%s%s", dir ? dir : "", dir && elem ? "/" : "", elem ? elem : "");
	Bprint(&bout, " %c%luo %1d %4d %4d", c, attr->mode, attr->nlink, attr->uid, attr->gid);
	Bprint(&bout, " %11,lld %11,lld %4d.%4d %#11,llux %#11,llux",
		attr->size, attr->used, attr->major, attr->minor, attr->fsid, attr->fileid);
	Bprint(&bout, "\n");
}

void
lsdir(char *dir, Nfs3Handle *h)
{
	uchar *data, *p, *ep;
	Nfs3Attr attr;
	Nfs3Entry e;
	Nfs3Handle eh;
	u32int count;
	u1int eof;
	Nfs3Status ok;
	u64int cookie;

	cookie = 0;
	for(;;){
		ok = fsysreaddir(fsys, auth, h, 8192, cookie, &data, &count, &eof);
		if(ok != Nfs3Ok){
			nfs3errstr(ok);
			fprint(2, "ls %s: %r\n", dir);
			return;
		}
fprint(2, "got %d\n", count);
		p = data;
		ep = data+count;
		while(p<ep){
			if(nfs3entryunpack(p, ep, &p, &e) < 0){
				fprint(2, "%s: unpacking directory: %r\n", dir);
				break;
			}
			cookie = e.cookie;
			if((ok = fsyslookup(fsys, auth, h, e.name, &eh)) != Nfs3Ok){
				nfs3errstr(ok);
				fprint(2, "%s/%s: %r\n", dir, e.name);
				continue;
			}
			if((ok = fsysgetattr(fsys, auth, &eh, &attr)) != Nfs3Ok){
				nfs3errstr(ok);
				fprint(2, "%s/%s: %r\n", dir, e.name);
				continue;
			}
			ls(dir, e.name, &attr);
		}
		free(data);
		if(eof)
			break;
	}
}

char*
cmdls(int argc, char **argv)
{
	int i;
	int dflag;
	char *e;
	Nfs3Handle h;
	Nfs3Attr attr;
	Nfs3Status ok;

	dflag = 0;
	ARGBEGIN{
	case 'd':
		dflag = 1;
		break;
	default:
		return "usage: ls [-d] [path...]";
	}ARGEND

	if(argc == 0){
		lsdir(nil, &cwd);
		Bflush(&bout);
		return nil;
	}

	for(i=0; i<argc; i++){
		if((e = walk(argv[i], &h)) != nil){
			fprint(2, "%s: %s\n", argv[i], e);
			continue;
		}
		if((ok = fsysgetattr(fsys, auth, &h, &attr)) != Nfs3Ok){
			nfs3errstr(ok);
			fprint(2, "%s: %r\n", argv[i]);
			continue;
		}
		if(attr.type != Nfs3FileDir || dflag)
			ls(argv[i], nil, &attr);
		else
			lsdir(argv[i], &h);
		Bflush(&bout);
	}
	return nil;
}

char*
cmdget(int argc, char **argv)
{
	uchar eof;
	u32int n;
	int dflag, fd;
	char *e, *local;
	uchar *buf;
	Nfs3Handle h;
	Nfs3Attr attr;
	Nfs3Status ok;
	vlong o;

	dflag = 0;
	ARGBEGIN{
	default:
	usage:
		return "usage: get path [lpath]]";
	}ARGEND

	if(argc != 1 && argc != 2)
		goto usage;

	if((e = walk(argv[0], &h)) != nil){
		fprint(2, "%s: %s\n", argv[0], e);
		return nil;
	}
	if((ok = fsysgetattr(fsys, auth, &h, &attr)) != Nfs3Ok){
		nfs3errstr(ok);
		fprint(2, "%s: %r\n", argv[0]);
		return nil;
	}
	local = argv[0];
	if(argc == 2)
		local = argv[1];
	if((fd = create(local, OWRITE, 0666)) < 0){
		fprint(2, "create %s: %r\n", local);
		return nil;
	}
	eof = 0;
	for(o=0; o<attr.size && !eof; o+=n){
		if((ok = fsysreadfile(fsys, nil, &h, fsys->blocksize, o, &buf, &n, &eof)) != Nfs3Ok){
			nfs3errstr(ok);
			fprint(2, "reading %s: %r\n", argv[0]);
			close(fd);
			return nil;
		}
		if(write(fd, buf, n) != n){
			fprint(2, "writing %s: %r\n", local);
			close(fd);
			free(buf);
			return nil;
		}
		free(buf);
	}
	close(fd);
	fprint(2, "copied %,lld bytes\n", o);
	return nil;
}


char*
cmdblock(int argc, char **argv)
{
	char *e;
	Nfs3Handle h;
	u64int bno;

	ARGBEGIN{
	default:
		return "usage: block path offset";
	}ARGEND

	if(argc != 2)
		return "usage: block path offset";

	if((e = walk(argv[0], &h)) != nil){
		fprint(2, "%s: %s\n", argv[0], e);
		return nil;
	}
	if((bno = fsys->fileblock(fsys, &h, strtoll(argv[1], 0, 0))) == 0){
		fprint(2, "%s: %r\n", argv[0]);
		return nil;
	}
	print("%#llux\n", bno);
	return nil;
}

char*
cmddisk(int argc, char **argv)
{
	Block *b;
	int delta, count, i;
	u64int offset;
	uchar *p;

	ARGBEGIN{
	default:
		return "usage: disk offset count";
	}ARGEND

	if(argc != 2)
		return "usage: disk offset count";

	offset = strtoull(argv[0], 0, 0);
	count = atoi(argv[1]);
	delta = offset%fsys->blocksize;

	b = diskread(disk, fsys->blocksize, offset-delta);
	if(b == nil){
		fprint(2, "diskread: %r\n");
		return nil;
	}
	p = b->data + delta;
	for(i=0; i<count; i++){
		Bprint(&bout, "%2.2ux ", p[i]);
		if(i%16 == 15)
			Bprint(&bout, "\n");
		else if(i%8 == 7)
			Bprint(&bout, " - ");
	}
	if(i%16 != 0)
		Bprint(&bout, "\n");
	Bflush(&bout);
	blockput(b);
	return nil;
}

void
usage(void)
{
	fprint(2, "usage: vftp score\n");
	threadexitsall("usage");
}

extern int allowall;

void
threadmain(int argc, char **argv)
{
	char *err, *f[10], *p;
	int i, nf;
	uchar score[VtScoreSize];
	Nfs3Status ok;

	allowall = 1;
	ARGBEGIN{
	case 'V':
		chattyventi++;
		break;
	default:
		usage();
	}ARGEND

	if(argc != 1)
		usage();

	fmtinstall('F', vtfcallfmt);
	fmtinstall('H', encodefmt);
	fmtinstall('V', vtscorefmt);

	if(access(argv[0], AEXIST) >= 0 || strchr(argv[0], '/')){
		if((disk = diskopenfile(argv[0])) == nil)
			sysfatal("diskopen: %r");
		if((disk = diskcache(disk, 32768, 16)) == nil)
			sysfatal("diskcache: %r");
	}else{
		if(vtparsescore(argv[0], nil, score) < 0)
			sysfatal("bad score '%s'", argv[0]);
		if((z = vtdial(nil)) == nil)
			sysfatal("vtdial: %r");
		if(vtconnect(z) < 0)
			sysfatal("vtconnect: %r");
		if((c = vtcachealloc(z, 32768, 32)) == nil)
			sysfatal("vtcache: %r");
		if((disk = diskopenventi(c, score)) == nil)
			sysfatal("diskopenventi: %r");
	}
	if((fsys = fsysopen(disk)) == nil)
		sysfatal("fsysopen: %r");

	fprint(2, "block size %d\n", fsys->blocksize);
	buf = emalloc(fsys->blocksize);
	if((ok = fsysroot(fsys, &root)) != Nfs3Ok){
		nfs3errstr(ok);
		sysfatal("accessing root: %r");
	}
	cwd = root;
	Binit(&bin, 0, OREAD);
	Binit(&bout, 1, OWRITE);

	while(fprint(2, "vftp> "), (p = Brdstr(&bin, '\n', 1)) != nil){
		if(p[0] == '#')
			continue;
		nf = tokenize(p, f, nelem(f));
		if(nf == 0)
			continue;
		for(i=0; i<nelem(cmdtab); i++){
			if(strcmp(f[0], cmdtab[i].s) == 0){
				if((err = cmdtab[i].fn(nf, f)) != nil)
					fprint(2, "%s\n", err);
				break;
			}
		}
		if(i == nelem(cmdtab))
			fprint(2, "unknown command '%s'\n", f[0]);
	}
	threadexitsall(nil);
}
