#include "astro.h"

void
star(void)
{
	double xm, ym, zm, dxm, dym, dzm;
	double xx, yx, zx, yy, zy, zz, tau;
	double capt0, capt1, capt12, capt13, sl, sb, cl;

/*
 *	remove E-terms of aberration
 *	except when finding catalog mean places
 */

	alpha += (.341/(3600.*15.))*sin((alpha+11.26)*15.*radian)
		  /cos(delta*radian);
	delta += (.341/3600.)*cos((alpha+11.26)*15.*radian)
		  *sin(delta*radian) - (.029/3600.)*cos(delta*radian);

/*
 *	correct for proper motion
 */

	tau = (eday - epoch)/365.24220;
	alpha += tau*da/3600.;
	delta += tau*dd/3600.;
	alpha *= 15.*radian;
	delta *= radian;

/*
 *	convert to rectangular coordinates merely for convenience
 */

	xm = cos(delta)*cos(alpha);
	ym = cos(delta)*sin(alpha);
	zm = sin(delta);

/*
 *	convert mean places at epoch of startable to current
 *	epoch (i.e. compute relevant precession)
 */

	capt0 = (epoch - 18262.427)/36524.220e0;
	capt1 = (eday - epoch)/36524.220;
	capt12 = capt1*capt1;
	capt13 = capt12*capt1;

	xx = - (.00029696+26.e-8*capt0)*capt12
		  - 13.e-8*capt13;
	yx =  -(.02234941+1355.e-8*capt0)*capt1
		  - 676.e-8*capt12 + 221.e-8*capt13;
	zx = -(.00971690-414.e-8*capt0)*capt1
		  + 207.e-8*capt12 + 96.e-8*capt13;
	yy = - (.00024975+30.e-8*capt0)*capt12
		  - 15.e-8*capt13;
	zy = -(.00010858+2.e-8*capt0)*capt12;
	zz = - (.00004721-4.e-8*capt0)*capt12;

	dxm =  xx*xm + yx*ym + zx*zm;
	dym = - yx*xm + yy*ym + zy*zm;
	dzm = - zx*xm + zy*ym + zz*zm;

	xm = xm + dxm;
	ym = ym + dym;
	zm = zm + dzm;

/*
 *	convert to mean ecliptic system of date
 */

	alpha = atan2(ym, xm);
	delta = atan2(zm, sqrt(xm*xm+ym*ym));
	cl = cos(delta)*cos(alpha);
	sl = cos(delta)*sin(alpha)*cos(obliq) + sin(delta)*sin(obliq);
	sb = -cos(delta)*sin(alpha)*sin(obliq) + sin(delta)*cos(obliq);
	lambda = atan2(sl, cl);
	beta = atan2(sb, sqrt(cl*cl+sl*sl));
	rad = 1.e9;
	if(px != 0)
		rad = 20600/px;
	motion = 0;
	semi = 0;

	helio();
	geo();
}
