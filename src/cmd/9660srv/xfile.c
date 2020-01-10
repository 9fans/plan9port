#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include "dat.h"
#include "fns.h"

static Xfile*	clean(Xfile*);

#define	FIDMOD	127	/* prime */

static Xdata*	xhead;
static Xfile*	xfiles[FIDMOD];
static Xfile*	freelist;

Xdata*
getxdata(char *name)
{
	int fd;
	Dir *dir;
	Xdata *xf, *fxf;
	int flag;

	if(name[0] == 0)
		name = deffile;
	if(name == 0)
		error(Enofile);
	flag = (access(name, 6) == 0) ? ORDWR : OREAD;
	fd = open(name, flag);
	if(fd < 0)
		error(Enonexist);
	dir = nil;
	if(waserror()){
		close(fd);
		free(dir);
		nexterror();
	}
	if((dir = dirfstat(fd)) == nil)
		error("I/O error");
	if((dir->qid.type & ~QTTMP) != QTFILE)
		error("attach name not a plain file");
	for(fxf=0,xf=xhead; xf; xf=xf->next){
		if(xf->name == 0){
			if(fxf == 0)
				fxf = xf;
			continue;
		}
		if(xf->qid.path != dir->qid.path || xf->qid.vers != dir->qid.vers)
			continue;
		if(xf->type != dir->type || xf->fdev != dir->dev)
			continue;
		xf->ref++;
		chat("incref=%d, \"%s\", dev=%d...", xf->ref, xf->name, xf->dev);
		close(fd);
		poperror();
		free(dir);
		return xf;
	}
	if(fxf==0){
		fxf = ealloc(sizeof(Xfs));
		fxf->next = xhead;
		xhead = fxf;
	}
	chat("alloc \"%s\", dev=%d...", name, fd);
	fxf->ref = 1;
	fxf->name = strcpy(ealloc(strlen(name)+1), name);
	fxf->qid = dir->qid;
	fxf->type = dir->type;
	fxf->fdev = dir->dev;
	fxf->dev = fd;
	free(dir);
	poperror();
	return fxf;
}

static void
putxdata(Xdata *d)
{
	if(d->ref <= 0)
		panic(0, "putxdata");
	d->ref--;
	chat("decref=%d, \"%s\", dev=%d...", d->ref, d->name, d->dev);
	if(d->ref == 0){
		chat("purgebuf...");
		purgebuf(d);
		close(d->dev);
		free(d->name);
		d->name = 0;
	}
}

void
refxfs(Xfs *xf, int delta)
{
	xf->ref += delta;
	if(xf->ref == 0){
		if(xf->d)
			putxdata(xf->d);
		if(xf->ptr)
			free(xf->ptr);
		free(xf);
	}
}

Xfile*
xfile(int fid, int flag)
{
	int k = fid%FIDMOD;
	Xfile **hp=&xfiles[k], *f, *pf;

	for(f=*hp,pf=0; f; pf=f,f=f->next)
		if(f->fid == fid)
			break;
	if(f && pf){
		pf->next = f->next;
		f->next = *hp;
		*hp = f;
	}
	switch(flag){
	default:
		panic(0, "xfile");
	case Asis:
		if(f == 0)
			error("unassigned fid");
		return f;
	case Clean:
		break;
	case Clunk:
		if(f){
			*hp = f->next;
			clean(f);
			f->next = freelist;
			freelist = f;
		}
		return 0;
	}
	if(f)
		return clean(f);
	if(f = freelist)	/* assign = */
		freelist = f->next;
	else
		f = ealloc(sizeof(Xfile));
	f->next = *hp;
	*hp = f;
	f->xf = 0;
	f->fid = fid;
	f->flags = 0;
	f->qid = (Qid){0,0,0};
	f->len = 0;
	f->ptr = 0;
	return f;
}

static Xfile *
clean(Xfile *f)
{
	if(f->xf){
		refxfs(f->xf, -1);
		f->xf = 0;
	}
	if(f->len){
		free(f->ptr);
		f->len = 0;
	}
	f->ptr = 0;
	f->flags = 0;
	f->qid = (Qid){0,0,0};
	return f;
}
