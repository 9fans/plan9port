#include <u.h>
#define NOPLAN9DEFINES
#include <libc.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <errno.h>

#undef sun
#define sun sockun

extern int _p9netfd(char*);

static char *unknown = "unknown";

static int
convert(int s, struct sockaddr *sa, char **lsys, char **lserv, char **laddr)
{
	struct sockaddr_un *sun;
	struct sockaddr_in *sin;
	uchar *ip;
	u32int ipl;
	socklen_t sn;
	int n;
	char *net;
	
	switch(sa->sa_family){
	case AF_INET:
		sin = (void*)sa;
		ip = (uchar*)&sin->sin_addr;
		ipl = *(u32int*)ip;
		if(ipl == 0)
			*lsys = strdup("*");
		else
			*lsys = smprint("%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
		*lserv = smprint("%d", ntohs(sin->sin_port));
		sn = sizeof n;
		if(getsockopt(s, SOL_SOCKET, SO_TYPE, (void*)&n, &sn) < 0)
			return -1;
		if(n == SOCK_STREAM)
			net = "tcp";
		else if(n == SOCK_DGRAM)
			net = "udp";
		else{
			werrstr("unknown network type");
			return -1;
		}
		*laddr = smprint("%s!%s!%s", net, *lsys, *lserv);
		if(*lsys == nil || *lserv == nil || *laddr == nil)
			return -1;
		return 0;
	case AF_UNIX:
		sun = (void*)sa;
		*lsys = unknown;
		*lserv = unknown;
		*laddr = smprint("unix!%s", sun->sun_path);
		if(*laddr == nil)
			return -1;
		return 0;
	default:
		werrstr("unknown socket family");
		return -1;
	}
}

NetConnInfo*
getnetconninfo(char *dir, int fd)
{
	socklen_t sn;
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
		struct sockaddr_un sun;
	} u;
	NetConnInfo *nci;

	if(dir){
		if((fd = _p9netfd(dir)) < 0){
			werrstr("no such network connection %s", dir);
			return nil;
		}
	}

	nci = mallocz(sizeof *nci, 1);
	if(nci == nil)
		goto err;
	nci->dir = smprint("/dev/fd/%d", fd);
	nci->root = strdup("/net");
	nci->spec = unknown;
	if(nci->dir == nil || nci->root == nil)
		goto err;
	sn = sizeof u;
	if(getsockname(fd, &u.sa, &sn) < 0)
		goto err;
	if(convert(fd, &u.sa, &nci->lsys, &nci->lserv, &nci->laddr) < 0)
		goto err;
	sn = sizeof u;
	if(getpeername(fd, &u.sa, &sn) < 0)
		goto err;
	if(convert(fd, &u.sa, &nci->rsys, &nci->rserv, &nci->raddr) < 0)
		goto err;
	return nci;

err:
	freenetconninfo(nci);
	return nil;
}

static void
xfree(void *v)
{
	if(v != nil && v != unknown)
		free(v);
}

void
freenetconninfo(NetConnInfo *nci)
{
	if(nci == nil)
		return;
	xfree(nci->dir);
	xfree(nci->root);
	xfree(nci->spec);
	xfree(nci->lsys);
	xfree(nci->lserv);
	xfree(nci->rsys);
	xfree(nci->rserv);
	xfree(nci->laddr);
	xfree(nci->raddr);
	free(nci);
}

