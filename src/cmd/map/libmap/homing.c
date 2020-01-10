#include <u.h>
#include <libc.h>
#include "map.h"

static struct coord p0;		/* standard parallel */

int first;

static double
trigclamp(double x)
{
	return x>1? 1: x<-1? -1: x;
}

static struct coord az;		/* azimuth of p0 seen from place */
static struct coord rad;	/* angular dist from place to p0 */

static int
azimuth(struct place *place)
{
	if(place->nlat.c < FUZZ) {
		az.l = PI/2 + place->nlat.l - place->wlon.l;
		sincos(&az);
		rad.l = fabs(place->nlat.l - p0.l);
		if(rad.l > PI)
			rad.l = 2*PI - rad.l;
		sincos(&rad);
		return 1;
	}
	rad.c = trigclamp(p0.s*place->nlat.s +	/* law of cosines */
		p0.c*place->nlat.c*place->wlon.c);
	rad.s = sqrt(1 - rad.c*rad.c);
	if(fabs(rad.s) < .001) {
		az.s = 0;
		az.c = 1;
	} else {
		az.s = trigclamp(p0.c*place->wlon.s/rad.s); /* sines */
		az.c = trigclamp((p0.s - rad.c*place->nlat.s)
				/(rad.s*place->nlat.c));
	}
	rad.l = atan2(rad.s, rad.c);
	return 1;
}

static int
Xmecca(struct place *place, double *x, double *y)
{
	if(!azimuth(place))
		return 0;
	*x = -place->wlon.l;
	*y = fabs(az.s)<.02? -az.c*rad.s/p0.c: *x*az.c/az.s;
	return fabs(*y)>2? -1:
	       rad.c<0? 0:
	       1;
}

proj
mecca(double par)
{
	first = 1;
	if(fabs(par)>80.)
		return(0);
	deg2rad(par,&p0);
	return(Xmecca);
}

static int
Xhoming(struct place *place, double *x, double *y)
{
	if(!azimuth(place))
		return 0;
	*x = -rad.l*az.s;
	*y = -rad.l*az.c;
	return place->wlon.c<0? 0: 1;
}

proj
homing(double par)
{
	first = 1;
	if(fabs(par)>80.)
		return(0);
	deg2rad(par,&p0);
	return(Xhoming);
}

int
hlimb(double *lat, double *lon, double res)
{
	if(first) {
		*lon = -90;
		*lat = -90;
		first = 0;
		return 0;
	}
	*lat += res;
	if(*lat <= 90)
		return 1;
	if(*lon == 90)
		return -1;
	*lon = 90;
	*lat = -90;
	return 0;
}

int
mlimb(double *lat, double *lon, double res)
{
	int ret = !first;
	if(fabs(p0.s) < .01)
		return -1;
	if(first) {
		*lon = -180;
		first = 0;
	} else
		*lon += res;
	if(*lon > 180)
		return -1;
	*lat = atan(-cos(*lon*RAD)/p0.s*p0.c)/RAD;
	return ret;
}
