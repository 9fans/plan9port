#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>

#if defined(__FreeBSD__)
#include <sys/disklabel.h>
#define _HAVEDISKLABEL
#endif

#if !defined(__linux__) && !defined(__sun__)
#define _HAVESTGEN
#endif

int
_p9dir(struct stat *st, char *name, Dir *d, char **str, char *estr)
{
	char *s;
	char tmp[20];
	struct group *g;
	struct passwd *p;
	int sz;

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
	p = getpwuid(st->st_uid);
	if(p == nil){
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
	g = getgrgid(st->st_gid);
	if(g == nil){
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
		d->qid.path = ((uvlong)st->st_dev<<32) | st->st_ino;
#ifdef _HAVESTGEN
		d->qid.vers = st->st_gen;
#endif
		d->mode = st->st_mode&0777;
		d->atime = st->st_atime;
		d->mtime = st->st_mtime;
		d->length = st->st_size;

		if(S_ISDIR(st->st_mode)){
			d->length = 0;
			d->mode |= DMDIR;
			d->qid.type = QTDIR;
		}

		/* fetch real size for disks */
#ifdef _HAVEDISKLABEL
		if(S_ISCHR(st->st_mode)){
			int fd, n;
			struct disklabel lab;

			if((fd = open(name, O_RDONLY)) < 0)
				goto nosize;
			if(ioctl(fd, DIOCGDINFO, &lab) < 0)
				goto nosize;
			n = minor(st->st_rdev)&7;
			if(n >= lab.d_npartitions)
				goto nosize;

			d->length = (vlong)(lab.d_partitions[n].p_size) * lab.d_secsize;

		nosize:
			if(fd >= 0)
				close(fd);
		}
#endif
	}

	return sz;
}

