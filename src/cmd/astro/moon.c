#include "astro.h"

double k1, k2, k3, k4;
double mnom, msun, noded, dmoon;

void
moon(void)
{
	Moontab *mp;
	double dlong, lsun, psun;
	double eccm, eccs, chp, cpe;
	double v0, t0, m0, j0;
	double arg1, arg2, arg3, arg4, arg5, arg6, arg7;
	double arg8, arg9, arg10;
	double dgamma, k5, k6;
	double lterms, sterms, cterms, nterms, pterms, spterms;
	double gamma1, gamma2, gamma3, arglat;
	double xmp, ymp, zmp;
	double obl2;

/*
 *	the fundamental elements - all referred to the epoch of
 *	Jan 0.5, 1900 and to the mean equinox of date.
 */

	dlong = 270.434164 + 13.1763965268*eday - .001133*capt2
		 + 2.e-6*capt3;
	argp = 334.329556 + .1114040803*eday - .010325*capt2
		 - 12.e-6*capt3;
	node = 259.183275 - .0529539222*eday + .002078*capt2
		 + 2.e-6*capt3;
	lsun = 279.696678 + .9856473354*eday + .000303*capt2;
	psun = 281.220833 + .0000470684*eday + .000453*capt2
		 + 3.e-6*capt3;

	dlong = fmod(dlong, 360.);
	argp = fmod(argp, 360.);
	node = fmod(node, 360.);
	lsun = fmod(lsun, 360.);
	psun = fmod(psun, 360.);

	eccm = 22639.550;
	eccs = .01675104 - .00004180*capt;
	incl = 18461.400;
	cpe = 124.986;
	chp = 3422.451;

/*
 *	some subsidiary elements - they are all longitudes
 *	and they are referred to the epoch 1/0.5 1900 and
 *	to the fixed mean equinox of 1850.0.
 */

	v0 = 342.069128 + 1.6021304820*eday;
	t0 =  98.998753 + 0.9856091138*eday;
	m0 = 293.049675 + 0.5240329445*eday;
	j0 = 237.352319 + 0.0830912295*eday;

/*
 *	the following are periodic corrections to the
 *	fundamental elements and constants.
 *	arg3 is the "Great Venus Inequality".
 */

	arg1 = 41.1 + 20.2*(capt+.5);
	arg2 = dlong - argp + 33. + 3.*t0 - 10.*v0 - 2.6*(capt+.5);
	arg3 = dlong - argp + 151.1 + 16.*t0 - 18.*v0 - (capt+.5);
	arg4 = node;
	arg5 = node + 276.2 - 2.3*(capt+.5);
	arg6 = 313.9 + 13.*t0 - 8.*v0;
	arg7 = dlong - argp + 112.0 + 29.*t0 - 26.*v0;
	arg8 = dlong + argp - 2.*lsun + 273. + 21.*t0 - 20.*v0;
	arg9 = node + 290.1 - 0.9*(capt+.5);
	arg10 = 115. + 38.5*(capt+.5);
	arg1 *= radian;
	arg2 *= radian;
	arg3 *= radian;
	arg4 *= radian;
	arg5 *= radian;
	arg6 *= radian;
	arg7 *= radian;
	arg8 *= radian;
	arg9 *= radian;
	arg10 *= radian;

	dlong +=
		   (0.84 *sin(arg1)
		 +  0.31 *sin(arg2)
		 + 14.27 *sin(arg3)
		 +  7.261*sin(arg4)
		 +  0.282*sin(arg5)
		 +  0.237*sin(arg6)
		 +  0.108*sin(arg7)
		 +  0.126*sin(arg8))/3600.;

	argp +=
		 (- 2.10 *sin(arg1)
		 -  0.118*sin(arg3)
		 -  2.076*sin(arg4)
		 -  0.840*sin(arg5)
		 -  0.593*sin(arg6))/3600.;

	node +=
		   (0.63*sin(arg1)
		 +  0.17*sin(arg3)
		 + 95.96*sin(arg4)
		 + 15.58*sin(arg5)
		 +  1.86*sin(arg9))/3600.;

	t0 +=
		 (- 6.40*sin(arg1)
		 -  1.89*sin(arg6))/3600.;

	psun +=
		   (6.40*sin(arg1)
		 +  1.89*sin(arg6))/3600.;

	dgamma = -  4.318*cos(arg4)
		 -  0.698*cos(arg5)
		 -  0.083*cos(arg9);

	j0 +=
		   0.33*sin(arg10);

/*
 *	the following factors account for the fact that the
 *	eccentricity, solar eccentricity, inclination and
 *	parallax used by Brown to make up his coefficients
 *	are both wrong and out of date.  Brown did the same
 *	thing in a different way.
 */

	k1 = eccm/22639.500;
	k2 = eccs/.01675104;
	k3 = 1. + 2.708e-6 + .000108008*dgamma;
	k4 = cpe/125.154;
	k5 = chp/3422.700;

/*
 *	the principal arguments that are used to compute
 *	perturbations are the following differences of the
 *	fundamental elements.
 */

	mnom = dlong - argp;
	msun = lsun - psun;
	noded = dlong - node;
	dmoon = dlong - lsun;

/*
 *	solar terms in longitude
 */

	lterms = 0.0;
	mp = moontab;
	for(;;) {
		if(mp->f == 0.0)
			break;
		lterms += sinx(mp->f,
			mp->c[0], mp->c[1],
			mp->c[2], mp->c[3], 0.0);
		mp++;
	}
	mp++;

/*
 *	planetary terms in longitude
 */

	lterms += sinx(0.822, 0,0,0,0, t0-v0);
	lterms += sinx(0.307, 0,0,0,0, 2.*t0-2.*v0+179.8);
	lterms += sinx(0.348, 0,0,0,0, 3.*t0-2.*v0+272.9);
	lterms += sinx(0.176, 0,0,0,0, 4.*t0-3.*v0+271.7);
	lterms += sinx(0.092, 0,0,0,0, 5.*t0-3.*v0+199.);
	lterms += sinx(0.129, 1,0,0,0, -t0+v0+180.);
	lterms += sinx(0.152, 1,0,0,0, t0-v0);
	lterms += sinx(0.127, 1,0,0,0, 3.*t0-3.*v0+180.);
	lterms += sinx(0.099, 0,0,0,2, t0-v0);
	lterms += sinx(0.136, 0,0,0,2, 2.*t0-2.*v0+179.5);
	lterms += sinx(0.083, -1,0,0,2, -4.*t0+4.*v0+180.);
	lterms += sinx(0.662, -1,0,0,2, -3.*t0+3.*v0+180.0);
	lterms += sinx(0.137, -1,0,0,2, -2.*t0+2.*v0);
	lterms += sinx(0.133, -1,0,0,2, t0-v0);
	lterms += sinx(0.157, -1,0,0,2, 2.*t0-2.*v0+179.6);
	lterms += sinx(0.079, -1,0,0,2, -8.*t0+6.*v0+162.6);
	lterms += sinx(0.073, 2,0,0,-2, 3.*t0-3.*v0+180.);
	lterms += sinx(0.643, 0,0,0,0, -t0+j0+178.8);
	lterms += sinx(0.187, 0,0,0,0, -2.*t0+2.*j0+359.6);
	lterms += sinx(0.087, 0,0,0,0, j0+289.9);
	lterms += sinx(0.165, 0,0,0,0, -t0+2.*j0+241.5);
	lterms += sinx(0.144, 1,0,0,0, t0-j0+1.0);
	lterms += sinx(0.158, 1,0,0,0, -t0+j0+179.0);
	lterms += sinx(0.190, 1,0,0,0, -2.*t0+2.*j0+180.0);
	lterms += sinx(0.096, 1,0,0,0, -2.*t0+3.*j0+352.5);
	lterms += sinx(0.070, 0,0,0,2, 2.*t0-2.*j0+180.);
	lterms += sinx(0.167, 0,0,0,2, -t0+j0+178.5);
	lterms += sinx(0.085, 0,0,0,2, -2.*t0+2.*j0+359.2);
	lterms += sinx(1.137, -1,0,0,2, 2.*t0-2.*j0+180.3);
	lterms += sinx(0.211, -1,0,0,2, -t0+j0+178.4);
	lterms += sinx(0.089, -1,0,0,2, -2.*t0+2.*j0+359.2);
	lterms += sinx(0.436, -1,0,0,2, 2.*t0-3.*j0+7.5);
	lterms += sinx(0.240, 2,0,0,-2, -2.*t0+2.*j0+179.9);
	lterms += sinx(0.284, 2,0,0,-2, -2.*t0+3.*j0+172.5);
	lterms += sinx(0.195, 0,0,0,0, -2.*t0+2.*m0+180.2);
	lterms += sinx(0.327, 0,0,0,0, -t0+2.*m0+224.4);
	lterms += sinx(0.093, 0,0,0,0, -2.*t0+4.*m0+244.8);
	lterms += sinx(0.073, 1,0,0,0, -t0+2.*m0+223.3);
	lterms += sinx(0.074, 1,0,0,0, t0-2.*m0+306.3);
	lterms += sinx(0.189, 0,0,0,0, node+180.);

/*
 *	solar terms in latitude
 */

	sterms = 0;
	for(;;) {
		if(mp->f == 0)
			break;
		sterms += sinx(mp->f,
			mp->c[0], mp->c[1],
			mp->c[2], mp->c[3], 0);
		mp++;
	}
	mp++;

	cterms = 0;
	for(;;) {
		if(mp->f == 0)
			break;
		cterms += cosx(mp->f,
			mp->c[0], mp->c[1],
			mp->c[2], mp->c[3], 0);
		mp++;
	}
	mp++;

	nterms = 0;
	for(;;) {
		if(mp->f == 0)
			break;
		nterms += sinx(mp->f,
			mp->c[0], mp->c[1],
			mp->c[2], mp->c[3], 0);
		mp++;
	}
	mp++;

/*
 *	planetary terms in latitude
 */

	pterms =
		   sinx(0.215, 0,0,0,0, dlong);

/*
 *	solar terms in parallax
 */

	spterms = 3422.700;
	for(;;) {
		if(mp->f == 0)
			break;
		spterms += cosx(mp->f,
			mp->c[0], mp->c[1],
			mp->c[2], mp->c[3], 0);
		mp++;
	}

/*
 *	planetary terms in parallax
 */

	spterms = spterms;

/*
 *	computation of longitude
 */

	lambda = (dlong + lterms/3600.)*radian;

/*
 *	computation of latitude
 */

	arglat = (noded + sterms/3600.)*radian;
	gamma1 = 18519.700 * k3;
	gamma2 = -6.241 * k3*k3*k3;
	gamma3 = 0.004 * k3*k3*k3*k3*k3;

	k6 = (gamma1 + cterms) / gamma1;

	beta = k6 * (gamma1*sin(arglat) + gamma2*sin(3.*arglat)
		 + gamma3*sin(5.*arglat) + nterms)
		 + pterms;
	if(flags['o'])
		beta -= 0.6;
	beta *= radsec;

/*
 *	computation of parallax
 */

	spterms = k5 * spterms *radsec;
	hp = spterms + (spterms*spterms*spterms)/6.;

	rad = hp/radsec;
	rp = 1.;
	semi = .0799 + .272453*(hp/radsec);
	if(dmoon < 0.)
		dmoon += 360.;
	mag = dmoon/360.;

/*
 *	change to equatorial coordinates
 */

	lambda += phi;
	obl2 = obliq + eps;
	xmp = rp*cos(lambda)*cos(beta);
	ymp = rp*(sin(lambda)*cos(beta)*cos(obl2) - sin(obl2)*sin(beta));
	zmp = rp*(sin(lambda)*cos(beta)*sin(obl2) + cos(obl2)*sin(beta));

	alpha = atan2(ymp, xmp);
	delta = atan2(zmp, sqrt(xmp*xmp+ymp*ymp));
	meday = eday;
	mhp = hp;

	geo();
}

double
sinx(double coef, int i, int j, int k, int m, double angle)
{
	double x;

	x = i*mnom + j*msun + k*noded + m*dmoon + angle;
	x = coef*sin(x*radian);
	if(i < 0)
		i = -i;
	for(; i>0; i--)
		x *= k1;
	if(j < 0)
		j = -j;
	for(; j>0; j--)
		x *= k2;
	if(k < 0)
		k = -k;
	for(; k>0; k--)
		x *= k3;
	if(m & 1)
		x *= k4;

	return x;
}

double
cosx(double coef, int i, int j, int k, int m, double angle)
{
	double x;

	x = i*mnom + j*msun + k*noded + m*dmoon + angle;
	x = coef*cos(x*radian);
	if(i < 0)
		i = -i;
	for(; i>0; i--)
		x *= k1;
	if(j < 0)
		j = -j;
	for(; j>0; j--)
		x *= k2;
	if(k < 0)
		k = -k;
	for(; k>0; k--)
		x *= k3;
	if(m & 1)
		x *= k4;

	return x;
}
