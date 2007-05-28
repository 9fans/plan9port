#include <u.h>
#include <libc.h>
#include <thread.h>
#include <sunrpc.h>
#include <nfs3.h>
#include <diskfs.h>

int allowall;

static Fsys *(*opentab[])(Disk*) =
{
	fsysopenffs,
	fsysopenhfs,
	fsysopenkfs,
	fsysopenext2,
	fsysopenfat,
};

Fsys*
fsysopen(Disk *disk)
{
	int i;
	Fsys *fsys;

	for(i=0; i<nelem(opentab); i++)
		if((fsys = (*opentab[i])(disk)) != nil)
			return fsys;
	return nil;
}

Block*
fsysreadblock(Fsys *fsys, u64int blockno)
{
	if(!fsys->_readblock){
		werrstr("no read dispatch function");
		return nil;
	}
	return (*fsys->_readblock)(fsys, blockno);
}

int
fsyssync(Fsys *fsys)
{
	if(disksync(fsys->disk) < 0)
		return -1;
	if(!fsys->_sync)
		return 0;
	return (*fsys->_sync)(fsys);
}

void
fsysclose(Fsys *fsys)
{
	if(!fsys->_close){
		fprint(2, "no fsysClose\n");
		abort();
	}
	(*fsys->_close)(fsys);
}

Nfs3Status
fsysroot(Fsys *fsys, Nfs3Handle *h)
{
	if(!fsys->_root)
		return Nfs3ErrNxio;
	return (*fsys->_root)(fsys, h);
}

Nfs3Status
fsyslookup(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, char *name, Nfs3Handle *nh)
{
	if(!fsys->_lookup)
		return Nfs3ErrNxio;
	return (*fsys->_lookup)(fsys, au, h, name, nh);
}

Nfs3Status
fsysgetattr(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, Nfs3Attr *attr)
{
	if(!fsys->_getattr)
		return Nfs3ErrNxio;
	return (*fsys->_getattr)(fsys, au, h, attr);
}

Nfs3Status
fsysreaddir(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, u32int count, u64int cookie, uchar **e, u32int *ne, u1int *peof)
{
	if(!fsys->_readdir)
		return Nfs3ErrNxio;
	return (*fsys->_readdir)(fsys, au, h, count, cookie, e, ne, peof);
}

Nfs3Status
fsysreadfile(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, u32int count, u64int offset, uchar **data, u32int *pcount, uchar *peof)
{
	if(!fsys->_readfile)
		return Nfs3ErrNxio;
	return (*fsys->_readfile)(fsys, au, h, count, offset, data, pcount, peof);
}

Nfs3Status
fsysreadlink(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, char **plink)
{
	if(!fsys->_readlink)
		return Nfs3ErrNxio;
	return (*fsys->_readlink)(fsys, au, h, plink);
}

Nfs3Status
fsysaccess(Fsys *fsys, SunAuthUnix *au, Nfs3Handle *h, u32int want, u32int *got, Nfs3Attr *attr)
{
	if(!fsys->_access)
		return Nfs3ErrNxio;
	return (*fsys->_access)(fsys, au, h, want, got, attr);
}
