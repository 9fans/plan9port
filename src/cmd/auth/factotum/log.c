#include "std.h"
#include "dat.h"

void
lbkick(Logbuf *lb)
{
	char *s;
	int n;
	Req *r;

	while(lb->wait && lb->rp != lb->wp){
		r = lb->wait;
		lb->wait = r->aux;
		if(lb->wait == nil)
			lb->waitlast = &lb->wait;
		r->aux = nil;
		if(r->ifcall.count < 5){
			respond(r, "factotum: read request count too short");
			continue;
		}
		s = lb->msg[lb->rp];
		lb->msg[lb->rp] = nil;
		if(++lb->rp == nelem(lb->msg))
			lb->rp = 0;
		n = r->ifcall.count;
		if(n < strlen(s)+1+1){
			memmove(r->ofcall.data, s, n-5);
			n -= 5;
			r->ofcall.data[n] = '\0';
			/* look for first byte of UTF-8 sequence by skipping continuation bytes */
			while(n>0 && (r->ofcall.data[--n]&0xC0)==0x80)
				;
			strcpy(r->ofcall.data+n, "...\n");
		}else{
			strcpy(r->ofcall.data, s);
			strcat(r->ofcall.data, "\n");
		}
		r->ofcall.count = strlen(r->ofcall.data);
		free(s);
		respond(r, nil);
	}
}

void
lbread(Logbuf *lb, Req *r)
{
	if(lb->waitlast == nil)
		lb->waitlast = &lb->wait;
	*lb->waitlast = r;
	lb->waitlast = (Req**)(void*)&r->aux;
	r->aux = nil;
	lbkick(lb);
}

void
lbflush(Logbuf *lb, Req *r)
{
	Req **l;

	for(l=&lb->wait; *l; l=(Req**)(void*)&(*l)->aux){
		if(*l == r){
			*l = r->aux;
			r->aux = nil;
			if(*l == nil)
				lb->waitlast = l;
			closereq(r);
			break;
		}
	}
}

void
lbappend(Logbuf *lb, char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	lbvappend(lb, fmt, arg);
	va_end(arg);
}

void
lbvappend(Logbuf *lb, char *fmt, va_list arg)
{
	char *s;

	s = vsmprint(fmt, arg);
	if(s == nil)
		sysfatal("out of memory");
	if(lb->msg[lb->wp])
		free(lb->msg[lb->wp]);
	lb->msg[lb->wp] = s;
	if(++lb->wp == nelem(lb->msg))
		lb->wp = 0;
	lbkick(lb);
}

Logbuf logbuf;

void
logread(Req *r)
{
	lbread(&logbuf, r);
}

void
logflush(Req *r)
{
	lbflush(&logbuf, r);
}

void
flog(char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	lbvappend(&logbuf, fmt, arg);
	va_end(arg);
}
