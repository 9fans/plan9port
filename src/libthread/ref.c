#include "u.h"
#include "libc.h"
#include "thread.h"

static long
refadd(Ref *r, long a)
{
	long ref;

	lock(&r->lock);
	r->ref += a;
	ref = r->ref;
	unlock(&r->lock);
	return ref;
}

long
incref(Ref *r)
{
	return refadd(r, 1);
}

long
decref(Ref *r)
{
	return refadd(r, -1);
}
