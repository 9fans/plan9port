#include <u.h>
#include <libc.h>
#include "map.h"

static struct place gywhem, gyehem;
static struct coord gytwist;
static double gyconst, gykc, gyside;


static void
dosquare(double z1, double z2, double *x, double *y)
{
	double w1,w2;
	w1 = z1 -1;
	if(fabs(w1*w1+z2*z2)>.000001) {
		cdiv(z1+1,z2,w1,z2,&w1,&w2);
		w1 *= gyconst;
		w2 *= gyconst;
		if(w1<0)
			w1 = 0;
		elco2(w1,w2,gykc,1.,1.,x,y);
	} else {
		*x = gyside;
		*y = 0;
	}
}

int
Xguyou(struct place *place, double *x, double *y)
{
	int ew;		/*which hemisphere*/
	double z1,z2;
	struct place pl;
	ew = place->wlon.l<0;
	copyplace(place,&pl);
	norm(&pl,ew?&gyehem:&gywhem,&gytwist);
	Xstereographic(&pl,&z1,&z2);
	dosquare(z1/2,z2/2,x,y);
	if(!ew)
		*x -= gyside;
	return(1);
}

proj
guyou(void)
{
	double junk;
	gykc = 1/(3+2*sqrt(2.));
	gyconst = -(1+sqrt(2.));
	elco2(-gyconst,0.,gykc,1.,1.,&gyside,&junk);
	gyside *= 2;
	latlon(0.,90.,&gywhem);
	latlon(0.,-90.,&gyehem);
	deg2rad(0.,&gytwist);
	return(Xguyou);
}

int
guycut(struct place *g, struct place *og, double *cutlon)
{
	int c;
	c = picut(g,og,cutlon);
	if(c!=1)
		return(c);
	*cutlon = 0.;
	if(g->nlat.c<.7071||og->nlat.c<.7071)
		return(ckcut(g,og,0.));
	return(1);
}

static int
Xsquare(struct place *place, double *x, double *y)
{
	double z1,z2;
	double r, theta;
	struct place p;
	copyplace(place,&p);
	if(place->nlat.l<0) {
		p.nlat.l = -p.nlat.l;
		p.nlat.s = -p.nlat.s;
	}
	if(p.nlat.l<FUZZ && fabs(p.wlon.l)>PI-FUZZ){
		*y = -gyside/2;
		*x = p.wlon.l>0?0:gyside;
		return(1);
	}
	Xstereographic(&p,&z1,&z2);
	r = sqrt(sqrt(hypot(z1,z2)/2));
	theta = atan2(z1,-z2)/4;
	dosquare(r*sin(theta),-r*cos(theta),x,y);
	if(place->nlat.l<0)
		*y = -gyside - *y;
	return(1);
}

proj
square(void)
{
	guyou();
	return(Xsquare);
}
