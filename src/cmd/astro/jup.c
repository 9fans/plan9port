#include "astro.h"

void
jup(void)
{
	double pturbl, pturbb, pturbr;
	double lograd;
	double dele, enom, vnom, nd, sl;


	ecc = .0483376 + 163.e-6*capt;
	incl = 1.308660 - .0055*capt;
	node = 99.43785 + 1.011*capt;
	argp = 12.71165 + 1.611*capt;
	mrad = 5.202803;
	anom = 225.22165 + .0830912*eday - .0484*capt;
	motion = 299.1284/3600.;


	anom = anom;
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


	lambda += 555.*radsec;
	beta -= 51.*radsec;
	motion *= radian*mrad*mrad/(rad*rad);
	semi = 98.47;

	mag = -8.93;
	helio();
	geo();
}
