#include "stdinc.h"

#include "9.h"

typedef struct Lstn Lstn;
struct Lstn {
	int	afd;
	int	flags;
	char*	address;
	char	dir[NETPATHLEN];

	Lstn*	next;
	Lstn*	prev;
};

static struct {
	VtLock*	lock;

	Lstn*	head;
	Lstn*	tail;
} lbox;

static void
lstnFree(Lstn* lstn)
{
	vtLock(lbox.lock);
	if(lstn->prev != nil)
		lstn->prev->next = lstn->next;
	else
		lbox.head = lstn->next;
	if(lstn->next != nil)
		lstn->next->prev = lstn->prev;
	else
		lbox.tail = lstn->prev;
	vtUnlock(lbox.lock);

	if(lstn->afd != -1)
		close(lstn->afd);
	vtMemFree(lstn->address);
	vtMemFree(lstn);
}

static void
lstnListen(void* a)
{
	Lstn *lstn;
	int dfd, lfd;
	char newdir[NETPATHLEN];
	
 	vtThreadSetName("listen");

	lstn = a;
	for(;;){
		if((lfd = listen(lstn->dir, newdir)) < 0){
			fprint(2, "listen: listen '%s': %r", lstn->dir);
			break;
		}
		if((dfd = accept(lfd, newdir)) >= 0)
			conAlloc(dfd, newdir, lstn->flags);
		else
			fprint(2, "listen: accept %s: %r\n", newdir);
		close(lfd);
	}
	lstnFree(lstn);
}

static Lstn*
lstnAlloc(char* address, int flags)
{
	int afd;
	Lstn *lstn;
	char dir[NETPATHLEN];

	vtLock(lbox.lock);
	for(lstn = lbox.head; lstn != nil; lstn = lstn->next){
		if(strcmp(lstn->address, address) != 0)
			continue;
		vtSetError("listen: already serving '%s'", address);
		vtUnlock(lbox.lock);
		return nil;
	}

	if((afd = announce(address, dir)) < 0){
		vtSetError("listen: announce '%s': %r", address);
		vtUnlock(lbox.lock);
		return nil;
	}

	lstn = vtMemAllocZ(sizeof(Lstn));
	lstn->afd = afd;
	lstn->address = vtStrDup(address);
	lstn->flags = flags;
	memmove(lstn->dir, dir, NETPATHLEN);

	if(lbox.tail != nil){
		lstn->prev = lbox.tail;
		lbox.tail->next = lstn;
	}
	else{
		lbox.head = lstn;
		lstn->prev = nil;
	}
	lbox.tail = lstn;
	vtUnlock(lbox.lock);

	if(vtThread(lstnListen, lstn) < 0){
		vtSetError("listen: thread '%s': %r", lstn->address);
		lstnFree(lstn);
		return nil;
	}

	return lstn;
}

static int
cmdLstn(int argc, char* argv[])
{
	int dflag, flags;
	Lstn *lstn;
	char *usage = "usage: listen [-dIN] [address]";

	dflag = 0;
	flags = 0;
	ARGBEGIN{
	default:
		return cliError(usage);
	case 'd':
		dflag = 1;
		break;
	case 'I':
		flags |= ConIPCheck;
		break;
	case 'N':
		flags |= ConNoneAllow;
		break;
	}ARGEND

	switch(argc){
	default:
		return cliError(usage);
	case 0:
		vtRLock(lbox.lock);
		for(lstn = lbox.head; lstn != nil; lstn = lstn->next)
			consPrint("\t%s\t%s\n", lstn->address, lstn->dir);
		vtRUnlock(lbox.lock);
		break;
	case 1:
		if(!dflag){
			if(lstnAlloc(argv[0], flags) == nil)
				return 0;
			break;
		}

		vtLock(lbox.lock);
		for(lstn = lbox.head; lstn != nil; lstn = lstn->next){
			if(strcmp(lstn->address, argv[0]) != 0)
				continue;
			if(lstn->afd != -1){
				close(lstn->afd);
				lstn->afd = -1;
			}
			break;
		}
		vtUnlock(lbox.lock);

		if(lstn == nil){
			vtSetError("listen: '%s' not found", argv[0]);
			return 0;
		}
		break;
	}

	return 1;
}

int
lstnInit(void)
{
	lbox.lock = vtLockAlloc();

	cliAddCmd("listen", cmdLstn);

	return 1;
}
