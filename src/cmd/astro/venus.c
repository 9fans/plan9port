#include "astro.h"


void
venus(void)
{
	double pturbl, pturbb, pturbr;
	double lograd;
	double dele, enom, vnom, nd, sl;
	double v0, t0, m0, j0, s0;
	double lsun, elong, ci, dlong;

/*
 *	here are the mean orbital elements
 */

	ecc = .00682069 - .00004774*capt + 0.091e-6*capt2;
	incl = 3.393631 + .0010058*capt - 0.97e-6*capt2;
	node = 75.779647 + .89985*capt + .00041*capt2;
	argp = 130.163833 + 1.408036*capt - .0009763*capt2;
	mrad = .7233316;
	anom = 212.603219 + 1.6021301540*eday + .00128605*capt2;
	motion = 1.6021687039;

/*
 *	mean anomalies of perturbing planets
 */

	v0 = 212.60 + 1.602130154*eday;
	t0 = 358.63  + .985608747*eday;
	m0 = 319.74 + 0.524032490*eday;
	j0 = 225.43 + .083090842*eday;
	s0 = 175.8  + .033459258*eday;

	v0 *= radian;
	t0 *= radian;
	m0 *= radian;
	j0 *= radian;
	s0 *= radian;

	incl *= radian;
	node *= radian;
	argp *= radian;
	anom = fmod(anom, 360.)*radian;

/*
 *	computation of long period terms affecting the mean anomaly
 */

	anom +=
		   (2.761-0.022*capt)*radsec*sin(
		  13.*t0 - 8.*v0 + 43.83*radian + 4.52*radian*capt)
		 + 0.268*radsec*cos(4.*m0 - 7.*t0 + 3.*v0)
		 + 0.019*radsec*sin(4.*m0 - 7.*t0 + 3.*v0)
		 - 0.208*radsec*sin(s0 + 1.4*radian*capt);

/*
 *	computation of elliptic orbit
 */

	enom = anom + ecc*sin(anom);
	do {
		dele = (anom - enom + ecc * sin(enom)) /
			(1 - ecc*cos(enom));
		enom += dele;
	} while(fabs(dele) > converge);
	vnom = 2*atan2(sqrt((1+ecc)/(1-ecc))*sin(enom/2),
		cos(enom/2));
	rad = mrad*(1 - ecc*cos(enom));

	lambda = vnom + argp;

/*
 *	perturbations in longitude
 */

	icosadd(venfp, vencp);
	pturbl = cosadd(4, v0, t0, m0, j0);
	pturbl *= radsec;

/*
 *	perturbations in latidude
 */

	pturbb = cosadd(3, v0, t0, j0);
	pturbb *= radsec;

/*
 *	perturbations in log radius vector
 */

	pturbr = cosadd(4, v0, t0, m0, j0);

/*
 *	reduction to the ecliptic
 */

	lambda += pturbl;
	nd = lambda - node;
	lambda = node + atan2(sin(nd)*cos(incl),cos(nd));

	sl = sin(incl)*sin(nd);
	beta = atan2(sl, pyth(sl)) + pturbb;

	lograd = pturbr*2.30258509;
	rad *= 1 + lograd;


	motion *= radian*mrad*mrad/(rad*rad);

/*
 *	computation of magnitude
 */

	lsun = 99.696678 + 0.9856473354*eday;
	lsun *= radian;
	elong = lambda - lsun;
	ci = (rad - cos(elong))/sqrt(1 + rad*rad - 2*rad*cos(elong));
	dlong = atan2(pyth(ci), ci)/radian;
	mag = -4 + .01322*dlong + .0000004247*dlong*dlong*dlong;

	semi = 8.41;

	helio();
	geo();
}
