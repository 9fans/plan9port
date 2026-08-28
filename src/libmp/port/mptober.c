#include "os.h"
#include <mp.h>
#include "dat.h"

void
mptober(mpint *b, uchar *p, int n)
{
	int i, j, m;
	mpdigit x;

	memset(p, 0, n);

	p += n;
	m = b->top*Dbytes;
	if(m < n)
		n = m;

	i = 0;
	while(n >= Dbytes){
		n -= Dbytes;
		x = b->p[i++];
		for(j = 0; j < Dbytes; j++){
			*--p = x;
			x >>= 8;
		}
	}
	if(n > 0){
		x = b->p[i];
		for(j = 0; j < n; j++){
			*--p = x;
			x >>= 8;
		}
	}
}
