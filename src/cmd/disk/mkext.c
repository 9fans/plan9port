#include <u.h>
#include <libc.h>
#include <bio.h>

#define mkdir plan9mkdir

enum{
	LEN	= 8*1024,
	NFLDS	= 6,		/* filename, modes, uid, gid, mtime, bytes */
};

int	selected(char*, int, char*[]);
void	mkdirs(char*, char*);
void	mkdir(char*, ulong, ulong, char*, char*);
void	extract(char*, ulong, ulong, char*, char*, uvlong);
void	seekpast(uvlong);
void	error(char*, ...);
void	warn(char*, ...);
void	usage(void);
#pragma varargck argpos warn 1
#pragma varargck argpos error 1

Biobuf	bin;
uchar	binbuf[2*LEN];
char	linebuf[LEN];
int	uflag;
int	hflag;
int	vflag;
int	Tflag;

void
main(int argc, char **argv)
{
	Biobuf bout;
	char *fields[NFLDS], name[2*LEN], *p, *namep;
	char *uid, *gid;
	ulong mode, mtime;
	uvlong bytes;

	quotefmtinstall();
	namep = name;
	ARGBEGIN{
	case 'd':
		p = ARGF();
		if(strlen(p) >= LEN)
			error("destination fs name too long\n");
		strcpy(name, p);
		namep = name + strlen(name);
		break;
	case 'h':
		hflag = 1;
		Binit(&bout, 1, OWRITE);
		break;
	case 'u':
		uflag = 1;
		Tflag = 1;
		break;
	case 'T':
		Tflag = 1;
		break;
	case 'v':
		vflag = 1;
		break;
	default:
		usage();
	}ARGEND

	Binits(&bin, 0, OREAD, binbuf, sizeof binbuf);
	while(p = Brdline(&bin, '\n')){
		p[Blinelen(&bin)-1] = '\0';
		strcpy(linebuf, p);
		p = linebuf;
		if(strcmp(p, "end of archive") == 0){
			Bterm(&bout);
			fprint(2, "done\n");
			exits(0);
		}
		if (gettokens(p, fields, NFLDS, " \t") != NFLDS){
			warn("too few fields in file header");
			continue;
		}
		p = unquotestrdup(fields[0]);
		strcpy(namep, p);
		free(p);
		mode = strtoul(fields[1], 0, 8);
		uid = fields[2];
		gid = fields[3];
		mtime = strtoul(fields[4], 0, 10);
		bytes = strtoull(fields[5], 0, 10);
		if(argc){
			if(!selected(namep, argc, argv)){
				if(bytes)
					seekpast(bytes);
				continue;
			}
			mkdirs(name, namep);
		}
		if(hflag){
			Bprint(&bout, "%q %luo %q %q %lud %llud\n",
				name, mode, uid, gid, mtime, bytes);
			if(bytes)
				seekpast(bytes);
			continue;
		}
		if(mode & DMDIR)
			mkdir(name, mode, mtime, uid, gid);
		else
			extract(name, mode, mtime, uid, gid, bytes);
	}
	fprint(2, "premature end of archive\n");
	exits("premature end of archive");
}

int
fileprefix(char *prefix, char *s)
{
	while(*prefix)
		if(*prefix++ != *s++)
			return 0;
	if(*s && *s != '/')
		return 0;
	return 1;
}

int
selected(char *s, int argc, char *argv[])
{
	int i;

	for(i=0; i<argc; i++)
		if(fileprefix(argv[i], s))
			return 1;
	return 0;
}

void
mkdirs(char *name, char *namep)
{
	char buf[2*LEN], *p;
	int fd;

	strcpy(buf, name);
	for(p = &buf[namep - name]; p = utfrune(p, '/'); p++){
		if(p[1] == '\0')
			return;
		*p = 0;
		fd = create(buf, OREAD, 0775|DMDIR);
		close(fd);
		*p = '/';
	}
}

void
mkdir(char *name, ulong mode, ulong mtime, char *uid, char *gid)
{
	Dir *d, xd;
	int fd;
	char *p;
	char olderr[256];

	fd = create(name, OREAD, mode);
	if(fd < 0){
		rerrstr(olderr, sizeof(olderr));
		if((d = dirstat(name)) == nil || !(d->mode & DMDIR)){
			free(d);
			warn("can't make directory %q, mode %luo: %s", name, mode, olderr);
			return;
		}
		free(d);
	}
	close(fd);

	d = &xd;
	nulldir(d);
	p = utfrrune(name, L'/');
	if(p)
		p++;
	else
		p = name;
	d->name = p;
	if(uflag){
		d->uid = uid;
		d->gid = gid;
	}
	if(Tflag)
		d->mtime = mtime;
	d->mode = mode;
	if(dirwstat(name, d) < 0)
		warn("can't set modes for %q: %r", name);

	if(uflag||Tflag){
		if((d = dirstat(name)) == nil){
			warn("can't reread modes for %q: %r", name);
			return;
		}
		if(Tflag && d->mtime != mtime)
			warn("%q: time mismatch %lud %lud\n", name, mtime, d->mtime);
		if(uflag && strcmp(uid, d->uid))
			warn("%q: uid mismatch %q %q", name, uid, d->uid);
		if(uflag && strcmp(gid, d->gid))
			warn("%q: gid mismatch %q %q", name, gid, d->gid);
	}
}

void
extract(char *name, ulong mode, ulong mtime, char *uid, char *gid, uvlong bytes)
{
	Dir d, *nd;
	Biobuf *b;
	char buf[LEN];
	char *p;
	ulong n;
	uvlong tot;

	if(vflag)
		print("x %q %llud bytes\n", name, bytes);

	b = Bopen(name, OWRITE);
	if(!b){
		warn("can't make file %q: %r", name);
		seekpast(bytes);
		return;
	}
	for(tot = 0; tot < bytes; tot += n){
		n = sizeof buf;
		if(tot + n > bytes)
			n = bytes - tot;
		n = Bread(&bin, buf, n);
		if(n <= 0)
			error("premature eof reading %q", name);
		if(Bwrite(b, buf, n) != n)
			warn("error writing %q: %r", name);
	}

	nulldir(&d);
	p = utfrrune(name, '/');
	if(p)
		p++;
	else
		p = name;
	d.name = p;
	if(uflag){
		d.uid = uid;
		d.gid = gid;
	}
	if(Tflag)
		d.mtime = mtime;
	d.mode = mode;
	Bflush(b);
	if(dirfwstat(Bfildes(b), &d) < 0)
		warn("can't set modes for %q: %r", name);
	if(uflag||Tflag){
		if((nd = dirfstat(Bfildes(b))) == nil)
			warn("can't reread modes for %q: %r", name);
		else{
			if(Tflag && nd->mtime != mtime)
				warn("%q: time mismatch %lud %lud\n",
					name, mtime, nd->mtime);
			if(uflag && strcmp(uid, nd->uid))
				warn("%q: uid mismatch %q %q",
					name, uid, nd->uid);
			if(uflag && strcmp(gid, nd->gid))
				warn("%q: gid mismatch %q %q",
					name, gid, nd->gid);
			free(nd);
		}
	}
	Bterm(b);
}

void
seekpast(uvlong bytes)
{
	char buf[LEN];
	long n;
	uvlong tot;

	for(tot = 0; tot < bytes; tot += n){
		n = sizeof buf;
		if(tot + n > bytes)
			n = bytes - tot;
		n = Bread(&bin, buf, n);
		if(n < 0)
			error("premature eof");
	}
}

void
error(char *fmt, ...)
{
	char buf[1024];
	va_list arg;

	sprint(buf, "%q: ", argv0);
	va_start(arg, fmt);
	vseprint(buf+strlen(buf), buf+sizeof(buf), fmt, arg);
	va_end(arg);
	fprint(2, "%s\n", buf);
	exits(0);
}

void
warn(char *fmt, ...)
{
	char buf[1024];
	va_list arg;

	sprint(buf, "%q: ", argv0);
	va_start(arg, fmt);
	vseprint(buf+strlen(buf), buf+sizeof(buf), fmt, arg);
	va_end(arg);
	fprint(2, "%s\n", buf);
}

void
usage(void)
{
	fprint(2, "usage: mkext [-h] [-u] [-v] [-d dest-fs] [file ...]\n");
	exits("usage");
}
