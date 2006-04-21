#include <u.h>
#include <libc.h>
#include <stdio.h>
#include "map.h"

static double cirmod(double);

static struct place pole;	/* map pole is tilted to here */
static struct coord twist;	/* then twisted this much */
static struct place ipole;	/* inverse transfrom */
static struct coord itwist;

void
orient(double lat, double lon, double theta)
{
	lat = cirmod(lat);
	if(lat>90.) {
		lat = 180. - lat;
		lon -= 180.;
		theta -= 180.;
	} else if(lat < -90.) {
		lat = -180. - lat;
		lon -= 180.;
		theta -= 180;
	}
	latlon(lat,lon,&pole);
	deg2rad(theta, &twist);
	latlon(lat,180.-theta,&ipole);
	deg2rad(180.-lon, &itwist);
}

void
latlon(double lat, double lon, struct place *p)
{
	lat = cirmod(lat);
	if(lat>90.) {
		lat = 180. - lat;
		lon -= 180.;
	} else if(lat < -90.) {
		lat = -180. - lat;
		lon -= 180.;
	}
	deg2rad(lat,&p->nlat);
	deg2rad(lon,&p->wlon);
}

void
deg2rad(double theta, struct coord *coord)
{
	theta = cirmod(theta);
	coord->l = theta*RAD;
	if(theta==90) {
		coord->s = 1;
		coord->c = 0;
	} else if(theta== -90) {
		coord->s = -1;
		coord->c = 0;
	} else
		sincos(coord);
}

static double
cirmod(double theta)
{
	while(theta >= 180.)
		theta -= 360;
	while(theta<-180.)
		theta += 360.;
	return(theta);
}

void
sincos(struct coord *coord)
{
	coord->s = sin(coord->l);
	coord->c = cos(coord->l);
}

void
normalize(struct place *gg)
{
	norm(gg,&pole,&twist);
}

void
invert(struct place *g)
{
	norm(g,&ipole,&itwist);
}

void
norm(struct place *gg, struct place *pp, struct coord *tw)
{
	register struct place *g;	/*geographic coords */
	register struct place *p;	/* new pole in old coords*/
	struct place m;			/* standard map coords*/
	g = gg;
	p = pp;
	if(p->nlat.s == 1.) {
		if(p->wlon.l+tw->l == 0.)
			return;
		g->wlon.l -= p->wlon.l+tw->l;
	} else {
		if(p->wlon.l != 0) {
			g->wlon.l -= p->wlon.l;
			sincos(&g->wlon);
		}
		m.nlat.s = p->nlat.s * g->nlat.s
			+ p->nlat.c * g->nlat.c * g->wlon.c;
		m.nlat.c = sqrt(1. - m.nlat.s * m.nlat.s);
		m.nlat.l = atan2(m.nlat.s, m.nlat.c);
		m.wlon.s = g->nlat.c * g->wlon.s;
		m.wlon.c = p->nlat.c * g->nlat.s
			- p->nlat.s * g->nlat.c * g->wlon.c;
		m.wlon.l = atan2(m.wlon.s, - m.wlon.c)
			- tw->l;
		*g = m;
	}
	sincos(&g->wlon);
	if(g->wlon.l>PI)
		g->wlon.l -= 2*PI;
	else if(g->wlon.l<-PI)
		g->wlon.l += 2*PI;
}

void
printp(struct place *g)
{
printf("%.3f %.3f %.3f %.3f %.3f %.3f\n",
g->nlat.l,g->nlat.s,g->nlat.c,g->wlon.l,g->wlon.s,g->wlon.c);
}

void
copyplace(struct place *g1, struct place *g2)
{
	*g2 = *g1;
}
