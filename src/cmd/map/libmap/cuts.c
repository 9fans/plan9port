#include <u.h>
#include <libc.h>
#include "map.h"
extern void abort(void);

/* these routines duplicate names found in map.c.  they are
called from routines in hex.c, guyou.c, and tetra.c, which
are in turn invoked directly from map.c.  this bad organization
arises from data hiding; only these three files know stuff
that's necessary for the proper handling of the unusual cuts
involved in these projections.

the calling routines are not advertised as part of the library,
and the library duplicates should never get loaded, however they
are included to make the libary self-standing.*/

int
picut(struct place *g, struct place *og, double *cutlon)
{
	g; og; cutlon;
	abort();
	return 0;
}

int
ckcut(struct place *g1, struct place *g2, double lon)
{
	g1; g2; lon;
	abort();
	return 0;
}

double
reduce(double x)
{
	x;
	abort();
	return 0;
}
