#define NOPLAN9DEFINES
#include <u.h>
#include <libc.h>
#include <thread.h>
#include "threadimpl.h"
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#define debugpoll 0

#ifdef __APPLE__
#include <sys/time.h>
enum { POLLIN=1, POLLOUT=2, POLLERR=4 };
struct pollfd
{
	int fd;
	int events;
	int revents;
};

int
poll(struct pollfd *p, int np, int ms)
{
	int i, maxfd, n;
	struct timeval tv, *tvp;
	fd_set rfd, wfd, efd;
	
	maxfd = -1;
	FD_ZERO(&rfd);
	FD_ZERO(&wfd);
	FD_ZERO(&efd);
	for(i=0; i<np; i++){
		p[i].revents = 0;
		if(p[i].fd == -1)
			continue;
		if(p[i].fd > maxfd)
			maxfd = p[i].fd;
		if(p[i].events & POLLIN)
			FD_SET(p[i].fd,	&rfd);
		if(p[i].events & POLLOUT)
			FD_SET(p[i].fd, &wfd);
		FD_SET(p[i].fd, &efd);
	}

	if(ms != -1){
		tv.tv_usec = (ms%1000)*1000;
		tv.tv_sec = ms/1000;
		tvp = &tv;
	}else
		tvp = nil;

	if(debugpoll){
		fprint(2, "select %d:", maxfd+1);
		for(i=0; i<=maxfd; i++){
			if(FD_ISSET(i, &rfd))
				fprint(2, " r%d", i);
			if(FD_ISSET(i, &wfd))
				fprint(2, " w%d", i);
			if(FD_ISSET(i, &efd))
				fprint(2, " e%d", i);
		}
		fprint(2, "; tp=%p, t=%d.%d\n", tvp, tv.tv_sec, tv.tv_usec);
	}

	n = select(maxfd+1, &rfd, &wfd, &efd, tvp);

	if(n <= 0)
		return n;

	for(i=0; i<np; i++){
		if(p[i].fd == -1)
			continue;
		if(FD_ISSET(p[i].fd, &rfd))
			p[i].revents |= POLLIN;
		if(FD_ISSET(p[i].fd, &wfd))
			p[i].revents |= POLLOUT;
		if(FD_ISSET(p[i].fd, &efd))
			p[i].revents |= POLLERR;
	} 
	return n;
}

#else
#include <poll.h>
#endif

/*
 * Poll file descriptors in an idle loop.
 */

typedef struct Poll Poll;

struct Poll
{
	Channel *c;	/* for sending back */
};

static Channel *sleepchan[64];
static int sleeptime[64];
static int nsleep;

static struct pollfd pfd[64];
static struct Poll polls[64];
static int npoll;

static void
pollidle(void *v)
{
	int i, n, t;
	uint now;

	for(;; yield()){
		if(debugpoll) fprint(2, "poll %d:", npoll);
		for(i=0; i<npoll; i++){
			if(debugpoll) fprint(2, " %d%c", pfd[i].fd, pfd[i].events==POLLIN ? 'r' : 'w');
			pfd[i].revents = 0;
		}
		t = -1;
		now = p9nsec()/1000000;
		for(i=0; i<nsleep; i++){
			n = sleeptime[i] - now;
			if(debugpoll) fprint(2, " s%d", n);
			if(n < 0)
				n = 0;
			if(t == -1 || n < t)
				t = n;
		}
		if(debugpoll) fprint(2, "; t=%d\n", t);
	
		n = poll(pfd, npoll, t);
		//fprint(2, "poll ret %d:", n);
		now = p9nsec()/1000000;
		for(i=0; i<nsleep; i++){
			if((int)(sleeptime[i] - now) < 0){
				nbsendul(sleepchan[i], 0);
				nsleep--;
				sleepchan[i] = sleepchan[nsleep];
				sleeptime[i] = sleeptime[nsleep];
				i--;
			}
		}
				
		if(n <= 0)
			continue;
		for(i=0; i<npoll; i++)
			if(pfd[i].fd != -1 && pfd[i].revents){
				//fprint(2, " %d", pfd[i].fd);
				pfd[i].fd = -1;
				pfd[i].events = 0;
				pfd[i].revents = 0;
				nbsendul(polls[i].c, 1);
				//fprint(2, " x%d", pfd[i].fd);
			}
		//fprint(2, "\n");
	}
}

void
threadfdwaitsetup(void)
{
	static int setup = 0;

	if(!setup){
		setup = 1;
		threadcreateidle(pollidle, nil, 16384);
	}
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
	errno = 0;
	nn = read(fd, a, n);
	if(nn <= 0){
		if(errno == EINTR)
			goto again;
		if(errno == EAGAIN || errno == EWOULDBLOCK){
			_threadfdwait(fd, 'r', getcallerpc(&fd));
			goto again;
		}
	}
	return nn;
}

int
threadrecvfd(int fd)
{
	int nn;

	threadfdnoblock(fd);
again:
	nn = recvfd(fd);
	if(nn < 0){
		if(errno == EINTR)
			goto again;
		if(errno == EAGAIN || errno == EWOULDBLOCK){
			_threadfdwait(fd, 'r', getcallerpc(&fd));
			goto again;
		}
	}
	return nn;
}

int
threadsendfd(int fd, int sfd)
{
	int nn;

	threadfdnoblock(fd);
again:
	nn = sendfd(fd, sfd);
	if(nn < 0){
		if(errno == EINTR)
			goto again;
		if(errno == EAGAIN || errno == EWOULDBLOCK){
			_threadfdwait(fd, 'w', getcallerpc(&fd));
			goto again;
		}
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
	nn = write(fd, a, n);
	if(nn < 0){
		if(errno == EINTR)
			goto again;
		if(errno == EAGAIN || errno == EWOULDBLOCK){
			_threadfdwait(fd, 'w', getcallerpc(&fd));
			goto again;
		}
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

