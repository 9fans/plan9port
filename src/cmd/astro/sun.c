#include "astro.h"

void
sun(void)
{
	double mven, merth, mmars, mjup, msat;
	double dmoon, mmoon, gmoon;
	double pturbb, pturbl, pturbr, lograd;

	ecc = .01675104 - 4.180e-5 * capt - 1.26e-7*capt2;
	incl = 0;
	node = 0;
	argp = 281.220833 + .0000470684*eday + .000453*capt2
		 + .000003*capt3;
	mrad = 1;
	anom = 358.475845 + .9856002670*eday - .000150*capt2
		 - .000003*capt3;
	motion = .9856473354;

	dmoon = 350.737681+12.1907491914*eday-.001436*capt2;
	gmoon = 11.250889 + 13.2293504490*eday - .003212*capt2;
	mmoon = 296.104608 + 13.0649924465*eday + 9.192e-3*capt2;
	mven  = 212.448 + 1.602121635*eday;
	merth = 358.476 + 0.985600267*eday;
	mmars = 319.590 + .524024095*eday;
	mjup = 225.269 + .083082362*eday;
	msat  = 175.593 + .033450794*eday;

	dmoon = fmod(dmoon, 360.)*radian;
	gmoon = fmod(gmoon, 360.)*radian;
	mmoon = fmod(mmoon, 360.)*radian;
	mven  *= radian;
	merth *= radian;
	mmars *= radian;
	mjup *= radian;
	msat  *= radian;

	icosadd(sunfp, suncp);
	anom += cosadd(4, mmars, merth, mven, mjup)/3600.;
	anom += sinadd(5, mmars, merth, mven, mjup, .07884*capt)/3600.;

	incl *= radian;
	node *= radian;
	argp *= radian;
	anom = fmod(anom, 360.)*radian;

/*
 *	computation of elliptic orbit
 */

	lambda = anom + argp;

	pturbl = (6910.057 - 17.240*capt - 0.052*capt2)*sin(anom)
		 + (72.338 - 0.361*capt) * sin(2.*anom)
		 + (1.054 - 0.001*capt) * sin(3.*anom)
		 + 0.018 * sin(4.*anom);

	lambda += pturbl*radsec;

	beta = 0.;

	lograd = (30.57e-6 - 0.15e-6*capt)
		 - (7274.12e-6 - 18.14e-6*capt - 0.05e-6*capt2)*cos(anom)
		 - (91.38e-6 - 0.46e-6*capt) * cos(2.*anom)
		 - (1.45e-6 - 0.01e-6*capt) * cos(3.*anom)
		 - 0.02e-6 * cos(4.*anom);

	pturbl = cosadd(5, mmars, merth, mven, mjup, msat);
	pturbl += sinadd(3, dmoon, mmoon, merth) + .9;
	pturbl *= radsec;

	pturbb =  cosadd(3, merth, mven, mjup);
	pturbb += sinadd(3, gmoon, mmoon, dmoon);
	pturbb *= radsec;

	pturbr =  cosadd(5, mmars, merth, mven, mjup, msat);
	pturbr += cosadd(3, dmoon, mmoon, merth);

	lambda += pturbl;
	if(lambda > pipi)
		lambda -= pipi;

	beta += pturbb;

	lograd = (lograd+pturbr) * 2.30258509;
	rad = 1 + lograd * (1 + lograd * (.5 + lograd/6));

	motion *= radian*mrad*mrad/(rad*rad);

	semi = 961.182;
	if(flags['o'])
		semi = 959.63;
	mag = -26.5;
}
