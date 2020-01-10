#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <9pclient.h>
#include <ctype.h>

CFsys*
nsinit(char *name)
{
	char *addr, *ns;
	int fd;

	ns = getns();
	if(ns == nil){
		werrstr("no name space");
		return nil;
	}

	addr = smprint("unix!%s/%s", ns, name);
	free(ns);
	if(addr == nil){
		werrstr("smprint: %r");
		return nil;
	}

	fd = dial(addr, 0, 0, 0);
	if(fd < 0){
		werrstr("dial %s: %r", addr);
		free(addr);
		return nil;
	}
	free(addr);

	fcntl(fd, F_SETFD, FD_CLOEXEC);
	return fsinit(fd);
}

CFsys*
nsmount(char *name, char *aname)
{
	CFsys *fs;
	CFid *fid;

	fs = nsinit(name);
	if(fs == nil)
		return nil;
	if((fid = fsattach(fs, nil, getuser(), aname)) == nil){
		_fsunmount(fs);
		return nil;
	}
	fssetroot(fs, fid);
	return fs;
}

CFid*
nsopen(char *name, char *aname, char *fname, int mode)
{
	CFsys *fs;
	CFid *fid;

	fs = nsmount(name, aname);
	if(fs == nil)
		return nil;
	fid = fsopen(fs, fname, mode);
	fsunmount(fs);
	return fid;
}
