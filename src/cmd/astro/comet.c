#include "astro.h"

#define	MAXE	(.999)	/* cant do hyperbolas */

void
comet(void)
{
	double pturbl, pturbb, pturbr;
	double lograd;
	double dele, enom, vnom, nd, sl;

	struct	elem
	{
		double	t;	/* time of perihelion */
		double	q;	/* perihelion distance */
		double	e;	/* eccentricity */
		double	i;	/* inclination */
		double	w;	/* argument of perihelion */
		double	o;	/* longitude of ascending node */
	} elem;

/*	elem = (struct elem)
	{
		etdate(1990, 5, 19.293),
		0.9362731,
		0.6940149,
		11.41096,
		198.77059,
		69.27130,
	};	/* p/schwassmann-wachmann 3, 1989d */
/*	elem = (struct elem)
	{
		etdate(1990, 4, 9.9761),
		0.349957,
		1.00038,
		58.9596,
		61.5546,
		75.2132,
	};	/* austin 3, 1989c */
/*	elem = (struct elem)
	{
		etdate(1990, 10, 24.36),
		0.9385,
		1.00038,
		131.62,
		242.58,
		138.57,
	};	/* levy 6 , 1990c */
/*	elem=(struct elem)
	{
		etdate(1996, 5, 1.3965),
		0.230035,
		0.999662,
		124.9098,
		130.2102,
		188.0430,
	};	/* C/1996 B2 (Hyakutake) */
/*	elem=(struct elem)
	{
		etdate(1997, 4, 1.13413),
		0.9141047,
		0.9950989,
		89.42932,
		130.59066,
		282.47069,
	};	/*C/1995 O1 (Hale-Bopp) */
/*	elem=(struct elem)
	{
		etdate(2000, 7, 26.1754),
		0.765126,
		0.999356,
		149.3904,
		151.0510,
		83.1909,
	};	/*C/1999 S4 (Linear) */
	elem=(struct elem)
	{
		etdate(2002, 3, 18.9784),
		0.5070601,
		0.990111,
		28.12106,
		34.6666,
		93.1206,
	};	/*C/2002 C1 (Ikeya-Zhang) */

	ecc = elem.e;
	if(ecc > MAXE)
		ecc = MAXE;
	incl = elem.i * radian;
	node = (elem.o + 0.4593) * radian;
	argp = (elem.w + elem.o + 0.4066) * radian;
	mrad = elem.q / (1-ecc);
        motion = .01720209895 * sqrt(1/(mrad*mrad*mrad))/radian;
	anom = (eday - (elem.t - 2415020)) * motion * radian;
	enom = anom + ecc*sin(anom);

	do {
		dele = (anom - enom + ecc * sin(enom)) /
			(1 - ecc*cos(enom));
		enom += dele;
	} while(fabs(dele) > converge);

	vnom = 2*atan2(
		sqrt((1+ecc)/(1-ecc))*sin(enom/2),
		cos(enom/2));
	rad = mrad*(1-ecc*cos(enom));
	lambda = vnom + argp;
	pturbl = 0;
	lambda += pturbl*radsec;
	pturbb = 0;
	pturbr = 0;

/*
 *	reduce to the ecliptic
 */
	nd = lambda - node;
	lambda = node + atan2(sin(nd)*cos(incl),cos(nd));

	sl = sin(incl)*sin(nd) + pturbb*radsec;
	beta = atan2(sl, sqrt(1-sl*sl));

	lograd = pturbr*2.30258509;
	rad *= 1 + lograd;

	motion *= radian*mrad*mrad/(rad*rad);
	semi = 0;

	mag = 5.47 + 6.1/2.303*log(rad);

	helio();
	geo();
}
