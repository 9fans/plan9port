#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include <ndb.h>

Ndb*
ndbcat(Ndb *a, Ndb *b)
{
	Ndb *db = a;

	if(a == nil)
		return b;
	while(a->next != nil)
		a = a->next;
	a->next = b;
	return db;
}
