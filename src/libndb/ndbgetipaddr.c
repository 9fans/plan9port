#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ndb.h>
#include <ip.h>

/* return list of ip addresses for a name */
Ndbtuple*
ndbgetipaddr(Ndb *db, char *val)
{
	char *attr, *p;
	Ndbtuple *it, *first, *last, *next;
	Ndbs s;

	/* already an IP address? */
	attr = ipattr(val);
	if(strcmp(attr, "ip") == 0){
		it = ndbnew("ip", val);
		return it;
	}

	/* look it up */
	p = ndbgetvalue(db, &s, attr, val, "ip", &it);
	if(p == nil)
		return nil;
	free(p);

	/* remove the non-ip entries */
	first = last = nil;
	for(; it; it = next){
		next = it->entry;
		if(strcmp(it->attr, "ip") == 0){
			if(first == nil)
				first = it;
			else
				last->entry = it;
			it->entry = nil;
			it->line = first;
			last = it;
		} else {
			it->entry = nil;
			ndbfree(it);
		}
	}

	return first;
}
