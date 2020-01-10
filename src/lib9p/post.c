#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include "post.h"

void
postmountsrv(Srv *s, char *name, char *mtpt, int flag)
{
	Postcrud *p;

	p = _post1(s, name, mtpt, flag);
	switch(rfork(RFPROC|RFNOTEG|RFNAMEG|RFMEM)){
	case -1:
		sysfatal("rfork: %r");
	case 0:
		_post2(s);
		exits(nil);
	default:
		_post3(p);
	}
}
