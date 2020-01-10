#include <u.h>
#include <libc.h>
#include <auth.h>
#include <bio.h>

#define mkdir plan9mkdir
#define getmode plan9_getmode
#define setuid plan9_setuid

enum{
	LEN	= 8*1024,
	HUNKS	= 128,

	/*
	 * types of destination file sytems
	 */
	Kfs = 0,
	Fs,
	Archive,
};

typedef struct File	File;

struct File{
	char	*new;
	char	*elem;
	char	*old;
	char	*uid;
	char	*gid;
	ulong	mode;
};

void	arch(Dir*);
void	copy(Dir*);
int	copyfile(File*, Dir*, int);
void*	emalloc(ulong);
void	error(char *, ...);
void	freefile(File*);
File*	getfile(File*);
char*	getmode(char*, ulong*);
char*	getname(char*, char**);
char*	getpath(char*);
void	kfscmd(char *);
void	mkdir(Dir*);
int	mkfile(File*);
void	mkfs(File*, int);
char*	mkpath(char*, char*);
void	mktree(File*, int);
void	mountkfs(char*);
void	printfile(File*);
void	setnames(File*);
void	setusers(void);
void	skipdir(void);
char*	strdup(char*);
int	uptodate(Dir*, char*);
void	usage(void);
void	warn(char *, ...);

Biobuf	*b;
Biobuf	bout;			/* stdout when writing archive */
uchar	boutbuf[2*LEN];
char	newfile[LEN];
char	oldfile[LEN];
char	*proto;
char	*cputype;
char	*users;
char	*oldroot;
char	*newroot;
char	*prog = "mkfs";
int	lineno;
char	*buf;
char	*zbuf;
int	buflen = 1024-8;
int	indent;
int	verb;
int	modes;
int	ream;
int	debug;
int	xflag;
int	sfd;
int	fskind;			/* Kfs, Fs, Archive */
int	setuid;			/* on Fs: set uid and gid? */
char	*user;

void
main(int argc, char **argv)
{
	File file;
	char *name;
	int i, errs;

	quotefmtinstall();
	user = getuser();
	name = "";
	memset(&file, 0, sizeof file);
	file.new = "";
	file.old = 0;
	oldroot = "";
	newroot = "/n/kfs";
	users = 0;
	fskind = Kfs;
	ARGBEGIN{
	case 'a':
		if(fskind != Kfs) {
			fprint(2, "cannot use -a with -d\n");
			usage();
		}
		fskind = Archive;
		newroot = "";
		Binits(&bout, 1, OWRITE, boutbuf, sizeof boutbuf);
		break;
	case 'd':
		if(fskind != Kfs) {
			fprint(2, "cannot use -d with -a\n");
			usage();
		}
		fskind = Fs;
		newroot = ARGF();
		break;
	case 'D':
		debug = 1;
		break;
	case 'n':
		name = EARGF(usage());
		break;
	case 'p':
		modes = 1;
		break;
	case 'r':
		ream = 1;
		break;
	case 's':
		oldroot = ARGF();
		break;
	case 'u':
		users = ARGF();
		break;
	case 'U':
		setuid = 1;
		break;
	case 'v':
		verb = 1;
		break;
	case 'x':
		xflag = 1;
		break;
	case 'z':
		buflen = atoi(ARGF())-8;
		break;
	default:
		usage();
	}ARGEND

	if(!argc)
		usage();

	buf = emalloc(buflen);
	zbuf = emalloc(buflen);
	memset(zbuf, 0, buflen);

	mountkfs(name);
	kfscmd("allow");
	proto = "users";
	setusers();
	cputype = getenv("cputype");
	if(cputype == 0)
		cputype = "68020";

	errs = 0;
	for(i = 0; i < argc; i++){
		proto = argv[i];
		fprint(2, "processing %q\n", proto);

		b = Bopen(proto, OREAD);
		if(!b){
			fprint(2, "%q: can't open %q: skipping\n", prog, proto);
			errs++;
			continue;
		}

		lineno = 0;
		indent = 0;
		mkfs(&file, -1);
		Bterm(b);
	}
	fprint(2, "file system made\n");
	kfscmd("disallow");
	kfscmd("sync");
	if(errs)
		exits("skipped protos");
	if(fskind == Archive){
		Bprint(&bout, "end of archive\n");
		Bterm(&bout);
	}
	exits(0);
}

void
mkfs(File *me, int level)
{
	File *child;
	int rec;

	child = getfile(me);
	if(!child)
		return;
	if((child->elem[0] == '+' || child->elem[0] == '*') && child->elem[1] == '\0'){
		rec = child->elem[0] == '+';
		free(child->new);
		child->new = strdup(me->new);
		setnames(child);
		mktree(child, rec);
		freefile(child);
		child = getfile(me);
	}
	while(child && indent > level){
		if(mkfile(child))
			mkfs(child, indent);
		freefile(child);
		child = getfile(me);
	}
	if(child){
		freefile(child);
		Bseek(b, -Blinelen(b), 1);
		lineno--;
	}
}

void
mktree(File *me, int rec)
{
	File child;
	Dir *d;
	int i, n, fd;

	fd = open(oldfile, OREAD);
	if(fd < 0){
		warn("can't open %q: %r", oldfile);
		return;
	}

	child = *me;
	while((n = dirread(fd, &d)) > 0){
		for(i = 0; i < n; i++){
			child.new = mkpath(me->new, d[i].name);
			if(me->old)
				child.old = mkpath(me->old, d[i].name);
			child.elem = d[i].name;
			setnames(&child);
			if(copyfile(&child, &d[i], 1) && rec)
				mktree(&child, rec);
			free(child.new);
			if(child.old)
				free(child.old);
		}
	}
	close(fd);
}

int
mkfile(File *f)
{
	Dir *dir;

	if((dir = dirstat(oldfile)) == nil){
		warn("can't stat file %q: %r", oldfile);
		skipdir();
		return 0;
	}
	return copyfile(f, dir, 0);
}

int
copyfile(File *f, Dir *d, int permonly)
{
	ulong mode;
	Dir nd;

	if(xflag){
		Bprint(&bout, "%q\t%ld\t%lld\n", f->new, d->mtime, d->length);
		return (d->mode & DMDIR) != 0;
	}
	if(verb && (fskind == Archive || ream))
		fprint(2, "%q\n", f->new);
	d->name = f->elem;
	if(d->type != 'M' && d->type != 'Z'){
		d->uid = "sys";
		d->gid = "sys";
		mode = (d->mode >> 6) & 7;
		d->mode |= mode | (mode << 3);
	}
	if(strcmp(f->uid, "-") != 0)
		d->uid = f->uid;
	if(strcmp(f->gid, "-") != 0)
		d->gid = f->gid;
	if(fskind == Fs && !setuid){
		d->uid = "";
		d->gid = "";
	}
	if(f->mode != ~0){
		if(permonly)
			d->mode = (d->mode & ~0666) | (f->mode & 0666);
		else if((d->mode&DMDIR) != (f->mode&DMDIR))
			warn("inconsistent mode for %q", f->new);
		else
			d->mode = f->mode;
	}
	if(!uptodate(d, newfile)){
		if(verb && (fskind != Archive && ream == 0))
			fprint(2, "%q\n", f->new);
		if(d->mode & DMDIR)
			mkdir(d);
		else
			copy(d);
	}else if(modes){
		nulldir(&nd);
		nd.mode = d->mode;
		nd.gid = d->gid;
		nd.mtime = d->mtime;
		if(verb && (fskind != Archive && ream == 0))
			fprint(2, "%q\n", f->new);
		if(dirwstat(newfile, &nd) < 0)
			warn("can't set modes for %q: %r", f->new);
		nulldir(&nd);
		nd.uid = d->uid;
		dirwstat(newfile, &nd);
	}
	return (d->mode & DMDIR) != 0;
}

/*
 * check if file to is up to date with
 * respect to the file represented by df
 */
int
uptodate(Dir *df, char *to)
{
	int ret;
	Dir *dt;

	if(fskind == Archive || ream || (dt = dirstat(to)) == nil)
		return 0;
	ret = dt->mtime >= df->mtime;
	free(dt);
	return ret;
}

void
copy(Dir *d)
{
	char cptmp[LEN], *p;
	int f, t, n, needwrite, nowarnyet = 1;
	vlong tot, len;
	Dir nd;

	f = open(oldfile, OREAD);
	if(f < 0){
		warn("can't open %q: %r", oldfile);
		return;
	}
	t = -1;
	if(fskind == Archive)
		arch(d);
	else{
		strcpy(cptmp, newfile);
		p = utfrrune(cptmp, L'/');
		if(!p)
			error("internal temporary file error");
		strcpy(p+1, "__mkfstmp");
		t = create(cptmp, OWRITE, 0666);
		if(t < 0){
			warn("can't create %q: %r", newfile);
			close(f);
			return;
		}
	}

	needwrite = 0;
	for(tot = 0; tot < d->length; tot += n){
		len = d->length - tot;
		/* don't read beyond d->length */
		if (len > buflen)
			len = buflen;
		n = read(f, buf, len);
		if(n <= 0) {
			if(n < 0 && nowarnyet) {
				warn("can't read %q: %r", oldfile);
				nowarnyet = 0;
			}
			/*
			 * don't quit: pad to d->length (in pieces) to agree
			 * with the length in the header, already emitted.
			 */
			memset(buf, 0, len);
			n = len;
		}
		if(fskind == Archive){
			if(Bwrite(&bout, buf, n) != n)
				error("write error: %r");
		}else if(memcmp(buf, zbuf, n) == 0){
			if(seek(t, n, 1) < 0)
				error("can't write zeros to %q: %r", newfile);
			needwrite = 1;
		}else{
			if(write(t, buf, n) < n)
				error("can't write %q: %r", newfile);
			needwrite = 0;
		}
	}
	close(f);
	if(needwrite){
		if(seek(t, -1, 1) < 0 || write(t, zbuf, 1) != 1)
			error("can't write zero at end of %q: %r", newfile);
	}
	if(tot != d->length){
		/* this should no longer happen */
		warn("wrong number of bytes written to %q (was %lld should be %lld)\n",
			newfile, tot, d->length);
		if(fskind == Archive){
			warn("seeking to proper position\n");
			/* does no good if stdout is a pipe */
			Bseek(&bout, d->length - tot, 1);
		}
	}
	if(fskind == Archive)
		return;
	remove(newfile);
	nulldir(&nd);
	nd.mode = d->mode;
	nd.gid = d->gid;
	nd.mtime = d->mtime;
	nd.name = d->name;
	if(dirfwstat(t, &nd) < 0)
		error("can't move tmp file to %q: %r", newfile);
	nulldir(&nd);
	nd.uid = d->uid;
	dirfwstat(t, &nd);
	close(t);
}

void
mkdir(Dir *d)
{
	Dir *d1;
	Dir nd;
	int fd;

	if(fskind == Archive){
		arch(d);
		return;
	}
	fd = create(newfile, OREAD, d->mode);
	nulldir(&nd);
	nd.mode = d->mode;
	nd.gid = d->gid;
	nd.mtime = d->mtime;
	if(fd < 0){
		if((d1 = dirstat(newfile)) == nil || !(d1->mode & DMDIR)){
			free(d1);
			error("can't create %q", newfile);
		}
		free(d1);
		if(dirwstat(newfile, &nd) < 0)
			warn("can't set modes for %q: %r", newfile);
		nulldir(&nd);
		nd.uid = d->uid;
		dirwstat(newfile, &nd);
		return;
	}
	if(dirfwstat(fd, &nd) < 0)
		warn("can't set modes for %q: %r", newfile);
	nulldir(&nd);
	nd.uid = d->uid;
	dirfwstat(fd, &nd);
	close(fd);
}

void
arch(Dir *d)
{
	Bprint(&bout, "%q %luo %q %q %lud %lld\n",
		newfile, d->mode, d->uid, d->gid, d->mtime, d->length);
}

char *
mkpath(char *prefix, char *elem)
{
	char *p;
	int n;

	n = strlen(prefix) + strlen(elem) + 2;
	p = emalloc(n);
	sprint(p, "%s/%s", prefix, elem);
	return p;
}

char *
strdup(char *s)
{
	char *t;

	t = emalloc(strlen(s) + 1);
	return strcpy(t, s);
}

void
setnames(File *f)
{
	sprint(newfile, "%s%s", newroot, f->new);
	if(f->old){
		if(f->old[0] == '/')
			sprint(oldfile, "%s%s", oldroot, f->old);
		else
			strcpy(oldfile, f->old);
	}else
		sprint(oldfile, "%s%s", oldroot, f->new);
	if(strlen(newfile) >= sizeof newfile
	|| strlen(oldfile) >= sizeof oldfile)
		error("name overfile");
}

void
freefile(File *f)
{
	if(f->old)
		free(f->old);
	if(f->new)
		free(f->new);
	free(f);
}

/*
 * skip all files in the proto that
 * could be in the current dir
 */
void
skipdir(void)
{
	char *p, c;
	int level;

	if(indent < 0 || b == nil)	/* b is nil when copying adm/users */
		return;
	level = indent;
	for(;;){
		indent = 0;
		p = Brdline(b, '\n');
		lineno++;
		if(!p){
			indent = -1;
			return;
		}
		while((c = *p++) != '\n')
			if(c == ' ')
				indent++;
			else if(c == '\t')
				indent += 8;
			else
				break;
		if(indent <= level){
			Bseek(b, -Blinelen(b), 1);
			lineno--;
			return;
		}
	}
}

File*
getfile(File *old)
{
	File *f;
	char *elem;
	char *p;
	int c;

	if(indent < 0)
		return 0;
loop:
	indent = 0;
	p = Brdline(b, '\n');
	lineno++;
	if(!p){
		indent = -1;
		return 0;
	}
	while((c = *p++) != '\n')
		if(c == ' ')
			indent++;
		else if(c == '\t')
			indent += 8;
		else
			break;
	if(c == '\n' || c == '#')
		goto loop;
	p--;
	f = emalloc(sizeof *f);
	p = getname(p, &elem);
	if(debug)
		fprint(2, "getfile: %q root %q\n", elem, old->new);
	f->new = mkpath(old->new, elem);
	f->elem = utfrrune(f->new, L'/') + 1;
	p = getmode(p, &f->mode);
	p = getname(p, &f->uid);
	if(!*f->uid)
		f->uid = "-";
	p = getname(p, &f->gid);
	if(!*f->gid)
		f->gid = "-";
	f->old = getpath(p);
	if(f->old && strcmp(f->old, "-") == 0){
		free(f->old);
		f->old = 0;
	}
	setnames(f);

	if(debug)
		printfile(f);

	return f;
}

char*
getpath(char *p)
{
	char *q, *new;
	int c, n;

	while((c = *p) == ' ' || c == '\t')
		p++;
	q = p;
	while((c = *q) != '\n' && c != ' ' && c != '\t')
		q++;
	if(q == p)
		return 0;
	n = q - p;
	new = emalloc(n + 1);
	memcpy(new, p, n);
	new[n] = 0;
	return new;
}

char*
getname(char *p, char **buf)
{
	char *s, *start;
	int c;

	while((c = *p) == ' ' || c == '\t')
		p++;

	start = p;
	while((c = *p) != '\n' && c != ' ' && c != '\t' && c != '\0')
		p++;

	*buf = malloc(p+1-start);
	if(*buf == nil)
		return nil;
	memmove(*buf, start, p-start);
	(*buf)[p-start] = '\0';

	if(**buf == '$'){
		s = getenv(*buf+1);
		if(s == 0){
			warn("can't read environment variable %q", *buf+1);
			skipdir();
			free(*buf);
			return nil;
		}
		free(*buf);
		*buf = s;
	}
	return p;
}

char*
getmode(char *p, ulong *xmode)
{
	char *buf, *s;
	ulong m;

	*xmode = ~0;
	p = getname(p, &buf);
	if(p == nil)
		return nil;

	s = buf;
	if(!*s || strcmp(s, "-") == 0)
		return p;
	m = 0;
	if(*s == 'd'){
		m |= DMDIR;
		s++;
	}
	if(*s == 'a'){
		m |= DMAPPEND;
		s++;
	}
	if(*s == 'l'){
		m |= DMEXCL;
		s++;
	}
	if(s[0] < '0' || s[0] > '7'
	|| s[1] < '0' || s[1] > '7'
	|| s[2] < '0' || s[2] > '7'
	|| s[3]){
		warn("bad mode specification %q", buf);
		free(buf);
		return p;
	}
	*xmode = m | strtoul(s, 0, 8);
	free(buf);
	return p;
}

void
setusers(void)
{
	File file;
	int m;

	if(fskind != Kfs)
		return;
	m = modes;
	modes = 1;
	file.uid = "adm";
	file.gid = "adm";
	file.mode = DMDIR|0775;
	file.new = "/adm";
	file.elem = "adm";
	file.old = 0;
	setnames(&file);
	strcpy(oldfile, file.new);	/* Don't use root for /adm */
	mkfile(&file);
	file.new = "/adm/users";
	file.old = users;
	file.elem = "users";
	file.mode = 0664;
	setnames(&file);
	if (file.old)
		strcpy(oldfile, file.old);	/* Don't use root for /adm/users */
	mkfile(&file);
	kfscmd("user");
	mkfile(&file);
	file.mode = DMDIR|0775;
	file.new = "/adm";
	file.old = "/adm";
	file.elem = "adm";
	setnames(&file);
	strcpy(oldfile, file.old);	/* Don't use root for /adm */
	mkfile(&file);
	modes = m;
}

void
mountkfs(char *name)
{
	if(fskind != Kfs)
		return;
	sysfatal("no kfs: use -a or -d");
}

void
kfscmd(char *cmd)
{
	char buf[4*1024];
	int n;

	if(fskind != Kfs)
		return;
	if(write(sfd, cmd, strlen(cmd)) != strlen(cmd)){
		fprint(2, "%q: error writing %q: %r", prog, cmd);
		return;
	}
	for(;;){
		n = read(sfd, buf, sizeof buf - 1);
		if(n <= 0)
			return;
		buf[n] = '\0';
		if(strcmp(buf, "done") == 0 || strcmp(buf, "success") == 0)
			return;
		if(strcmp(buf, "unknown command") == 0){
			fprint(2, "%q: command %q not recognized\n", prog, cmd);
			return;
		}
	}
}

void *
emalloc(ulong n)
{
	void *p;

	if((p = malloc(n)) == 0)
		error("out of memory");
	return p;
}

void
error(char *fmt, ...)
{
	char buf[1024];
	va_list arg;

	sprint(buf, "%q: %q:%d: ", prog, proto, lineno);
	va_start(arg, fmt);
	vseprint(buf+strlen(buf), buf+sizeof(buf), fmt, arg);
	va_end(arg);
	fprint(2, "%s\n", buf);
	kfscmd("disallow");
	kfscmd("sync");
	exits(0);
}

void
warn(char *fmt, ...)
{
	char buf[1024];
	va_list arg;

	sprint(buf, "%q: %q:%d: ", prog, proto, lineno);
	va_start(arg, fmt);
	vseprint(buf+strlen(buf), buf+sizeof(buf), fmt, arg);
	va_end(arg);
	fprint(2, "%s\n", buf);
}

void
printfile(File *f)
{
	if(f->old)
		fprint(2, "%q from %q %q %q %lo\n", f->new, f->old, f->uid, f->gid, f->mode);
	else
		fprint(2, "%q %q %q %lo\n", f->new, f->uid, f->gid, f->mode);
}

void
usage(void)
{
	fprint(2, "usage: %q [-aprvx] [-d root] [-n name] [-s source] [-u users] [-z n] proto ...\n", prog);
	exits("usage");
}
