#include "astro.h"

void
merc(void)
{
	double pturbl, pturbr;
	double lograd;
	double dele, enom, vnom, nd, sl;
	double q0, v0, t0, j0 , s0;
	double lsun, elong, ci, dlong;

	ecc = .20561421 + .00002046*capt - 0.03e-6*capt2;
	incl = 7.0028806 + .0018608*capt - 18.3e-6*capt2;
	node = 47.145944 + 1.185208*capt + .0001739*capt2;
	argp = 75.899697 + 1.555490*capt + .0002947*capt2;
	mrad = .3870986;
	anom = 102.279381 + 4.0923344364*eday + 6.7e-6*capt2;
	motion = 4.0923770233;

	q0 = 102.28  + 4.092334429*eday;
	v0 = 212.536 + 1.602126105*eday;
	t0 = -1.45  + .985604737*eday;
	j0 = 225.36 + .083086735*eday;
	s0 = 175.68 + .033455441*eday;

	q0 *= radian;
	v0 *= radian;
	t0 *= radian;
	j0 *= radian;
	s0 *= radian;

	incl *= radian;
	node *= radian;
	argp *= radian;
	anom = fmod(anom, 360.)*radian;


	enom = anom + ecc*sin(anom);
	do {
		dele = (anom - enom + ecc * sin(enom)) /
			(1. - ecc*cos(enom));
		enom += dele;
	} while(fabs(dele) > converge);
	vnom = 2.*atan2(sqrt((1.+ecc)/(1.-ecc))*sin(enom/2.),
			cos(enom/2.));
	rad = mrad*(1. - ecc*cos(enom));

	icosadd(mercfp, merccp);
	pturbl =  cosadd(2, q0, -v0);
	pturbl += cosadd(2, q0, -t0);
	pturbl += cosadd(2, q0, -j0);
	pturbl += cosadd(2, q0, -s0);

	pturbr =  cosadd(2, q0, -v0);
	pturbr += cosadd(2, q0, -t0);
	pturbr += cosadd(2, q0, -j0);

/*
 *	reduce to the ecliptic
 */

	lambda = vnom + argp + pturbl*radsec;
	nd = lambda - node;
	lambda = node + atan2(sin(nd)*cos(incl), cos(nd));

	sl = sin(incl)*sin(nd);
	beta = atan2(sl, pyth(sl));

	lograd = pturbr*2.30258509;
	rad *= 1. + lograd;

	motion *= radian*mrad*mrad/(rad*rad);
	semi = 3.34;

	lsun = 99.696678 + 0.9856473354*eday;
	lsun *= radian;
	elong = lambda - lsun;
	ci = (rad - cos(elong))/sqrt(1. + rad*rad - 2.*rad*cos(elong));
	dlong = atan2(pyth(ci), ci)/radian;
	mag = -.003 + .01815*dlong + .0001023*dlong*dlong;

	helio();
	geo();
}
