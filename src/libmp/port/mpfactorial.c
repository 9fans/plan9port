#include "os.h"
#include <mp.h>
#include <libsec.h>
#include "dat.h"

mpint*
mpfactorial(ulong n)
{
	int i;
	ulong k;
	unsigned cnt;
	int max, mmax;
	mpdigit p, pp[2];
	mpint *r, *s, *stk[31];

	cnt = 0;
	max = mmax = -1;
	p = 1;
	r = mpnew(0);
	for(k=2; k<=n; k++){
		pp[0] = 0;
		pp[1] = 0;
		mpvecdigmuladd(&p, 1, (mpdigit)k, pp);
		if(pp[1] == 0)	/* !overflow */
			p = pp[0];
		else{
			cnt++;
			if((cnt & 1) == 0){
				s = stk[max];
				mpbits(r, Dbits*(s->top+1+1));
				memset(r->p, 0, Dbytes*(s->top+1+1));
				mpvecmul(s->p, s->top, &p, 1, r->p);
				r->sign = 1;
				r->top = s->top+1+1;		/* XXX: norm */
				mpassign(r, s);
				for(i=4; (cnt & (i-1)) == 0; i=i<<1){
					mpmul(stk[max], stk[max-1], r);
					mpassign(r, stk[max-1]);
					max--;
				}
			}else{
				max++;
				if(max > mmax){
					mmax++;
					if(max > nelem(stk))
						abort();
					stk[max] = mpnew(Dbits);
				}
				stk[max]->top = 1;
				stk[max]->p[0] = p;
			}
			p = (mpdigit)k;
		}
	}
	if(max < 0){
		mpbits(r, Dbits);
		r->top = 1;
		r->sign = 1;
		r->p[0] = p;
	}else{
		s = stk[max--];
		mpbits(r, Dbits*(s->top+1+1));
		memset(r->p, 0, Dbytes*(s->top+1+1));
		mpvecmul(s->p, s->top, &p, 1, r->p);
		r->sign = 1;
		r->top = s->top+1+1;		/* XXX: norm */
	}

	while(max >= 0)
		mpmul(r, stk[max--], r);
	for(max=mmax; max>=0; max--)
		mpfree(stk[max]);
	mpnorm(r);
	return r;
}
