#include <u.h>
#include <libc.h>
#include <ip.h>
#include <thread.h>
#include <sunrpc.h>

typedef struct Arg Arg;
struct Arg
{
	int fd;
	char adir[40];
	SunSrv *srv;
};

static void
sunnetlisten(void *v)
{
	int fd, lcfd;
	char ldir[40];
	uchar ip[IPaddrlen];
	int port;
	Arg *a = v;
	NetConnInfo *nci;

	for(;;){
		lcfd = listen(a->adir, ldir);
		if(lcfd < 0)
			break;
		fd = accept(lcfd, ldir);
		close(lcfd);
		if(fd < 0)
			continue;
		if(a->srv->ipokay){
			if((nci = getnetconninfo(nil, fd)) == nil){
				close(fd);
				continue;
			}
			port = atoi(nci->rserv);
			parseip(ip, nci->raddr);
			freenetconninfo(nci);
			if(!a->srv->ipokay(ip, port)){
				close(fd);
				continue;
			}
		}
		if(!sunsrvfd(a->srv, fd))
			close(fd);
	}
	free(a);
	close(a->fd);
}

int
sunsrvnet(SunSrv *srv, char *addr)
{
	Arg *a;

	a = emalloc(sizeof(Arg));
	if((a->fd = announce(addr, a->adir)) < 0)
		return -1;
	a->srv = srv;

	proccreate(sunnetlisten, a, SunStackSize);
	return 0;
}

int
sunsrvannounce(SunSrv *srv, char *addr)
{
	if(strstr(addr, "udp!"))
		return sunsrvudp(srv, addr);
	else
		return sunsrvnet(srv, addr);
}
