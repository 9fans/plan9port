#include "astro.h"

void
nutate(void)
{

/*
 *	uses radian, radsec
 *	sets phi, eps, dphi, deps, obliq, gst, tobliq
 */

	double msun, mnom, noded, dmoon;

/*
 *	nutation of the equinoxes is a wobble of the pole
 *	of the earths rotation whose magnitude is about
 *	9 seconds of arc and whose period is about 18.6 years.
 *
 *	it depends upon the pull of the sun and moon on the
 *	equatorial bulge of the earth.
 *
 *	phi and eps are the two angles which specify the
 *	true pole with respect to the mean pole.
 *
 *	all coeffieients are from Exp. Supp. pp.44-45
 */

	mnom = 296.104608 + 13.0649924465*eday + 9.192e-3*capt2
		 + 14.38e-6*capt3;
	mnom *= radian;
	msun = 358.475833 + .9856002669*eday - .150e-3*capt2
		 - 3.33e-6*capt3;
	msun *= radian;
	noded = 11.250889 + 13.2293504490*eday - 3.211e-3*capt2
		 - 0.33e-6*capt3;
	noded *= radian;
	dmoon = 350.737486 + 12.1907491914*eday - 1.436e-3*capt2
		 + 1.89e-6*capt3;
	dmoon *= radian;
	node = 259.183275 - .0529539222*eday + 2.078e-3*capt2
		 + 2.22e-6*capt3;
	node *= radian;

/*
 *	long period terms
 */

	phi = 0.;
	eps = 0.;
	dphi = 0.;
	deps = 0.;


	icosadd(nutfp, nutcp);
	phi = -(17.2327+.01737*capt)*sin(node);
	phi += sinadd(4, node, noded, dmoon, msun);

	eps = cosadd(4, node, noded, dmoon, msun);

/*
 *	short period terms
 */


	dphi = sinadd(4, node, noded, mnom, dmoon);

	deps = cosadd(3, node, noded, mnom);

	phi = (phi+dphi)*radsec;
	eps = (eps+deps)*radsec;
	dphi *= radsec;
	deps *= radsec;

	obliq = 23.452294 - .0130125*capt - 1.64e-6*capt2
		 + 0.503e-6*capt3;
	obliq *= radian;
	tobliq = obliq + eps;

	gst = 99.690983 + 360.9856473354*eday + .000387*capt2;
	gst -= 180.;
	gst = fmod(gst, 360.);
	if(gst < 0.)
		gst += 360.;
	gst *= radian;
	gst += phi*cos(obliq);
}
