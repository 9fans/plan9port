#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <9pclient.h>
#include "plumb.h"

static CFsys *fsplumb;
static int pfd = -1;
static CFid *pfid;

int
plumbunmount(void)
{
	CFsys *fsys;

	if(fsplumb){
		fsys = fsplumb;
		fsplumb = nil;
		fsunmount(fsys);
	}
	return 0;
}

int
plumbopen(char *name, int omode)
{
	if(fsplumb == nil)
		fsplumb = nsmount("plumb", "");
	if(fsplumb == nil)
		return -1;
	/*
	* It's important that when we send something,
	* we find out whether it was a valid plumb write.
	* (If it isn't, the client might fall back to some
	* other mechanism or indicate to the user what happened.)
	* We can't use a pipe for this, so we have to use the
	* fid interface.  But we need to return a fd.
	* Return a fd for /dev/null so that we return a unique
	* file descriptor.  In plumbsend we'll look for pfd
	* and use the recorded fid instead.
	*/
	if((omode&3) == OWRITE){
		if(pfd != -1){
			werrstr("already have plumb send open");
			return -1;
		}
		pfd = open("/dev/null", OWRITE);
		if(pfd < 0)
			return -1;
		pfid = fsopen(fsplumb, name, omode);
		if(pfid == nil){
			close(pfd);
			pfd = -1;
			return -1;
		}
		return pfd;
	}

	return fsopenfd(fsplumb, name, omode);
}

CFid*
plumbopenfid(char *name, int mode)
{
	if(fsplumb == nil){
		fsplumb = nsmount("plumb", "");
		if(fsplumb == nil){
			werrstr("mount plumb: %r");
			return nil;
		}
	}
	return fsopen(fsplumb, name, mode);
}

int
plumbsendtofid(CFid *fid, Plumbmsg *m)
{
	char *buf;
	int n;

	if(fid == nil){
		werrstr("invalid fid");
		return -1;
	}
	buf = plumbpack(m, &n);
	if(buf == nil)
		return -1;
	n = fswrite(fid, buf, n);
	free(buf);
	return n;
}

int
plumbsend(int fd, Plumbmsg *m)
{
	if(fd == -1){
		werrstr("invalid fd");
		return -1;
	}
	if(fd != pfd){
		werrstr("fd is not the plumber");
		return -1;
	}
	return plumbsendtofid(pfid, m);
}

Plumbmsg*
plumbrecv(int fd)
{
	char *buf;
	Plumbmsg *m;
	int n, more;

	buf = malloc(8192);
	if(buf == nil)
		return nil;
	n = read(fd, buf, 8192);
	m = nil;
	if(n > 0){
		m = plumbunpackpartial(buf, n, &more);
		if(m==nil && more>0){
			/* we now know how many more bytes to read for complete message */
			buf = realloc(buf, n+more);
			if(buf == nil)
				return nil;
			if(readn(fd, buf+n, more) == more)
				m = plumbunpackpartial(buf, n+more, nil);
		}
	}
	free(buf);
	return m;
}

Plumbmsg*
plumbrecvfid(CFid *fid)
{
	char *buf;
	Plumbmsg *m;
	int n, more;

	if(fid == nil){
		werrstr("invalid fid");
		return nil;
	}
	buf = malloc(8192);
	if(buf == nil)
		return nil;
	n = fsread(fid, buf, 8192);
	m = nil;
	if(n > 0){
		m = plumbunpackpartial(buf, n, &more);
		if(m==nil && more>0){
			/* we now know how many more bytes to read for complete message */
			buf = realloc(buf, n+more);
			if(buf == nil)
				return nil;
			if(fsreadn(fid, buf+n, more) == more)
				m = plumbunpackpartial(buf, n+more, nil);
		}
	}
	free(buf);
	return m;
}
