#ifndef __MP_H__
#define __MP_H__ 1
#ifdef __cplusplus
extern "C" {
#endif

AUTOLIB(mp)

/*
#pragma	src	"/sys/src/libmp"
#pragma	lib	"libmp.a"
*/

#define _MPINT 1

typedef uint mpdigit;

/* the code assumes mpdigit to be at least an int */
/* mpdigit must be an atomic type.  mpdigit is defined */
/* in the architecture specific u.h */

typedef struct mpint mpint;

struct mpint
{
	int	sign;	/* +1 or -1 */
	int	size;	/* allocated digits */
	int	top;	/* significant digits */
	mpdigit	*p;
	char	flags;
};

enum
{
	MPstatic=	0x01,
	Dbytes=		sizeof(mpdigit),	/* bytes per digit */
	Dbits=		Dbytes*8		/* bits per digit */
};

/* allocation */
void	mpsetminbits(int n);	/* newly created mpint's get at least n bits */
mpint*	mpnew(int n);		/* create a new mpint with at least n bits */
void	mpfree(mpint *b);
void	mpbits(mpint *b, int n);	/* ensure that b has at least n bits */
void	mpnorm(mpint *b);		/* dump leading zeros */
mpint*	mpcopy(mpint *b);
void	mpassign(mpint *old, mpint *new);

/* random bits */
mpint*	mprand(int bits, void (*gen)(uchar*, int), mpint *b);

/* conversion */
mpint*	strtomp(char*, char**, int, mpint*);	/* ascii */
int	mpfmt(Fmt*);
char*	mptoa(mpint*, int, char*, int);
mpint*	letomp(uchar*, uint, mpint*);	/* byte array, little-endian */
int	mptole(mpint*, uchar*, uint, uchar**);
mpint*	betomp(uchar*, uint, mpint*);	/* byte array, little-endian */
int	mptobe(mpint*, uchar*, uint, uchar**);
uint	mptoui(mpint*);			/* unsigned int */
mpint*	uitomp(uint, mpint*);
int	mptoi(mpint*);			/* int */
mpint*	itomp(int, mpint*);
uvlong	mptouv(mpint*);			/* unsigned vlong */
mpint*	uvtomp(uvlong, mpint*);
vlong	mptov(mpint*);			/* vlong */
mpint*	vtomp(vlong, mpint*);

/* divide 2 digits by one */
void	mpdigdiv(mpdigit *dividend, mpdigit divisor, mpdigit *quotient);

/* in the following, the result mpint may be */
/* the same as one of the inputs. */
void	mpadd(mpint *b1, mpint *b2, mpint *sum);	/* sum = b1+b2 */
void	mpsub(mpint *b1, mpint *b2, mpint *diff);	/* diff = b1-b2 */
void	mpleft(mpint *b, int shift, mpint *res);	/* res = b<<shift */
void	mpright(mpint *b, int shift, mpint *res);	/* res = b>>shift */
void	mpmul(mpint *b1, mpint *b2, mpint *prod);	/* prod = b1*b2 */
void	mpexp(mpint *b, mpint *e, mpint *m, mpint *res);	/* res = b**e mod m */
void	mpmod(mpint *b, mpint *m, mpint *remainder);	/* remainder = b mod m */

/* quotient = dividend/divisor, remainder = dividend % divisor */
void	mpdiv(mpint *dividend, mpint *divisor,  mpint *quotient, mpint *remainder);

/* return neg, 0, pos as b1-b2 is neg, 0, pos */
int	mpcmp(mpint *b1, mpint *b2);

/* extended gcd return d, x, and y, s.t. d = gcd(a,b) and ax+by = d */
void	mpextendedgcd(mpint *a, mpint *b, mpint *d, mpint *x, mpint *y);

/* res = b**-1 mod m */
void	mpinvert(mpint *b, mpint *m, mpint *res);

/* bit counting */
int	mpsignif(mpint*);	/* number of sigificant bits in mantissa */
int	mplowbits0(mpint*);	/* k, where n = 2**k * q for odd q */

/* well known constants */
extern mpint	*mpzero, *mpone, *mptwo;

/* sum[0:alen] = a[0:alen-1] + b[0:blen-1] */
/* prereq: alen >= blen, sum has room for alen+1 digits */
void	mpvecadd(mpdigit *a, int alen, mpdigit *b, int blen, mpdigit *sum);

/* diff[0:alen-1] = a[0:alen-1] - b[0:blen-1] */
/* prereq: alen >= blen, diff has room for alen digits */
void	mpvecsub(mpdigit *a, int alen, mpdigit *b, int blen, mpdigit *diff);

/* p[0:n] += m * b[0:n-1] */
/* prereq: p has room for n+1 digits */
void	mpvecdigmuladd(mpdigit *b, int n, mpdigit m, mpdigit *p);

/* p[0:n] -= m * b[0:n-1] */
/* prereq: p has room for n+1 digits */
int	mpvecdigmulsub(mpdigit *b, int n, mpdigit m, mpdigit *p);

/* p[0:alen*blen-1] = a[0:alen-1] * b[0:blen-1] */
/* prereq: alen >= blen, p has room for m*n digits */
void	mpvecmul(mpdigit *a, int alen, mpdigit *b, int blen, mpdigit *p);

/* sign of a - b or zero if the same */
int	mpveccmp(mpdigit *a, int alen, mpdigit *b, int blen);

/* divide the 2 digit dividend by the one digit divisor and stick in quotient */
/* we assume that the result is one digit - overflow is all 1's */
void	mpdigdiv(mpdigit *dividend, mpdigit divisor, mpdigit *quotient);

/* playing with magnitudes */
int	mpmagcmp(mpint *b1, mpint *b2);
void	mpmagadd(mpint *b1, mpint *b2, mpint *sum);	/* sum = b1+b2 */
void	mpmagsub(mpint *b1, mpint *b2, mpint *sum);	/* sum = b1+b2 */

/* chinese remainder theorem */
typedef struct CRTpre	CRTpre;		/* precomputed values for converting */
					/*  twixt residues and mpint */
typedef struct CRTres	CRTres;		/* residue form of an mpint */

struct CRTres
{
	int	n;		/* number of residues */
	mpint	*r[1];		/* residues */
};

CRTpre*	crtpre(int, mpint**);			/* precompute conversion values */
CRTres*	crtin(CRTpre*, mpint*);			/* convert mpint to residues */
void	crtout(CRTpre*, CRTres*, mpint*);	/* convert residues to mpint */
void	crtprefree(CRTpre*);
void	crtresfree(CRTres*);


/* #pragma	varargck	type	"B"	mpint* */
#ifdef __cplusplus
}
#endif
#endif
