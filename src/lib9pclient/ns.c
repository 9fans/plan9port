#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <9pclient.h>
#include <ctype.h>

CFsys*
nsmount(char *name, char *aname)
{
	char *addr, *ns;
	int fd;
	CFsys *fs;

	ns = getns();
	if(ns == nil)
		return nil;

	addr = smprint("unix!%s/%s", ns, name);
	free(ns);
	if(addr == nil)
		return nil;

	fd = dial(addr, 0, 0, 0);
	if(fd < 0){
		werrstr("dial %s: %r", addr);
		free(addr);
		return nil;
	}
	free(addr);

	fcntl(fd, F_SETFL, FD_CLOEXEC);

	fs = fsmount(fd, aname);
	if(fs == nil){
		close(fd);
		return nil;
	}

	return fs;
}
