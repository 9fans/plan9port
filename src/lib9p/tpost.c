#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include "post.h"

void
threadpostmountsrv(Srv *s, char *name, char *mtpt, int flag)
{
	Postcrud *p;

	p = _post1(s, name, mtpt, flag);
	if(procrfork(_post2, s, 32*1024, RFNAMEG|RFNOTEG) < 0)
		sysfatal("procrfork: %r");
	_post3(p);
}

