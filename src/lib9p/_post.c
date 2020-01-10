#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include "post.h"

Postcrud*
_post1(Srv *s, char *name, char *mtpt, int flag)
{
	Postcrud *p;

	p = emalloc9p(sizeof *p);
	if(!s->nopipe){
		if(pipe(p->fd) < 0)
			sysfatal("pipe: %r");
		s->infd = s->outfd = p->fd[1];
		s->srvfd = p->fd[0];
	}
	if(name)
		if(postfd(name, s->srvfd) < 0)
			sysfatal("postfd %s: %r", name);
	p->s = s;
	p->mtpt = mtpt;
	p->flag = flag;
	return p;
}

void
_post2(void *v)
{
	Srv *s;

	s = v;
	if(!s->leavefdsopen){
		rfork(RFNOTEG);
		rendezvous((ulong)s, 0);
		close(s->srvfd);
	}
	srv(s);
}

void
_post3(Postcrud *p)
{
	/*
	 * Normally the server is posting as the last thing it does
	 * before exiting, so the correct thing to do is drop into
	 * a different fd space and close the 9P server half of the
	 * pipe before trying to mount the kernel half.  This way,
	 * if the file server dies, we don't have a ref to the 9P server
	 * half of the pipe.  Then killing the other procs will drop
	 * all the refs on the 9P server half, and the mount will fail.
	 * Otherwise the mount hangs forever.
	 *
	 * Libthread in general and acme win in particular make
	 * it hard to make this fd bookkeeping work out properly,
	 * so leaveinfdopen is a flag that win sets to opt out of this
	 * safety net.
	 */
	if(!p->s->leavefdsopen){
		rfork(RFFDG);
		rendezvous((ulong)p->s, 0);
		close(p->s->infd);
		if(p->s->infd != p->s->outfd)
			close(p->s->outfd);
	}

#if 0
	if(p->mtpt){
		if(amount(p->s->srvfd, p->mtpt, p->flag, "") == -1)
			sysfatal("mount %s: %r", p->mtpt);
	}else
#endif
		close(p->s->srvfd);
	free(p);
}
