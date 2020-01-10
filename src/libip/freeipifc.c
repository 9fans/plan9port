#include <u.h>
#include <libc.h>
#include <ip.h>

void
freeipifc(Ipifc *i)
{
	Ipifc *next;
	Iplifc *l, *lnext;

	for(; i; i=next){
		next = i->next;
		for(l=i->lifc; l; l=lnext){
			lnext = l->next;
			free(l);
		}
		free(i);
	}
}
