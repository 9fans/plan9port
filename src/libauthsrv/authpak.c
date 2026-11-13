#include <u.h>
#include <libc.h>
#include <mp.h>
#include <libsec.h>
#include <authsrv.h>

#include "msqrt.mpc"
#include "decaf.mpc"
#include "edwards.mpc"
#include "elligator2.mpc"
#include "spake2ee.mpc"
#include "ed448.mpc"

typedef struct PAKcurve PAKcurve;
struct PAKcurve
{
	Lock	lk;
	mpint	*P;
	mpint	*A;
	mpint	*D;
	mpint	*X;
	mpint	*Y;
};

static PAKcurve*
authpak_curve(void)
{
	static PAKcurve a;

	lock(&a.lk);
	if(a.P == nil){
		a.P = mpnew(0);
		a.A = mpnew(0);
		a.D = mpnew(0);
		a.X = mpnew(0);
		a.Y = mpnew(0);
		ed448_curve(a.P, a.A, a.D, a.X, a.Y);
		a.P = mpfield(a.P);
	}
	unlock(&a.lk);
	return &a;
}

void
authpak_hash(Authkey *k, char *u)
{
	static char info[] = "Plan 9 AuthPAK hash";
	uchar *bp, salt[SHA2_256dlen], h[2*PAKSLEN];
	mpint *H, *PX,*PY,*PZ,*PT;
	PAKcurve *c;

	H = mpnew(0);
	PX = mpnew(0);
	PY = mpnew(0);
	PZ = mpnew(0);
	PT = mpnew(0);

	sha2_256((uchar*)u, strlen(u), salt, nil);

	hkdf_x(	salt, SHA2_256dlen,
		(uchar*)info, sizeof(info)-1,
		k->aes, AESKEYLEN,
		h, sizeof(h),
		hmac_sha2_256, SHA2_256dlen);

	c = authpak_curve();

	betomp(h + 0*PAKSLEN, PAKSLEN, H);		/* HM */
	spake2ee_h2P(c->P,c->A,c->D, H, PX,PY,PZ,PT);	/* PM */

	bp = k->pakhash;
	mptober(PX, bp, PAKSLEN), bp += PAKSLEN;
	mptober(PY, bp, PAKSLEN), bp += PAKSLEN;
	mptober(PZ, bp, PAKSLEN), bp += PAKSLEN;
	mptober(PT, bp, PAKSLEN), bp += PAKSLEN;

	betomp(h + 1*PAKSLEN, PAKSLEN, H);		/* HN */
	spake2ee_h2P(c->P,c->A,c->D, H, PX,PY,PZ,PT);	/* PN */

	mptober(PX, bp, PAKSLEN), bp += PAKSLEN;
	mptober(PY, bp, PAKSLEN), bp += PAKSLEN;
	mptober(PZ, bp, PAKSLEN), bp += PAKSLEN;
	mptober(PT, bp, PAKSLEN);

	mpfree(PX);
	mpfree(PY);
	mpfree(PZ);
	mpfree(PT);
	mpfree(H);
}

void
authpak_new(PAKpriv *p, Authkey *k, uchar y[PAKYLEN], int isclient)
{
	mpint *PX,*PY,*PZ,*PT, *X, *Y;
	PAKcurve *c;
	uchar *bp;

	memset(p, 0, sizeof(PAKpriv));
	p->isclient = isclient != 0;

	X = mpnew(0);
	Y = mpnew(0);

	PX = mpnew(0);
	PY = mpnew(0);
	PZ = mpnew(0);
	PT = mpnew(0);

	PX->flags |= MPtimesafe;
	PY->flags |= MPtimesafe;
	PZ->flags |= MPtimesafe;
	PT->flags |= MPtimesafe;

	bp = k->pakhash + PAKPLEN*(p->isclient == 0);
	betomp(bp, PAKSLEN, PX), bp += PAKSLEN;
	betomp(bp, PAKSLEN, PY), bp += PAKSLEN;
	betomp(bp, PAKSLEN, PZ), bp += PAKSLEN;
	betomp(bp, PAKSLEN, PT);

	c = authpak_curve();

	X->flags |= MPtimesafe;
	mpnrand(c->P, genrandom, X);

	spake2ee_1(c->P,c->A,c->D, X, c->X,c->Y, PX,PY,PZ,PT, Y);

	mptober(X, p->x, PAKXLEN);
	mptober(Y, p->y, PAKYLEN);

	memmove(y, p->y, PAKYLEN);

	mpfree(PX);
	mpfree(PY);
	mpfree(PZ);
	mpfree(PT);

	mpfree(X);
	mpfree(Y);
}

int
authpak_finish(PAKpriv *p, Authkey *k, uchar y[PAKYLEN])
{
	static char info[] = "Plan 9 AuthPAK key";
	uchar *bp, z[PAKSLEN], salt[SHA2_256dlen];
	mpint *PX,*PY,*PZ,*PT, *X, *Y, *Z, *ok;
	DigestState *s;
	PAKcurve *c;
	int ret;

	X = mpnew(0);
	Y = mpnew(0);
	Z = mpnew(0);
	ok = mpnew(0);

	PX = mpnew(0);
	PY = mpnew(0);
	PZ = mpnew(0);
	PT = mpnew(0);

	PX->flags |= MPtimesafe;
	PY->flags |= MPtimesafe;
	PZ->flags |= MPtimesafe;
	PT->flags |= MPtimesafe;

	bp = k->pakhash + PAKPLEN*(p->isclient != 0);
	betomp(bp, PAKSLEN, PX), bp += PAKSLEN;
	betomp(bp, PAKSLEN, PY), bp += PAKSLEN;
	betomp(bp, PAKSLEN, PZ), bp += PAKSLEN;
	betomp(bp, PAKSLEN, PT);

	Z->flags |= MPtimesafe;
	X->flags |= MPtimesafe;
	betomp(p->x, PAKXLEN, X);

	betomp(y, PAKYLEN, Y);

	c = authpak_curve();
	spake2ee_2(c->P,c->A,c->D, PX,PY,PZ,PT, X, Y, ok, Z);

	if(mpcmp(ok, mpzero) == 0){
		ret = -1;
		goto out;
	}

	mptober(Z, z, sizeof(z));

	s = sha2_256(p->isclient ? p->y : y, PAKYLEN, nil, nil);
	sha2_256(p->isclient ? y : p->y, PAKYLEN, salt, s);

	hkdf_x(	salt, SHA2_256dlen,
		(uchar*)info, sizeof(info)-1,
		z, sizeof(z),
		k->pakkey, PAKKEYLEN,
		hmac_sha2_256, SHA2_256dlen);

	ret = 0;
out:
	memset(z, 0, sizeof(z));
	memset(p, 0, sizeof(PAKpriv));

	mpfree(PX);
	mpfree(PY);
	mpfree(PZ);
	mpfree(PT);

	mpfree(X);
	mpfree(Y);
	mpfree(Z);
	mpfree(ok);

	return ret;
}
