#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

static void
launchsrv(void *v)
{
	srv(v);
}

void
threadpostmountsrv(Srv *s, char *name, char *mtpt, int flag)
{
	int fd[2];

	if(mtpt)
		sysfatal("mount not supported");

	if(!s->nopipe){
		if(pipe(fd) < 0)
			sysfatal("pipe: %r");
		s->infd = s->outfd = fd[1];
		s->srvfd = fd[0];
	}
	if(name && post9pservice(s->srvfd, name) < 0)
		sysfatal("post9pservice %s: %r", name);
	proccreate(launchsrv, s, 32*1024);
}
