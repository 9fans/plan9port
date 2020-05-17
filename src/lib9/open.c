#define _GNU_SOURCE	/* for Linux O_DIRECT */
#include <u.h>
#define NOPLAN9DEFINES
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>
#include <libc.h>
#include <sys/stat.h>
#include <dirent.h>
#ifndef O_DIRECT
#define O_DIRECT 0
#endif

int
p9create(char *path, int mode, ulong perm)
{
	int fd, cexec, umode, rclose, lock, rdwr;
	struct flock fl;

	rdwr = mode&3;
	lock = mode&OLOCK;
	cexec = mode&OCEXEC;
	rclose = mode&ORCLOSE;
	mode &= ~(ORCLOSE|OCEXEC|OLOCK);

	/* XXX should get mode mask right? */
	fd = -1;
	if(perm&DMDIR){
		if(mode != OREAD){
			werrstr("bad mode in directory create");
			goto out;
		}
		if(mkdir(path, perm&0777) < 0)
			goto out;
		fd = open(path, O_RDONLY);
	}else{
		umode = (mode&3)|O_CREAT|O_TRUNC;
		mode &= ~(3|OTRUNC);
		if(mode&ODIRECT){
			umode |= O_DIRECT;
			mode &= ~ODIRECT;
		}
		if(mode&OEXCL){
			umode |= O_EXCL;
			mode &= ~OEXCL;
		}
		if(mode&OAPPEND){
			umode |= O_APPEND;
			mode &= ~OAPPEND;
		}
		if(mode){
			werrstr("unsupported mode in create");
			goto out;
		}
		fd = open(path, umode, perm);
	}
out:
	if(fd >= 0){
		if(lock){
			fl.l_type = (rdwr==OREAD) ? F_RDLCK : F_WRLCK;
			fl.l_whence = SEEK_SET;
			fl.l_start = 0;
			fl.l_len = 0;
			if(fcntl(fd, F_SETLK, &fl) < 0){
				close(fd);
				werrstr("lock: %r");
				return -1;
			}
		}
		if(cexec)
			fcntl(fd, F_SETFL, FD_CLOEXEC);
		if(rclose)
			remove(path);
	}
	return fd;
}

int
p9open(char *name, int mode)
{
	int cexec, rclose;
	int fd, umode, lock, rdwr;
	struct flock fl;

	rdwr = mode&3;
	umode = rdwr;
	cexec = mode&OCEXEC;
	rclose = mode&ORCLOSE;
	lock = mode&OLOCK;
	mode &= ~(3|OCEXEC|ORCLOSE|OLOCK);
	if(mode&OTRUNC){
		umode |= O_TRUNC;
		mode ^= OTRUNC;
	}
	if(mode&ODIRECT){
		umode |= O_DIRECT;
		mode ^= ODIRECT;
	}
	if(mode&ONONBLOCK){
		umode |= O_NONBLOCK;
		mode ^= ONONBLOCK;
	}
	if(mode&OAPPEND){
		umode |= O_APPEND;
		mode ^= OAPPEND;
	}
	if(mode){
		werrstr("mode 0x%x not supported", mode);
		return -1;
	}
	fd = open(name, umode);
	if(fd >= 0){
		if(lock){
			fl.l_type = (rdwr==OREAD) ? F_RDLCK : F_WRLCK;
			fl.l_whence = SEEK_SET;
			fl.l_start = 0;
			fl.l_len = 0;
			if(fcntl(fd, F_SETLK, &fl) < 0){
				close(fd);
				werrstr("lock: %r");
				return -1;
			}
		}
		if(cexec)
			fcntl(fd, F_SETFL, FD_CLOEXEC);
		if(rclose)
			remove(name);
	}
	return fd;
}

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
