#define NOPLAN9DEFINES
#include <u.h>
#include <libc.h>
#include <thread.h>

#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>

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
		//fprint(2, "poll %d:", npoll);
		for(i=0; i<npoll; i++){
			//fprint(2, " %d%c", pfd[i].fd, pfd[i].events==POLLIN ? 'r' : 'w');
			pfd[i].revents = 0;
		}
		t = -1;
		for(i=0; i<nsleep; i++){
			now = p9nsec()/1000000;
			n = sleeptime[i] - now;
			if(n < 0)
				n = 0;
			if(t == -1 || n < t)
				t = n;
		}
		//fprint(2, "\n");
	
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
				nbsendul(polls[i].c, 1);
				pfd[i].fd = -1;
				pfd[i].events = 0;
				pfd[i].revents = 0;
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
threadfdwait(int fd, int rw)
{
	int i;

	struct {
		Channel c;
		ulong x;
	} s;

	threadfdwaitsetup();
	chaninit(&s.c, sizeof(ulong), 1);
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
	//threadstate("fdwait %d %d", f->fd, e);
	recvul(&s.c);
}

void
threadsleep(int ms)
{
	struct {
		Channel c;
		ulong x;
	} s;

	threadfdwaitsetup();
	chaninit(&s.c, sizeof(ulong), 1);

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
	nn = read(fd, a, n);
	if(nn < 0){
		if(errno == EINTR)
			goto again;
		if(errno == EAGAIN || errno == EWOULDBLOCK){
			threadfdwait(fd, 'r');
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
			threadfdwait(fd, 'r');
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
			threadfdwait(fd, 'w');
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
			threadfdwait(fd, 'w');
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

