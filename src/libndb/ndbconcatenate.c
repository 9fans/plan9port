#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ndb.h>

/* concatenate two tuples */
Ndbtuple*
ndbconcatenate(Ndbtuple *a, Ndbtuple *b)
{
	Ndbtuple *t;

	if(a == nil)
		return b;
	for(t = a; t->entry; t = t->entry)
		;
	t->entry = b;
	return a;
}
