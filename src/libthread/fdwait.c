#define NOPLAN9DEFINES
#include <u.h>
#include <libc.h>
#include <thread.h>
#include "threadimpl.h"
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

void
fdwait()
{
	fd_set rfd, wfd, efd;

	FD_ZERO(&rfd);
	FD_ZERO(&wfd);
	FD_ZERO(&efd);
	if(mode=='w')
		FD_SET(&wfd, fd);
	else
		FD_SET(&rfd, fd);
	FD_SET(&efd, fd);
	select(fd+1, &rfd, &wfd, &efd, nil);
}

void
threadfdwaitsetup(void)
{
}

void
_threadfdwait(int fd, int rw, ulong pc)
{
	int i;

	struct {
		Channel c;
		ulong x;
		Alt *qentry[2];
	} s;

	threadfdwaitsetup();
	chaninit(&s.c, sizeof(ulong), 1);
	s.c.qentry = (volatile Alt**)s.qentry;
	s.c.nentry = 2;
	memset(s.qentry, 0, sizeof s.qentry);
	for(i=0; i<npoll; i++)
		if(pfd[i].fd == -1)
			break;
	if(i==npoll){
		if(npoll >= nelem(polls)){
			fprint(2, "Too many polled fds.\n");
			abort();
		}
		npoll++;
	}

	pfd[i].fd = fd;
	pfd[i].events = rw=='r' ? POLLIN : POLLOUT;
	polls[i].c = &s.c;
	if(0) fprint(2, "%s [%3d] fdwait %d %c list *0x%lux\n",
		argv0, threadid(), fd, rw, pc);
	if(pollpid)
		postnote(PNPROC, pollpid, "interrupt");
	recvul(&s.c);
}

void
threadfdwait(int fd, int rw)
{
	_threadfdwait(fd, rw, getcallerpc(&fd));
}

void
threadsleep(int ms)
{
	struct {
		Channel c;
		ulong x;
		Alt *qentry[2];
	} s;

	threadfdwaitsetup();
	chaninit(&s.c, sizeof(ulong), 1);
	s.c.qentry = (volatile Alt**)s.qentry;
	s.c.nentry = 2;
	memset(s.qentry, 0, sizeof s.qentry);
	
	sleepchan[nsleep] = &s.c;
	sleeptime[nsleep++] = p9nsec()/1000000+ms;
	recvul(&s.c);
}

void
threadfdnoblock(int fd)
{
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0)|O_NONBLOCK);
}

long
threadread(int fd, void *a, long n)
{
	int nn;

	threadfdnoblock(fd);
again:
	/*
	 * Always call wait (i.e. don't optimistically try the read)
	 * so that the scheduler gets a chance to run other threads.
	 */
	_threadfdwait(fd, 'r', getcallerpc(&fd));
	errno = 0;
	nn = read(fd, a, n);
	if(nn <= 0){
		if(errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
			goto again;
	}
	return nn;
}

int
threadrecvfd(int fd)
{
	int nn;

	threadfdnoblock(fd);
again:
	/*
	 * Always call wait (i.e. don't optimistically try the recvfd)
	 * so that the scheduler gets a chance to run other threads.
	 */
	_threadfdwait(fd, 'r', getcallerpc(&fd));
	nn = recvfd(fd);
	if(nn < 0){
		if(errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
			goto again;
	}
	return nn;
}

int
threadsendfd(int fd, int sfd)
{
	int nn;

	threadfdnoblock(fd);
again:
	/*
	 * Always call wait (i.e. don't optimistically try the sendfd)
	 * so that the scheduler gets a chance to run other threads.
	 */
	_threadfdwait(fd, 'w', getcallerpc(&fd));
	nn = sendfd(fd, sfd);
	if(nn < 0){
		if(errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
			goto again;
	}
	return nn;
}

long
threadreadn(int fd, void *a, long n)
{
	int tot, nn;

	for(tot = 0; tot<n; tot+=nn){
		nn = threadread(fd, (char*)a+tot, n-tot);
		if(nn <= 0){
			if(tot == 0)
				return nn;
			return tot;
		}
	}
	return tot;
}

long
_threadwrite(int fd, const void *a, long n)
{
	int nn;

	threadfdnoblock(fd);
again:
	/*
	 * Always call wait (i.e. don't optimistically try the write)
	 * so that the scheduler gets a chance to run other threads.
	 */
	_threadfdwait(fd, 'w', getcallerpc(&fd));
	nn = write(fd, a, n);
	if(nn < 0){
		if(errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
			goto again;
	}
	return nn;
}

long
threadwrite(int fd, const void *a, long n)
{
	int tot, nn;

	for(tot = 0; tot<n; tot+=nn){
		nn = _threadwrite(fd, (char*)a+tot, n-tot);
		if(nn <= 0){
			if(tot == 0)
				return nn;
			return tot;
		}
	}
	return tot;
}

int
threadannounce(char *addr, char *dir)
{
	return p9announce(addr, dir);
}

int
threadlisten(char *dir, char *newdir)
{
	int fd, ret;
	extern int _p9netfd(char*);

	fd = _p9netfd(dir);
	if(fd < 0){
		werrstr("bad 'directory' in listen: %s", dir);
		return -1;
	}
	threadfdnoblock(fd);
	while((ret = p9listen(dir, newdir)) < 0 && errno==EAGAIN)
		_threadfdwait(fd, 'r', getcallerpc(&dir));
	return ret;
}

int
threadaccept(int cfd, char *dir)
{
	return p9accept(cfd, dir);
}

