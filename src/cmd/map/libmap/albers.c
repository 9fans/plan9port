#include <u.h>
#include <libc.h>
#include "map.h"

/* For Albers formulas see Deetz and Adams "Elements of Map Projection", */
/* USGS Special Publication No. 68, GPO 1921 */

static double r0sq, r1sq, d2, n, den, sinb1, sinb2;
static struct coord plat1, plat2;
static int southpole;

static double num(double s)
{
	if(d2==0)
		return(1);
	s = d2*s*s;
	return(1+s*(2./3+s*(3./5+s*(4./7+s*5./9))));
}

/* Albers projection for a spheroid, good only when N pole is fixed */

static int
Xspalbers(struct place *place, double *x, double *y)
{
	double r = sqrt(r0sq-2*(1-d2)*place->nlat.s*num(place->nlat.s)/n);
	double t = n*place->wlon.l;
	*y = r*cos(t);
	*x = -r*sin(t);
	if(!southpole)
		*y = -*y;
	else
		*x = -*x;
	return(1);
}

/* lat1, lat2: std parallels; e2: squared eccentricity */

static proj albinit(double lat1, double lat2, double e2)
{
	double r1;
	double t;
	for(;;) {
		if(lat1 < -90)
			lat1 = -180 - lat1;
		if(lat2 > 90)
			lat2 = 180 - lat2;
		if(lat1 <= lat2)
			break;
		t = lat1; lat1 = lat2; lat2 = t;
	}
	if(lat2-lat1 < 1) {
		if(lat1 > 89)
			return(azequalarea());
		return(0);
	}
	if(fabs(lat2+lat1) < 1)
		return(cylequalarea(lat1));
	d2 = e2;
	den = num(1.);
	deg2rad(lat1,&plat1);
	deg2rad(lat2,&plat2);
	sinb1 = plat1.s*num(plat1.s)/den;
	sinb2 = plat2.s*num(plat2.s)/den;
	n = (plat1.c*plat1.c/(1-e2*plat1.s*plat1.s) -
	    plat2.c*plat2.c/(1-e2*plat2.s*plat2.s)) /
	    (2*(1-e2)*den*(sinb2-sinb1));
	r1 = plat1.c/(n*sqrt(1-e2*plat1.s*plat1.s));
	r1sq = r1*r1;
	r0sq = r1sq + 2*(1-e2)*den*sinb1/n;
	southpole = lat1<0 && plat2.c>plat1.c;
	return(Xspalbers);
}

proj
sp_albers(double lat1, double lat2)
{
	return(albinit(lat1,lat2,EC2));
}

proj
albers(double lat1, double lat2)
{
	return(albinit(lat1,lat2,0.));
}

static double scale = 1;
static double twist = 0;

void
albscale(double x, double y, double lat, double lon)
{
	struct place place;
	double alat, alon, x1,y1;
	scale = 1;
	twist = 0;
	invalb(x,y,&alat,&alon);
	twist = lon - alon;
	deg2rad(lat,&place.nlat);
	deg2rad(lon,&place.wlon);
	Xspalbers(&place,&x1,&y1);
	scale = sqrt((x1*x1+y1*y1)/(x*x+y*y));
}

void
invalb(double x, double y, double *lat, double *lon)
{
	int i;
	double sinb_den, sinp;
	x *= scale;
	y *= scale;
	*lon = atan2(-x,fabs(y))/(RAD*n) + twist;
	sinb_den = (r0sq - x*x - y*y)*n/(2*(1-d2));
	sinp = sinb_den;
	for(i=0; i<5; i++)
		sinp = sinb_den/num(sinp);
	*lat = asin(sinp)/RAD;
}
