#include <u.h>
#include <libc.h>

#include <sys/stat.h>
#include <sys/disklabel.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>

int
_p9dir(struct stat *st, char *name, Dir *d, char **str, char *estr)
{
	char *s;
	char tmp[20];
	struct group *g;
	struct pwd *p;
	int sz;

	sz = 0;

	/* name */
	s = strrchr(name, '/');
	if(s && s[1])
		s++;
	else
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
		if(*str + strlen(s)+1 > estr){
			d->gid = "oops";	
		else{
			strcpy(*str, s);
			d->gid = *str;
			*str += strlen(*str)+1;
		}
	}

	if(d){
		d->muid = "";
		d->qid.path = ((uvlong)st->st_dev<<32) | st->st_ino;
		d->qid.vers = st->st_gen;
		d->mode = st->st_mode&0777;
		if(S_ISDIR(st->st_mode)){
			d->mode |= DMDIR;
			d->qid.type = QTDIR;
		}
		d->atime = st->st_atime;
		d->mtime = st->st_mtime;
		d->length = st->st_size;

		/* fetch real size for disks */
		if(S_ISCHR(st->st_mode)){
			int fd, n;
			struct disklabel lab;

			if((fd = open(name, O_RDONLY)) < 0)
				goto nosize;
			if(ioctl(fd, DIOCGDINFO, &lab) < 0)
				goto nosize;
			n = minor(st->st_rdev)&0xFFFF;
			if(n >= lab.d_npartitions)
				goto nosize;
			d->length = (vlong)lab.d_npartitions[n].p_size * lab.d_secsize;
		nosize:
			if(fd >= 0)
				close(fd);
		}
	}

	return sz;
}

Dir*
_dirfstat(char *name, int fd)
{
	Dir *d;
	int size;

	
}
