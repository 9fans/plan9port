#include "os.h"
#include <mp.h>
#include "dat.h"

// res = s != 0 ? b1 : b2
void
mpsel(int s, mpint *b1, mpint *b2, mpint *res)
{
	mpdigit d;
	int n, m, i;

	res->flags |= (b1->flags | b2->flags) & MPtimesafe;
	if((res->flags & MPtimesafe) == 0){
		mpassign(s ? b1 : b2, res);
		return;
	}
	res->flags &= ~MPnorm;

	n = b1->top;
	m = b2->top;
	mpbits(res, Dbits*(n >= m ? n : m));
	res->top = n >= m ? n : m;

	s = ((-s^s)|s)>>(sizeof(s)*8-1);
	res->sign = (b1->sign & s) | (b2->sign & ~s);

	d = -((mpdigit)s & 1);

	i = 0;
	while(i < n && i < m){
		res->p[i] = (b1->p[i] & d) | (b2->p[i] & ~d);
		i++;
	}
	while(i < n){
		res->p[i] = b1->p[i] & d;
		i++;
	}
	while(i < m){
		res->p[i] = b2->p[i] & ~d;
		i++;
	}
}
