#include "astro.h"

static	double	elem[] =
{
	36525.0,		// [0] eday of epoc

	39.48168677,		// [1] semi major axis (au)
	0.24880766,		// [2] eccentricity
 	17.14175,		// [3] inclination (deg)
	110.30347,		// [4] longitude of the ascending node (deg)
	224.06676,		// [5] longitude of perihelion (deg)
	238.92881,		// [6] mean longitude (deg)

	-0.00076912,		// [1+6] (au/julian century)
	0.00006465,		// [2+6] (e/julian century)
  	11.07,			// [3+6] (arcsec/julian century)
	-37.33,			// [4+6] (arcsec/julian century)
	-132.25,		// [5+6] (arcsec/julian century)
	522747.90,		// [6+6] (arcsec/julian century)
};

void
plut(void)
{
	double pturbl, pturbb, pturbr;
	double lograd;
	double dele, enom, vnom, nd, sl;

	double capj, capn, eye, comg, omg;
	double sb, su, cu, u, b, up;
	double sd, ca, sa;

	double cy;

	cy = (eday - elem[0]) / 36525.;		// per julian century

	mrad = elem[1] + elem[1+6]*cy;
	ecc = elem[2] + elem[2+6]*cy;

	cy = cy / 3600;				// arcsec/deg per julian century
	incl = elem[3] + elem[3+6]*cy;
	node = elem[4] + elem[4+6]*cy;
	argp = elem[5] + elem[5+6]*cy;

	anom = elem[6] + elem[6+6]*cy - argp;
	motion = elem[6+6] / 36525. / 3600;

	incl *= radian;
	node *= radian;
	argp *= radian;
	anom = fmod(anom,360.)*radian;

	enom = anom + ecc*sin(anom);
	do {
		dele = (anom - enom + ecc * sin(enom)) /
			(1. - ecc*cos(enom));
		enom += dele;
	} while(fabs(dele) > converge);
	vnom = 2.*atan2(sqrt((1.+ecc)/(1.-ecc))*sin(enom/2.),
		cos(enom/2.));
	rad = mrad*(1. - ecc*cos(enom));

	lambda = vnom + argp;
	pturbl = 0.;
	lambda += pturbl*radsec;
	pturbb = 0.;
	pturbr = 0.;

/*
 *	reduce to the ecliptic
 */

	nd = lambda - node;
	lambda = node + atan2(sin(nd)*cos(incl),cos(nd));

	sl = sin(incl)*sin(nd) + pturbb*radsec;
	beta = atan2(sl, pyth(sl));

	lograd = pturbr*2.30258509;
	rad *= 1. + lograd;


	lambda -= 1185.*radsec;
	beta -= 51.*radsec;

	motion *= radian*mrad*mrad/(rad*rad);
	semi = 83.33;

/*
 *	here begins the computation of magnitude
 *	first find the geocentric equatorial coordinates of Saturn
 */

	sd = rad*(cos(beta)*sin(lambda)*sin(obliq) +
		sin(beta)*cos(obliq));
	sa = rad*(cos(beta)*sin(lambda)*cos(obliq) -
		sin(beta)*sin(obliq));
	ca = rad*cos(beta)*cos(lambda);
	sd += zms;
	sa += yms;
	ca += xms;
	alpha = atan2(sa,ca);
	delta = atan2(sd,sqrt(sa*sa+ca*ca));

/*
 *	here are the necessary elements of Saturn's rings
 *	cf. Exp. Supp. p. 363ff.
 */

	capj = 6.9056 - 0.4322*capt;
	capn = 126.3615 + 3.9894*capt + 0.2403*capt2;
	eye = 28.0743 - 0.0128*capt;
	comg = 168.1179 + 1.3936*capt;
	omg = 42.9236 - 2.7390*capt - 0.2344*capt2;

	capj *= radian;
	capn *= radian;
	eye *= radian;
	comg *= radian;
	omg *= radian;

/*
 *	now find saturnicentric ring-plane coords of the earth
 */

	sb = sin(capj)*cos(delta)*sin(alpha-capn) -
		cos(capj)*sin(delta);
	su = cos(capj)*cos(delta)*sin(alpha-capn) +
		sin(capj)*sin(delta);
	cu = cos(delta)*cos(alpha-capn);
	u = atan2(su,cu);
	b = atan2(sb,sqrt(su*su+cu*cu));

/*
 *	and then the saturnicentric ring-plane coords of the sun
 */

	su = sin(eye)*sin(beta) +
		cos(eye)*cos(beta)*sin(lambda-comg);
	cu = cos(beta)*cos(lambda-comg);
	up = atan2(su,cu);

/*
 *	at last, the magnitude
 */


	sb = sin(b);
	mag = -8.68 +2.52*fabs(up+omg-u)-
		2.60*fabs(sb) + 1.25*(sb*sb);

	helio();
	geo();
}
