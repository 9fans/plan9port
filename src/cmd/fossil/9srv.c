#include "stdinc.h"

#include "9.h"

typedef struct Srv Srv;
struct Srv {
	int	fd;
	int	srvfd;
	char*	service;
	char*	mntpnt;

	Srv*	next;
	Srv*	prev;
};

static struct {
	VtLock*	lock;

	Srv*	head;
	Srv*	tail;
} sbox;

static int
srvFd(char* name, int mode, int fd, char** mntpnt)
{
	int n, srvfd;
	char *p, buf[10];

	/*
	 * Drop a file descriptor with given name and mode into /srv.
	 * Create with ORCLOSE and don't close srvfd so it will be removed
	 * automatically on process exit.
	 */
	p = smprint("/srv/%s", name);
	if((srvfd = create(p, ORCLOSE|OWRITE, mode)) < 0){
		vtMemFree(p);
		p = smprint("#s/%s", name);
		if((srvfd = create(p, ORCLOSE|OWRITE, mode)) < 0){
			vtSetError("create %s: %r", p);
			vtMemFree(p);
			return -1;
		}
	}

	n = snprint(buf, sizeof(buf), "%d", fd);
	if(write(srvfd, buf, n) < 0){
		close(srvfd);
		vtSetError("write %s: %r", p);
		vtMemFree(p);
		return -1;
	}

	*mntpnt = p;

	return srvfd;
}

static void
srvFree(Srv* srv)
{
	if(srv->prev != nil)
		srv->prev->next = srv->next;
	else
		sbox.head = srv->next;
	if(srv->next != nil)
		srv->next->prev = srv->prev;
	else
		sbox.tail = srv->prev;

	if(srv->srvfd != -1)
		close(srv->srvfd);
	vtMemFree(srv->service);
	vtMemFree(srv->mntpnt);
	vtMemFree(srv);
}

static Srv*
srvAlloc(char* service, int mode, int fd)
{
	Dir *dir;
	Srv *srv;
	int srvfd;
	char *mntpnt;

	vtLock(sbox.lock);
	for(srv = sbox.head; srv != nil; srv = srv->next){
		if(strcmp(srv->service, service) != 0)
			continue;
		/*
		 * If the service exists, but is stale,
		 * free it up and let the name be reused.
		 */
		if((dir = dirfstat(srv->srvfd)) != nil){
			free(dir);
			vtSetError("srv: already serving '%s'", service);
			vtUnlock(sbox.lock);
			return nil;
		}
		srvFree(srv);
		break;
	}

	if((srvfd = srvFd(service, mode, fd, &mntpnt)) < 0){
		vtUnlock(sbox.lock);
		return nil;
	}
	close(fd);

	srv = vtMemAllocZ(sizeof(Srv));
	srv->srvfd = srvfd;
	srv->service = vtStrDup(service);
	srv->mntpnt = mntpnt;

	if(sbox.tail != nil){
		srv->prev = sbox.tail;
		sbox.tail->next = srv;
	}
	else{
		sbox.head = srv;
		srv->prev = nil;
	}
	sbox.tail = srv;
	vtUnlock(sbox.lock);

	return srv;
}

static int
cmdSrv(int argc, char* argv[])
{
	Con *con;
	Srv *srv;
	char *usage = "usage: srv [-APWdp] [service]";
	int conflags, dflag, fd[2], mode, pflag, r;

	dflag = 0;
	pflag = 0;
	conflags = 0;
	mode = 0666;

	ARGBEGIN{
	default:
		return cliError(usage);
	case 'A':
		conflags |= ConNoAuthCheck;
		break;
	case 'I':
		conflags |= ConIPCheck;
		break;
	case 'N':
		conflags |= ConNoneAllow;
		break;
	case 'P':
		conflags |= ConNoPermCheck;
		mode = 0600;
		break;
	case 'W':
		conflags |= ConWstatAllow;
		mode = 0600;
		break;
	case 'd':
		dflag = 1;
		break;
	case 'p':
		pflag = 1;
		mode = 0600;
		break;
	}ARGEND

	if(pflag && (conflags&ConNoPermCheck)){
		vtSetError("srv: cannot use -P with -p");
		return 0;
	}

	switch(argc){
	default:
		return cliError(usage);
	case 0:
		vtRLock(sbox.lock);
		for(srv = sbox.head; srv != nil; srv = srv->next)
			consPrint("\t%s\t%d\n", srv->service, srv->srvfd);
		vtRUnlock(sbox.lock);

		return 1;
	case 1:
		if(!dflag)
			break;

		vtLock(sbox.lock);
		for(srv = sbox.head; srv != nil; srv = srv->next){
			if(strcmp(srv->service, argv[0]) != 0)
				continue;
			srvFree(srv);
			break;
		}
		vtUnlock(sbox.lock);

		if(srv == nil){
			vtSetError("srv: '%s' not found", argv[0]);
			return 0;
		}

		return 1;
	}

	if(pipe(fd) < 0){
		vtSetError("srv pipe: %r");
		return 0;
	}
	if((srv = srvAlloc(argv[0], mode, fd[0])) == nil){
		close(fd[0]); close(fd[1]);
		return 0;
	}

	if(pflag)
		r = consOpen(fd[1], srv->srvfd, -1);
	else{
		con = conAlloc(fd[1], srv->mntpnt, conflags);
		if(con == nil)
			r = 0;
		else
			r = 1;
	}
	if(r == 0){
		close(fd[1]);
		vtLock(sbox.lock);
		srvFree(srv);
		vtUnlock(sbox.lock);
	}

	return r;
}

int
srvInit(void)
{
	sbox.lock = vtLockAlloc();

	cliAddCmd("srv", cmdSrv);

	return 1;
}
