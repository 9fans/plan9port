#define _GNU_SOURCE	/* for Linux O_DIRECT */
#include <u.h>
#include <dirent.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/stat.h>
#define NOPLAN9DEFINES
#include <libc.h>

static struct {
	Lock lk;
	DIR **d;
	int nd;
} dirs;

static int
dirput(int fd, DIR *d)
{
	int i, nd;
	DIR **dp;

	if(fd < 0) {
		werrstr("invalid fd");
		return -1;
	}
	lock(&dirs.lk);
	if(fd >= dirs.nd) {
		nd = dirs.nd*2;
		if(nd <= fd)
			nd = fd+1;
		dp = realloc(dirs.d, nd*sizeof dirs.d[0]);
		if(dp == nil) {
			werrstr("out of memory");
			unlock(&dirs.lk);
			return -1;
		}
		for(i=dirs.nd; i<nd; i++)
			dp[i] = nil;
		dirs.d = dp;
		dirs.nd = nd;
	}
	dirs.d[fd] = d;
	unlock(&dirs.lk);
	return 0;
}

static DIR*
dirget(int fd)
{
	DIR *d;

	lock(&dirs.lk);
	d = nil;
	if(0 <= fd && fd < dirs.nd)
		d = dirs.d[fd];
	unlock(&dirs.lk);
	return d;
}

static DIR*
dirdel(int fd)
{
	DIR *d;

	lock(&dirs.lk);
	d = nil;
	if(0 <= fd && fd < dirs.nd) {
		d = dirs.d[fd];
		dirs.d[fd] = nil;
	}
	unlock(&dirs.lk);
	return d;
}

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
	struct stat st;
	DIR *d;

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
		if(fstat(fd, &st) >= 0 && S_ISDIR(st.st_mode)) {
			d = fdopendir(fd);
			if(d == nil) {
				close(fd);
				return -1;
			}
			if(dirput(fd, d) < 0) {
				closedir(d);
				return -1;
			}
		}
		if(rclose)
			remove(name);
	}
	return fd;
}

vlong
p9seek(int fd, vlong offset, int whence)
{
	DIR *d;

	if((d = dirget(fd)) != nil) {
		if(whence == 1 && offset == 0)
			return telldir(d);
		if(whence == 0) {
			seekdir(d, offset);
			return 0;
		}
		werrstr("bad seek in directory");
		return -1;
	}

	return lseek(fd, offset, whence);
}

int
p9close(int fd)
{
	DIR *d;

	if((d = dirdel(fd)) != nil)
		return closedir(d);
	return close(fd);
}

typedef struct DirBuild DirBuild;
struct DirBuild {
	Dir *d;
	int nd;
	int md;
	char *str;
	char *estr;
};

extern int _p9dir(struct stat*, struct stat*, char*, Dir*, char**, char*);

static int
dirbuild1(DirBuild *b, struct stat *lst, struct stat *st, char *name)
{
	int i, nstr;
	Dir *d;
	int md, mstr;
	char *lo, *hi, *newlo;

	nstr = _p9dir(lst, st, name, nil, nil, nil);
	if(b->md-b->nd < 1 || b->estr-b->str < nstr) {
		// expand either d space or str space or both.
		md = b->md;
		if(b->md-b->nd < 1) {
			md *= 2;
			if(md < 16)
				md = 16;
		}
		mstr = b->estr-(char*)&b->d[b->md];
		if(b->estr-b->str < nstr) {
			mstr += nstr;
			mstr += mstr/2;
		}
		if(mstr < 512)
			mstr = 512;
		d = realloc(b->d, md*sizeof d[0] + mstr);
		if(d == nil)
			return -1;
		// move strings and update pointers in Dirs
		lo = (char*)&b->d[b->md];
		newlo = (char*)&d[md];
		hi = b->str;
		memmove(newlo, lo+((char*)d-(char*)b->d), hi-lo);
		for(i=0; i<b->nd; i++) {
			if(lo <= d[i].name && d[i].name < hi)
				d[i].name += newlo - lo;
			if(lo <= d[i].uid && d[i].uid < hi)
				d[i].uid += newlo - lo;
			if(lo <= d[i].gid && d[i].gid < hi)
				d[i].gid += newlo - lo;
			if(lo <= d[i].muid && d[i].muid < hi)
				d[i].muid += newlo - lo;
		}
		b->d = d;
		b->md = md;
		b->str += newlo - lo;
		b->estr = newlo + mstr;
	}
	_p9dir(lst, st, name, &b->d[b->nd], &b->str, b->estr);
	b->nd++;
	return 0;
}

static long
dirreadmax(int fd, Dir **dp, int max)
{
	int i;
	DIR *dir;
	DirBuild b;
	struct dirent *de;
	struct stat st, lst;

	if((dir = dirget(fd)) == nil) {
		werrstr("not a directory");
		return -1;
	}

	memset(&b, 0, sizeof b);
	for(i=0; max == -1 || i<max; i++) { // max = not too many, not too few
		errno = 0;
		de = readdir(dir);
		if(de == nil) {
			if(b.nd == 0 && errno != 0)
				return -1;
			break;
		}
		// Note: not all systems have d_namlen. Assume NUL-terminated.
		if(de->d_name[0]=='.' && de->d_name[1]==0)
			continue;
		if(de->d_name[0]=='.' && de->d_name[1]=='.' && de->d_name[2]==0)
			continue;
		if(fstatat(fd, de->d_name, &lst, AT_SYMLINK_NOFOLLOW) < 0)
			continue;
		st = lst;
		if(S_ISLNK(lst.st_mode))
			fstatat(fd, de->d_name, &st, 0);
		dirbuild1(&b, &lst, &st, de->d_name);
	}
	*dp = b.d;
	return b.nd;
}

long
dirread(int fd, Dir **dp)
{
	return dirreadmax(fd, dp, 10);
}

long
dirreadall(int fd, Dir **dp)
{
	return dirreadmax(fd, dp, -1);
}
