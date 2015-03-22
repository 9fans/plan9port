#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>
#include <sys/stat.h>
#include <dirent.h>

extern int _p9dir(struct stat*, struct stat*, char*, Dir*, char**, char*);

#if defined(__linux__)
static int
mygetdents(int fd, struct dirent *buf, int n)
{
	off_t off;
	int nn;

	/* This doesn't match the man page, but it works in Debian with a 2.2 kernel */
	off = p9seek(fd, 0, 1);
	nn = getdirentries(fd, (void*)buf, n, &off);
	return nn;
}
#elif defined(__APPLE__) 
static int
mygetdents(int fd, struct dirent *buf, int n)
{
	long off;
	return getdirentries(fd, (void*)buf, n, &off);
}
#elif defined(__FreeBSD__) || defined(__DragonFly__)
static int
mygetdents(int fd, struct dirent *buf, int n)
{
	off_t off;
	return getdirentries(fd, (void*)buf, n, &off);
}
#elif defined(__sun__) || defined(__NetBSD__) || defined(__OpenBSD__)
static int
mygetdents(int fd, struct dirent *buf, int n)
{
	return getdents(fd, (void*)buf, n);
}
#elif defined(__AIX__)
static int
mygetdents(int fd, struct dirent *buf, int n)
{
	return getdirent(fd, (void*)buf, n);
}
#endif

#if defined(__DragonFly__)
static inline int d_reclen(struct dirent *de) { return _DIRENT_DIRSIZ(de); }
#else
static inline int d_reclen(struct dirent *de) { return de->d_reclen; }
#endif

static int
countde(char *p, int n)
{
	char *e;
	int m;
	struct dirent *de;

	e = p+n;
	m = 0;
	while(p < e){
		de = (struct dirent*)p;
		if(d_reclen(de) <= 4+2+2+1 || p+d_reclen(de) > e)
			break;
		if(de->d_name[0]=='.' && de->d_name[1]==0)
			de->d_name[0] = 0;
		else if(de->d_name[0]=='.' && de->d_name[1]=='.' && de->d_name[2]==0)
			de->d_name[0] = 0;
		m++;
		p += d_reclen(de);
	}
	return m;
}

static int
dirpackage(int fd, char *buf, int n, Dir **dp)
{
	int oldwd;
	char *p, *str, *estr;
	int i, nstr, m;
	struct dirent *de;
	struct stat st, lst;
	Dir *d;

	n = countde(buf, n);
	if(n <= 0)
		return n;

	if((oldwd = open(".", O_RDONLY)) < 0)
		return -1;
	if(fchdir(fd) < 0)
		return -1;
		
	p = buf;
	nstr = 0;

	for(i=0; i<n; i++){
		de = (struct dirent*)p;
		memset(&lst, 0, sizeof lst);
		if(de->d_name[0] == 0)
			/* nothing */ {}
		else if(lstat(de->d_name, &lst) < 0)
			de->d_name[0] = 0;
		else{
			st = lst;
			if(S_ISLNK(lst.st_mode))
				stat(de->d_name, &st);
			nstr += _p9dir(&lst, &st, de->d_name, nil, nil, nil);
		}
		p += d_reclen(de);
	}

	d = malloc(sizeof(Dir)*n+nstr);
	if(d == nil){
		fchdir(oldwd);
		close(oldwd);
		return -1;
	}
	str = (char*)&d[n];
	estr = str+nstr;

	p = buf;
	m = 0;
	for(i=0; i<n; i++){
		de = (struct dirent*)p;
		if(de->d_name[0] != 0 && lstat(de->d_name, &lst) >= 0){
			st = lst;
			if((lst.st_mode&S_IFMT) == S_IFLNK)
				stat(de->d_name, &st);
			_p9dir(&lst, &st, de->d_name, &d[m++], &str, estr);
		}
		p += d_reclen(de);
	}

	fchdir(oldwd);
	close(oldwd);
	*dp = d;
	return m;
}

long
dirread(int fd, Dir **dp)
{
	char *buf;
	struct stat st;
	int n;

	*dp = 0;

	if(fstat(fd, &st) < 0)
		return -1;

	if(st.st_blksize < 8192)
		st.st_blksize = 8192;

	buf = malloc(st.st_blksize);
	if(buf == nil)
		return -1;

	n = mygetdents(fd, (void*)buf, st.st_blksize);
	if(n < 0){
		free(buf);
		return -1;
	}
	n = dirpackage(fd, buf, n, dp);
	free(buf);
	return n;
}


long
dirreadall(int fd, Dir **d)
{
	uchar *buf, *nbuf;
	long n, ts;
	struct stat st;

	if(fstat(fd, &st) < 0)
		return -1;

	if(st.st_blksize < 8192)
		st.st_blksize = 8192;

	buf = nil;
	ts = 0;
	for(;;){
		nbuf = realloc(buf, ts+st.st_blksize);
		if(nbuf == nil){
			free(buf);
			return -1;
		}
		buf = nbuf;
		n = mygetdents(fd, (void*)(buf+ts), st.st_blksize);
		if(n <= 0)
			break;
		ts += n;
	}
	if(ts >= 0)
		ts = dirpackage(fd, (char*)buf, ts, d);
	free(buf);
	if(ts == 0 && n < 0)
		return -1;
	return ts;
}
