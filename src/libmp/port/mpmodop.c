#include "os.h"
#include <mp.h>

/* operands need to have m->top+1 digits of space and satisfy 0 ≤ a ≤ m-1 */
static mpint*
modarg(mpint *a, mpint *m)
{
	if(a->size <= m->top || a->sign < 0 || mpmagcmp(a, m) >= 0){
		a = mpcopy(a);
		mpmod(a, m, a);
		mpbits(a, Dbits*(m->top+1));
		a->top = m->top;
	} else if(a->top < m->top){
		memset(&a->p[a->top], 0, (m->top - a->top)*Dbytes);
	}
	return a;
}

void
mpmodadd(mpint *b1, mpint *b2, mpint *m, mpint *sum)
{
	mpint *a, *b;
	mpdigit d;
	int i, j;

	a = modarg(b1, m);
	b = modarg(b2, m);

	sum->flags |= (a->flags | b->flags) & MPtimesafe;
	mpbits(sum, Dbits*2*(m->top+1));

	mpvecadd(a->p, m->top, b->p, m->top, sum->p);
	mpvecsub(sum->p, m->top+1, m->p, m->top, sum->p+m->top+1);

	d = sum->p[2*m->top+1];
	for(i = 0, j = m->top+1; i < m->top; i++, j++)
		sum->p[i] = (sum->p[i] & d) | (sum->p[j] & ~d);

	sum->top = m->top;
	sum->sign = 1;
	mpnorm(sum);

	if(a != b1)
		mpfree(a);
	if(b != b2)
		mpfree(b);
}

void
mpmodsub(mpint *b1, mpint *b2, mpint *m, mpint *diff)
{
	mpint *a, *b;
	mpdigit d;
	int i, j;

	a = modarg(b1, m);
	b = modarg(b2, m);

	diff->flags |= (a->flags | b->flags) & MPtimesafe;
	mpbits(diff, Dbits*2*(m->top+1));

	a->p[m->top] = 0;
	mpvecsub(a->p, m->top+1, b->p, m->top, diff->p);
	mpvecadd(diff->p, m->top, m->p, m->top, diff->p+m->top+1);

	d = ~diff->p[m->top];
	for(i = 0, j = m->top+1; i < m->top; i++, j++)
		diff->p[i] = (diff->p[i] & d) | (diff->p[j] & ~d);

	diff->top = m->top;
	diff->sign = 1;
	mpnorm(diff);

	if(a != b1)
		mpfree(a);
	if(b != b2)
		mpfree(b);
}

void
mpmodmul(mpint *b1, mpint *b2, mpint *m, mpint *prod)
{
	mpint *a, *b;

	a = modarg(b1, m);
	b = modarg(b2, m);

	mpmul(a, b, prod);
	mpmod(prod, m, prod);

	if(a != b1)
		mpfree(a);
	if(b != b2)
		mpfree(b);
}
