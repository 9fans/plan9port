/*
 * Atomic reference counts - used by applications.
 *
 * We use locks to avoid the assembly of the Plan 9 versions.
 */

#include "threadimpl.h"

void
incref(Ref *r)
{
	lock(&r->lk);
	r->ref++;
	unlock(&r->lk);
}

long
decref(Ref *r)
{
	long n;

	lock(&r->lk);
	n = --r->ref;
	unlock(&r->lk);
	return n;
}
