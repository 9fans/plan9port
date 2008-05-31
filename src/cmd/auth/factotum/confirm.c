#include "std.h"
#include "dat.h"

Logbuf confbuf;

void
confirmread(Req *r)
{
	lbread(&confbuf, r);
}

void
confirmflush(Req *r)
{
	lbflush(&confbuf, r);
}

int
confirmwrite(char *s)
{
	char *t, *ans;
	int allow;
	ulong tag;
	Attr *a;
	Conv *c;

	a = _parseattr(s);
	if(a == nil){
		werrstr("bad attr");
		return -1;
	}
	if((t = _strfindattr(a, "tag")) == nil){
		flog("bad confirm write: no tag");
		werrstr("no tag");
		return -1;
	}
	tag = strtoul(t, 0, 0);
	if((ans = _strfindattr(a, "answer")) == nil){
		flog("bad confirm write: no answer");
		werrstr("no answer");
		return -1;
	}
	if(strcmp(ans, "yes") == 0)
		allow = 1;
	else if(strcmp(ans, "no") == 0)
		allow = 0;
	else{
		flog("bad confirm write: bad answer");
		werrstr("bad answer");
		return -1;
	}
	for(c=conv; c; c=c->next){
		if(tag == c->tag){
			nbsendul(c->keywait, allow);
			break;
		}
	}
	if(c == nil){
		werrstr("tag not found");
		return -1;
	}
	return 0;
}

int
confirmkey(Conv *c, Key *k)
{
	int ret;

	if(*confirminuse == 0)
		return -1;

	lbappend(&confbuf, "confirm tag=%lud %A %N", c->tag, k->attr, k->privattr);
	flog("confirm %A %N", k->attr, k->privattr);
	c->state = "keyconfirm";
	ret = recvul(c->keywait);
	flog("confirm=%d %A %N", ret, k->attr, k->privattr);
	return ret;
}

Logbuf needkeybuf;

void
needkeyread(Req *r)
{
	lbread(&needkeybuf, r);
}

void
needkeyflush(Req *r)
{
	lbflush(&needkeybuf, r);
}

int
needkeywrite(char *s)
{
	char *t;
	ulong tag;
	Attr *a;
	Conv *c;

	a = _parseattr(s);
	if(a == nil){
		werrstr("empty write");
		return -1;
	}
	if((t = _strfindattr(a, "tag")) == nil){
		werrstr("no tag");
		freeattr(a);
		return -1;
	}
	tag = strtoul(t, 0, 0);
	for(c=conv; c; c=c->next)
		if(c->tag == tag){
			nbsendul(c->keywait, 0);
			break;
		}
	if(c == nil){
		werrstr("tag not found");
		freeattr(a);
		return -1;
	}
	freeattr(a);
	return 0;
}

int
needkey(Conv *c, Attr *a)
{
	if(c == nil || *needkeyinuse == 0)
		return -1;

	lbappend(&needkeybuf, "needkey tag=%lud %A", c->tag, a);
	flog("needkey %A", a);
	return nbrecvul(c->keywait);
}

int
badkey(Conv *c, Key *k, char *msg, Attr *a)
{
	if(c == nil || *needkeyinuse == 0)
		return -1;

	lbappend(&needkeybuf, "badkey tag=%lud %A %N\n%s\n%A",
		c->tag, k->attr, k->privattr, msg, a);
	flog("badkey %A / %N / %s / %A",
		k->attr, k->privattr, msg, a);
	return nbrecvul(c->keywait);
}
