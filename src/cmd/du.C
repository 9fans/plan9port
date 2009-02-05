#include <u.h>
#include <libc.h>

extern	vlong	du(char*, Dir*);
extern	vlong	k(vlong);
extern	void	err(char*);
extern	int	warn(char*);
extern	int	seen(Dir*);

int	aflag;
int	fflag;
int	nflag;
int	sflag;
int	tflag;
int	uflag;
int	qflag;
char	*fmt = "%llud\t%s\n";
vlong	blocksize = 1024LL;

void
main(int argc, char *argv[])
{
	int i;
	char *s, *ss;

	ARGBEGIN {
	case 'a':	/* all files */
		aflag = 1;
		break;
	case 's':	/* only top level */
		sflag = 1;
		break;
	case 'f':	/* ignore errors */
		fflag = 1;
		break;
	case 'n':	/* all files, number of bytes */
		aflag = 1;
		nflag = 1;
		break;
	case 't':	/* return modified/accessed time */
		tflag = 1;
		break;
	case 'u':	/* accessed time */
		uflag = 1;
		break;
	case 'q':	/* qid */
		fmt = "%.16llux\t%s\n";
		qflag = 1;
		break;
	case 'b':	/* block size */
		s = ARGF();
		if(s) {
			blocksize = strtoul(s, &ss, 0);
			if(s == ss)
				blocksize = 1;
			if(*ss == 'k')
				blocksize *= 1024;
		}
		break;
	} ARGEND
	if(argc==0)
		print(fmt, du(".", dirstat(".")), ".");
	else
		for(i=0; i<argc; i++)
			print(fmt, du(argv[i], dirstat(argv[i])), argv[i]);
	exits(0);
}

vlong
du(char *name, Dir *dir)
{
	int fd, i, n;
	Dir *buf, *d;
	char file[256];
	vlong nk, t;

	if(dir == nil)
		return warn(name);

	fd = open(name, OREAD);
	if(fd < 0)
		return warn(name);

	if((dir->qid.type&QTDIR) == 0)
		nk = k(dir->length);
	else{
		nk = 0;
		while((n=dirread(fd, &buf)) > 0) {
			d = buf;
			for(i=0; i<n; i++, d++) {
				if((d->qid.type&QTDIR) == 0) {
					t = k(d->length);
					nk += t;
					if(aflag) {
						sprint(file, "%s/%s", name, d->name);
						if(tflag) {
							t = d->mtime;
							if(uflag)
								t = d->atime;
						}
						if(qflag)
							t = d->qid.path;
						print(fmt, t, file);
					}
					continue;
				}
				if(strcmp(d->name, ".") == 0 ||
				   strcmp(d->name, "..") == 0 ||
				   seen(d))
					continue;
				sprint(file, "%s/%s", name, d->name);
				t = du(file, d);
				nk += t;
				if(tflag) {
					t = d->mtime;
					if(uflag)
						t = d->atime;
				}
				if(qflag)
					t = d->qid.path;
				if(!sflag)
					print(fmt, t, file);
			}
			free(buf);
		}
		if(n < 0)
			warn(name);
	}
	close(fd);
	if(tflag) {
		if(uflag)
			return dir->atime;
		return dir->mtime;
	}
	if(qflag)
		return dir->qid.path;
	return nk;
}

#define	NCACHE	128	/* must be power of two */
typedef	struct	Cache	Cache;
struct	Cache
{
	Dir*	cache;
	int	n;
	int	max;
} cache[NCACHE];

int
seen(Dir *dir)
{
	Dir *dp;
	int i;
	Cache *c;

	c = &cache[dir->qid.path&(NCACHE-1)];
	dp = c->cache;
	for(i=0; i<c->n; i++, dp++)
		if(dir->qid.path == dp->qid.path &&
		   dir->type == dp->type &&
		   dir->dev == dp->dev)
			return 1;
	if(c->n == c->max){
		c->cache = realloc(c->cache, (c->max+=20)*sizeof(Dir));
		if(c->cache == 0)
			err("malloc failure");
	}
	c->cache[c->n++] = *dir;
	return 0;
}

void
err(char *s)
{
	fprint(2, "du: %s: %r\n", s);
	exits(s);
}

int
warn(char *s)
{
	if(fflag == 0)
		fprint(2, "du: %s: %r\n", s);
	return 0;
}

vlong
k(vlong n)
{
	if(nflag)
		return n;
	n = (n+blocksize-1)/blocksize;
	return n*blocksize/1024LL;
}
