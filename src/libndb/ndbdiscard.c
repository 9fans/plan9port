#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ndb.h>

/* remove a from t and free it */
Ndbtuple*
ndbdiscard(Ndbtuple *t, Ndbtuple *a)
{
	Ndbtuple *nt;

	/* unchain a */
	for(nt = t; nt != nil; nt = nt->entry){
		if(nt->line == a)
			nt->line = a->line;
		if(nt->entry == a)
			nt->entry = a->entry;
	}

	/* a may be start of chain */
	if(t == a)
		t = a->entry;

	/* free a */
	a->entry = nil;
	ndbfree(a);

	return t;
}
