#include "astro.h"

void
mars(void)
{
	double pturbl, pturbb, pturbr;
	double lograd;
	double dele, enom, vnom, nd, sl;
	double lsun, elong, ci, dlong;


	ecc = .09331290 + .000092064*capt;
	incl = 1.850333 - 6.75e-4*capt;
	node = 48.786442 + .770992*capt;
	argp = 334.218203 + 1.840758*capt + 1.30e-4*capt2;
	mrad = 1.5236915;
	anom = 319.529425 + .5240207666*eday + 1.808e-4*capt2;
	motion = 0.5240711638;


	incl = incl*radian;
	node = node*radian;
	argp = argp*radian;
	anom = fmod(anom,360.)*radian;

	enom = anom + ecc*sin(anom);
	do {
		dele = (anom - enom + ecc * sin(enom)) /
			(1. - ecc*cos(enom));
		enom += dele;
	} while(fabs(dele) > converge);
	vnom = 2.*atan2(sqrt((1.+ecc)/(1.-ecc))*sin(enom/2.),cos(enom/2.));
	rad = mrad*(1. - ecc*cos(enom));

	lambda = vnom + argp;
	pturbl = 0.;
	lambda = lambda + pturbl*radsec;
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


	motion *= radian*mrad*mrad/(rad*rad);
	semi = 4.68;

	lsun = 99.696678 + 0.9856473354*eday;
	lsun *= radian;
	elong = lambda - lsun;
	ci = (rad - cos(elong))/sqrt(1. + rad*rad - 2.*rad*cos(elong));
	dlong = atan2(pyth(ci), ci)/radian;
	mag = -1.30 + .01486*dlong;

	helio();
	geo();
}
