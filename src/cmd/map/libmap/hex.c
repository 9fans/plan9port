#include <u.h>
#include <libc.h>
#include "map.h"

#define BIG 1.e15
#define HFUZZ .0001

static double hcut[3] ;
static double kr[3] = { .5, -1., .5 };
static double ki[3] = { -1., 0., 1. }; 	/*to multiply by sqrt(3)/2*/
static double cr[3];
static double ci[3];
static struct place hem;
static struct coord twist;
static double  rootroot3, hkc;
static double w2;
static double rootk;

static void
reflect(int i, double wr, double wi, double *x, double *y)
{
	double pr,pi,l;
	pr = cr[i]-wr;
	pi = ci[i]-wi;
	l = 2*(kr[i]*pr + ki[i]*pi);
	*x = wr + l*kr[i];
	*y = wi + l*ki[i];
}

static int
Xhex(struct place *place, double *x, double *y)
{
	int ns;
	int i;
	double zr,zi;
	double sr,si,tr,ti,ur,ui,vr,vi,yr,yi;
	struct place p;
	copyplace(place,&p);
	ns = place->nlat.l >= 0;
	if(!ns) {
		p.nlat.l = -p.nlat.l;
		p.nlat.s = -p.nlat.s;
	}
	if(p.nlat.l<HFUZZ) {
		for(i=0;i<3;i++)
			if(fabs(reduce(p.wlon.l-hcut[i]))<HFUZZ) {
				if(i==2) {
					*x = 2*cr[0] - cr[1];
					*y = 0;
				} else {
					*x = cr[1];
					*y = 2*ci[2*i];
				}
				return(1);
			}
		p.nlat.l = HFUZZ;
		sincos(&p.nlat);
	}
	norm(&p,&hem,&twist);
	Xstereographic(&p,&zr,&zi);
	zr /= 2;
	zi /= 2;
	cdiv(1-zr,-zi,1+zr,zi,&sr,&si);
	csq(sr,si,&tr,&ti);
	ccubrt(1+3*tr,3*ti,&ur,&ui);
	csqrt(ur-1,ui,&vr,&vi);
	cdiv(rootroot3+vr,vi,rootroot3-vr,-vi,&yr,&yi);
	yr /= rootk;
	yi /= rootk;
	elco2(fabs(yr),yi,hkc,1.,1.,x,y);
	if(yr < 0)
		*x = w2 - *x;
	if(!ns) reflect(hcut[0]>place->wlon.l?0:
			hcut[1]>=place->wlon.l?1:
			2,*x,*y,x,y);
	return(1);
}

proj
hex(void)
{
	int i;
	double t;
	double root3;
	double c,d;
	struct place p;
	hcut[2] = PI;
	hcut[1] = hcut[2]/3;
	hcut[0] = -hcut[1];
	root3 = sqrt(3.);
	rootroot3 = sqrt(root3);
	t = 15 -8*root3;
	hkc = t*(1-sqrt(1-1/(t*t)));
	elco2(BIG,0.,hkc,1.,1.,&w2,&t);
	w2 *= 2;
	rootk = sqrt(hkc);
	latlon(90.,90.,&hem);
	latlon(90.,0.,&p);
	Xhex(&p,&c,&t);
	latlon(0.,0.,&p);
	Xhex(&p,&d,&t);
	for(i=0;i<3;i++) {
		ki[i] *= root3/2;
		cr[i] = c + (c-d)*kr[i];
		ci[i] = (c-d)*ki[i];
	}
	deg2rad(0.,&twist);
	return(Xhex);
}

int
hexcut(struct place *g, struct place *og, double *cutlon)
{
	int t,i;
	if(g->nlat.l>=-HFUZZ&&og->nlat.l>=-HFUZZ)
		return(1);
	for(i=0;i<3;i++) {
		t = ckcut(g,og,*cutlon=hcut[i]);
		if(t!=1) return(t);
	}
	return(1);
}
