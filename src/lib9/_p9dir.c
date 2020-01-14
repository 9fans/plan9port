#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>

#if defined(__APPLE__)
#define _HAVESTGEN
#include <sys/disk.h>
static vlong
disksize(int fd, struct stat *st)
{
	u64int bc;
	u32int bs;

	bs = 0;
	bc = 0;
	ioctl(fd, DKIOCGETBLOCKSIZE, &bs);
	ioctl(fd, DKIOCGETBLOCKCOUNT, &bc);
	if(bs >0 && bc > 0)
		return bc*bs;
	return 0;
}

#elif defined(__FreeBSD__)
#define _HAVESTGEN
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
static vlong
disksize(int fd, struct stat *st)
{
	off_t mediasize;

	if(ioctl(fd, DIOCGMEDIASIZE, &mediasize) >= 0)
		return mediasize;
	return 0;
}

#elif defined(__OpenBSD__)
#define _HAVESTGEN
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/dkio.h>
static vlong
disksize(int fd, struct stat *st)
{
	struct disklabel lab;
	int n;

	if(!S_ISCHR(st->st_mode))
		return 0;
	if(ioctl(fd, DIOCGDINFO, &lab) < 0)
		return 0;
	n = minor(st->st_rdev)&7;
	if(n >= lab.d_npartitions)
		return 0;
	return (vlong)lab.d_partitions[n].p_size * lab.d_secsize;
}

#elif defined(__linux__)
#include <linux/hdreg.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#undef major
#define major(dev) ((int)(((dev) >> 8) & 0xff))
static vlong
disksize(int fd, struct stat *st)
{
	u64int u64;
	long l;
	struct hd_geometry geo;

	memset(&geo, 0, sizeof geo);
	l = 0;
	u64 = 0;
#ifdef BLKGETSIZE64
	if(ioctl(fd, BLKGETSIZE64, &u64) >= 0)
		return u64;
#endif
	if(ioctl(fd, BLKGETSIZE, &l) >= 0)
		return l*512;
	if(ioctl(fd, HDIO_GETGEO, &geo) >= 0)
		return (vlong)geo.heads*geo.sectors*geo.cylinders*512;
	return 0;
}

#else
static vlong
disksize(int fd, struct stat *st)
{
	return 0;
}
#endif

int _p9usepwlibrary = 1;
/*
 * Caching the last group and passwd looked up is
 * a significant win (stupidly enough) on most systems.
 * It's not safe for threaded programs, but neither is using
 * getpwnam in the first place, so I'm not too worried.
 */
int
_p9dir(struct stat *lst, struct stat *st, char *name, Dir *d, char **str, char *estr)
{
	char *s;
	char tmp[20];
	static struct group *g;
	static struct passwd *p;
	static int gid, uid;
	int sz, fd;

	fd = -1;
	USED(fd);
	sz = 0;
	if(d)
		memset(d, 0, sizeof *d);

	/* name */
	s = strrchr(name, '/');
	if(s)
		s++;
	if(!s || !*s)
		s = name;
	if(*s == '/')
		s++;
	if(*s == 0)
		s = "/";
	if(d){
		if(*str + strlen(s)+1 > estr)
			d->name = "oops";
		else{
			strcpy(*str, s);
			d->name = *str;
			*str += strlen(*str)+1;
		}
	}
	sz += strlen(s)+1;

	/* user */
	if(p && st->st_uid == uid && p->pw_uid == uid)
		;
	else if(_p9usepwlibrary){
		p = getpwuid(st->st_uid);
		uid = st->st_uid;
	}
	if(p == nil || st->st_uid != uid || p->pw_uid != uid){
		snprint(tmp, sizeof tmp, "%d", (int)st->st_uid);
		s = tmp;
	}else
		s = p->pw_name;
	sz += strlen(s)+1;
	if(d){
		if(*str+strlen(s)+1 > estr)
			d->uid = "oops";
		else{
			strcpy(*str, s);
			d->uid = *str;
			*str += strlen(*str)+1;
		}
	}

	/* group */
	if(g && st->st_gid == gid && g->gr_gid == gid)
		;
	else if(_p9usepwlibrary){
		g = getgrgid(st->st_gid);
		gid = st->st_gid;
	}
	if(g == nil || st->st_gid != gid || g->gr_gid != gid){
		snprint(tmp, sizeof tmp, "%d", (int)st->st_gid);
		s = tmp;
	}else
		s = g->gr_name;
	sz += strlen(s)+1;
	if(d){
		if(*str + strlen(s)+1 > estr)
			d->gid = "oops";
		else{
			strcpy(*str, s);
			d->gid = *str;
			*str += strlen(*str)+1;
		}
	}

	if(d){
		d->type = 'M';

		d->muid = "";
		d->qid.path = st->st_ino;
		/*
		 * do not include st->st_dev in path, because
		 * automounters give the same file system different
		 * st_dev values for successive mounts, causing
		 * spurious write warnings in acme and sam.
		d->qid.path |= (uvlong)st->st_dev<<32;
		 */
#ifdef _HAVESTGEN
		d->qid.vers = st->st_gen;
#endif
		if(d->qid.vers == 0)
			d->qid.vers = st->st_mtime + st->st_ctime;
		d->mode = st->st_mode&0777;
		d->atime = st->st_atime;
		d->mtime = st->st_mtime;
		d->length = st->st_size;

		if(S_ISLNK(lst->st_mode)){	/* yes, lst not st */
			d->mode |= DMSYMLINK;
			d->length = lst->st_size;
		}
		else if(S_ISDIR(st->st_mode)){
			d->length = 0;
			d->mode |= DMDIR;
			d->qid.type = QTDIR;
		}
		else if(S_ISFIFO(st->st_mode))
			d->mode |= DMNAMEDPIPE;
		else if(S_ISSOCK(st->st_mode))
			d->mode |= DMSOCKET;
		else if(S_ISBLK(st->st_mode)){
			d->mode |= DMDEVICE;
			d->qid.path = ('b'<<16)|st->st_rdev;
		}
		else if(S_ISCHR(st->st_mode)){
			d->mode |= DMDEVICE;
			d->qid.path = ('c'<<16)|st->st_rdev;
		}
		/* fetch real size for disks */
		if(S_ISBLK(lst->st_mode)){
			if((fd = open(name, O_RDONLY)) >= 0){
				d->length = disksize(fd, st);
				close(fd);
			}
		}
	}

	return sz;
}
