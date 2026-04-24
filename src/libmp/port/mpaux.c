#include "os.h"
#include <mp.h>
#include "dat.h"

static mpdigit _mptwodata[1] = { 2 };
static mpint _mptwo =
{
	1, 1, 1,
	_mptwodata,
	MPstatic|MPnorm
};
mpint *mptwo = &_mptwo;

static mpdigit _mponedata[1] = { 1 };
static mpint _mpone =
{
	1, 1, 1,
	_mponedata,
	MPstatic|MPnorm
};
mpint *mpone = &_mpone;

static mpdigit _mpzerodata[1] = { 0 };
static mpint _mpzero =
{
	1, 1, 0,
	_mpzerodata,
	MPstatic|MPnorm
};
mpint *mpzero = &_mpzero;

static int mpmindigits = 33;

// set minimum digit allocation
void
mpsetminbits(int n)
{
	if(n < 0)
		sysfatal("mpsetminbits: n < 0");
	if(n == 0)
		n = 1;
	mpmindigits = DIGITS(n);
}

// allocate an n bit 0'd number
mpint*
mpnew(int n)
{
	mpint *b;

	if(n < 0)
		sysfatal("mpsetminbits: n < 0");

	n = DIGITS(n);
	if(n < mpmindigits)
		n = mpmindigits;
	b = mallocz(sizeof(mpint) + n*Dbytes, 1);
	if(b == nil)
		sysfatal("mpnew: %r");
	setmalloctag(b, getcallerpc(&n));
	b->p = (mpdigit*)&b[1];
	b->size = n;
	b->sign = 1;
	b->flags = MPnorm;

	return b;
}

// guarantee at least n significant bits
void
mpbits(mpint *b, int m)
{
	int n;

	n = DIGITS(m);
	if(b->size >= n){
		if(b->top >= n)
			return;
	} else {
		if(b->p == (mpdigit*)&b[1]){
			b->p = (mpdigit*)mallocz(n*Dbytes, 0);
			if(b->p == nil)
				sysfatal("mpbits: %r");
			memmove(b->p, &b[1], Dbytes*b->top);
			memset(&b[1], 0, Dbytes*b->size);
		} else {
			b->p = (mpdigit*)realloc(b->p, n*Dbytes);
			if(b->p == nil)
				sysfatal("mpbits: %r");
		}
		b->size = n;
	}
	memset(&b->p[b->top], 0, Dbytes*(n - b->top));
	b->top = n;
	b->flags &= ~MPnorm;
}

void
mpfree(mpint *b)
{
	if(b == nil)
		return;
	if(b->flags & MPstatic)
		sysfatal("freeing mp constant");
	memset(b->p, 0, b->size*Dbytes);
	if(b->p != (mpdigit*)&b[1])
		free(b->p);
	free(b);
}

mpint*
mpnorm(mpint *b)
{
	int i;

	if(b->flags & MPtimesafe){
		assert(b->sign == 1);
		b->flags &= ~MPnorm;
		return b;
	}
	for(i = b->top-1; i >= 0; i--)
		if(b->p[i] != 0)
			break;
	b->top = i+1;
	if(b->top == 0)
		b->sign = 1;
	b->flags |= MPnorm;
	return b;
}

mpint*
mpcopy(mpint *old)
{
	mpint *new;

	new = mpnew(Dbits*old->size);
	setmalloctag(new, getcallerpc(&old));
	new->sign = old->sign;
	new->top = old->top;
	new->flags = old->flags & ~(MPstatic|MPfield);
	memmove(new->p, old->p, Dbytes*old->top);
	return new;
}

void
mpassign(mpint *old, mpint *new)
{
	if(new == nil || old == new)
		return;
	new->top = 0;
	mpbits(new, Dbits*old->top);
	new->sign = old->sign;
	new->top = old->top;
	new->flags &= ~MPnorm;
	new->flags |= old->flags & ~(MPstatic|MPfield);
	memmove(new->p, old->p, Dbytes*old->top);
}

// number of significant bits in mantissa
int
mpsignif(mpint *n)
{
	int i, j;
	mpdigit d;

	if(n->top == 0)
		return 0;
	for(i = n->top-1; i >= 0; i--){
		d = n->p[i];
		for(j = Dbits-1; j >= 0; j--){
			if(d & (((mpdigit)1)<<j))
				return i*Dbits + j + 1;
		}
	}
	return 0;
}

// k, where n = 2**k * q for odd q
int
mplowbits0(mpint *n)
{
	int k, bit, digit;
	mpdigit d;

	assert(n->flags & MPnorm);
	if(n->top==0)
		return 0;
	k = 0;
	bit = 0;
	digit = 0;
	d = n->p[0];
	for(;;){
		if(d & (1<<bit))
			break;
		k++;
		bit++;
		if(bit==Dbits){
			if(++digit >= n->top)
				return 0;
			d = n->p[digit];
			bit = 0;
		}
	}
	return k;
}
