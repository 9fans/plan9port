#include "astro.h"

void
helio(void)
{
/*
 *	uses lambda, beta, rad, motion
 *	sets alpha, delta, rp
 */

/*
 *	helio converts from ecliptic heliocentric coordinates
 *	referred to the mean equinox of date
 *	to equatorial geocentric coordinates referred to
 *	the true equator and equinox
 */

	double xmp, ymp, zmp;
	double beta2;

/*
 *	compute geocentric distance of object and
 *	compute light-time correction (i.i. planetary aberration)
 */

	xmp = rad*cos(beta)*cos(lambda);
	ymp = rad*cos(beta)*sin(lambda);
	zmp = rad*sin(beta);
	rp = sqrt((xmp+xms)*(xmp+xms) +
		(ymp+yms)*(ymp+yms) +
		(zmp+zms)*(zmp+zms));
	lmb2 = lambda - .0057756e0*rp*motion;

	xmp = rad*cos(beta)*cos(lmb2);
	ymp = rad*cos(beta)*sin(lmb2);
	zmp = rad*sin(beta);

/*
 *	compute annual parallax from the position of the sun
 */

	xmp += xms;
	ymp += yms;
	zmp += zms;
	rp = sqrt(xmp*xmp + ymp*ymp + zmp*zmp);

/*
 *	compute annual (i.e. stellar) aberration
 *	from the orbital velocity of the earth
 *	(by an incorrect method)
 */

	xmp -= xdot*rp;
	ymp -= ydot*rp;
	zmp -= zdot*rp;

/*
 *	perform the nutation and so convert from the mean
 *	equator and equinox to the true
 */

	lmb2 = atan2(ymp, xmp);
	beta2 = atan2(zmp, sqrt(xmp*xmp+ymp*ymp));
	lmb2 += phi;

/*
 *	change to equatorial coordinates
 */

	xmp = rp*cos(lmb2)*cos(beta2);
	ymp = rp*(sin(lmb2)*cos(beta2)*cos(tobliq) - sin(tobliq)*sin(beta2));
	zmp = rp*(sin(lmb2)*cos(beta2)*sin(tobliq) + cos(tobliq)*sin(beta2));

	alpha = atan2(ymp, xmp);
	delta = atan2(zmp, sqrt(xmp*xmp+ymp*ymp));

	hp = 8.794e0*radsec/rp;
	semi /= rp;
	if(rad > 0 && rad < 2.e5)
		mag += 2.17*log(rad*rp);
}
