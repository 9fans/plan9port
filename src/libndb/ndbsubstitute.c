#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ndb.h>

/* replace a in t with b, the line structure in b is lost, c'est la vie */
Ndbtuple*
ndbsubstitute(Ndbtuple *t, Ndbtuple *a, Ndbtuple *b)
{
	Ndbtuple *nt;

	if(a == b)
		return t;
	if(b == nil)
		return ndbdiscard(t, a);

	/* all pointers to a become pointers to b */
	for(nt = t; nt != nil; nt = nt->entry){
		if(nt->line == a)
			nt->line = b;
		if(nt->entry == a)
			nt->entry = b;
	}

	/* end of b chain points to a's successors */
	for(nt = b; nt->entry; nt = nt->entry){
		nt->line = nt->entry;
	}
	nt->line = a->line;
	nt->entry = a->entry;

	a->entry = nil;
	ndbfree(a);

	if(a == t)
		return b;
	else
		return t;
}
