#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include <thread.h>
#include "9p.h"

void
readbuf(Req *r, void *s, long n)
{
	r->ofcall.count = r->ifcall.count;
	if(r->ifcall.offset >= n){
		r->ofcall.count = 0;
		return;
	}
	if(r->ifcall.offset+r->ofcall.count > n)
		r->ofcall.count = n - r->ifcall.offset;
	memmove(r->ofcall.data, (char*)s+r->ifcall.offset, r->ofcall.count);
}

void
readstr(Req *r, char *s)
{
	readbuf(r, s, strlen(s));
}
