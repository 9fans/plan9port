#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include <thread.h>
#include "9p.h"

static void
incfidref(void *v)
{
	Fid *f;

	f = v;
	if(f)
		incref(&f->ref);
}

Fidpool*
allocfidpool(void (*destroy)(Fid*))
{
	Fidpool *f;

	f = emalloc9p(sizeof *f);
	f->map = allocmap(incfidref);
	f->destroy = destroy;
	return f;
}

void
freefidpool(Fidpool *p)
{
	freemap(p->map, (void(*)(void*))p->destroy);
	free(p);
}

Fid*
allocfid(Fidpool *pool, ulong fid)
{
	Fid *f;

	f = emalloc9p(sizeof *f);
	f->fid = fid;
	f->omode = -1;
	f->pool = pool;

	incfidref(f);
	incfidref(f);
	if(caninsertkey(pool->map, fid, f) == 0){
		closefid(f);
		closefid(f);
		return nil;
	}

	return f;
}

Fid*
lookupfid(Fidpool *pool, ulong fid)
{
	return lookupkey(pool->map, fid);
}

void
closefid(Fid *f)
{
	if(decref(&f->ref) == 0) {
		if(f->rdir)
			closedirfile(f->rdir);
		if(f->pool->destroy)
			f->pool->destroy(f);
		if(f->file)
			closefile(f->file);
		free(f->uid);
		free(f);
	}
}

Fid*
removefid(Fidpool *pool, ulong fid)
{
	return deletekey(pool->map, fid);
}
