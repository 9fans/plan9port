#include <u.h>
#include <libc.h>
#include "map.h"

/* Given two lat-lon pairs, find an orientation for the
   -o option of "map" that will place those two points
   on the equator of a standard projection, equally spaced
   about the prime meridian.

   -w and -l options are suggested also.

   Option -t prints out a series of
   coordinates that follows the (great circle) track
   in the original coordinate system,
   followed by ".
   This data is just right for map -t.

   Option -i inverts the map top-to-bottom.
*/
struct place pole;
struct coord twist;
int track;
int inv = -1;

extern void doroute(double, double, double, double, double);

void
dorot(double a, double b, double *x, double *y, void (*f)(struct place *))
{
	struct place g;
	deg2rad(a,&g.nlat);
	deg2rad(b,&g.wlon);
	(*f)(&g);
	*x = g.nlat.l/RAD;
	*y = g.wlon.l/RAD;
}

void
rotate(double a, double b, double *x, double *y)
{
	dorot(a,b,x,y,normalize);
}

void
rinvert(double a, double b, double *x, double *y)
{
	dorot(a,b,x,y,invert);
}

main(int argc, char **argv)
{
#pragma ref argv
	double an,aw,bn,bw;
	ARGBEGIN {
	case 't':
		track = 1;
		break;

	case 'i':
		inv = 1;
		break;

	default:
		exits("route: bad option");
	} ARGEND;
	if (argc<4) {
		print("use route [-t] [-i] lat lon lat lon\n");
		exits("arg count");
	}
	an = atof(argv[0]);
	aw = atof(argv[1]);
	bn = atof(argv[2]);
	bw = atof(argv[3]);
	doroute(inv*90.,an,aw,bn,bw);
	return 0;
}

void
doroute(double dir, double an, double aw, double bn, double bw)
{
	double an1,aw1,bn1,bw1,pn,pw;
	double theta;
	double cn,cw,cn1,cw1;
	int i,n;
	orient(an,aw,0.);
	rotate(bn,bw,&bn1,&bw1);
/*	printf("b %f %f\n",bn1,bw1);*/
	orient(an,aw,bw1);
	rinvert(0.,dir,&pn,&pw);
/*	printf("p %f %f\n",pn,pw);*/
	orient(pn,pw,0.);
	rotate(an,aw,&an1,&aw1);
	rotate(bn,bw,&bn1,&bw1);
	theta = (aw1+bw1)/2;
/*	printf("a %f %f \n",an1,aw1);*/
	orient(pn,pw,theta);
	rotate(an,aw,&an1,&aw1);
	rotate(bn,bw,&bn1,&bw1);
	if(fabs(aw1-bw1)>180)
		if(theta<0.) theta+=180;
		else theta -= 180;
	orient(pn,pw,theta);
	rotate(an,aw,&an1,&aw1);
	rotate(bn,bw,&bn1,&bw1);
	if(!track) {
		double dlat, dlon, t;
		/* printf("A %.4f %.4f\n",an1,aw1); */
		/* printf("B %.4f %.4f\n",bn1,bw1); */
		cw1 = fabs(bw1-aw1);	/* angular difference for map margins */
		/* while (aw<0.0)
			aw += 360.;
		while (bw<0.0)
			bw += 360.; */
		dlon = fabs(aw-bw);
		if (dlon>180)
			dlon = 360-dlon;
		dlat = fabs(an-bn);
		printf("-o %.4f %.4f %.4f -w %.2f %.2f %.2f %.2f \n",
		  pn,pw,theta, -0.3*cw1, .3*cw1, -.6*cw1, .6*cw1);

	} else {
		cn1 = 0;
		n = 1 + fabs(bw1-aw1)/.2;
		for(i=0;i<=n;i++) {
			cw1 = aw1 + i*(bw1-aw1)/n;
			rinvert(cn1,cw1,&cn,&cw);
			printf("%f %f\n",cn,cw);
		}
		printf("\"\n");
	}
}
