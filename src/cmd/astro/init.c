#include "astro.h"

Obj2*	objlst[] =
{
	&osun,
	&omoon,
	&oshad,
	&omerc,
	&ovenus,
	&omars,
	&ojup,
	&osat,
	&ouran,
	&onept,
	&oplut,
	&ocomet,
	0,
};

struct	idata
{
	char*	name;
	char*	name1;
	void	(*obj)(void);
} idata[] =
{
	"The sun",	"sun",		fsun,
	"The moon",	"moon",		moon,
	"The shadow",	"shadow",	shad,
	"Mercury",	"mercury",	merc,
	"Venus",	"venus",	venus,
	"Mars",		"mars",		mars,
	"Jupiter",	"jupiter",	jup,
	"Saturn",	"saturn",	sat,
	"Uranus",	"uranus",	uran,
	"Neptune",	"neptune",	nept,
	"Pluto",	"pluto",	plut,
	"Comet",	"comet",	comet,
};

void
init(void)
{
	Obj2 *q;
	int i;

	glat = nlat - (692.74*radsec)*sin(2.*nlat)
		 + (1.16*radsec)*sin(4.*nlat);
	erad = .99832707e0 + .00167644e0*cos(2.*nlat)
		 - 0.352e-5*cos(4.*nlat)
		 + 0.001e-5*cos(6.*nlat)
		 + 0.1568e-6*elev;

	for(i=0; q=objlst[i]; i++) {
		q->name = idata[i].name;
		q->name1 = idata[i].name1;
		q->obj = idata[i].obj;
	}
	ostar.obj = fstar;
	ostar.name = "star";
}

void
setime(double d)
{
	double x, xm, ym, zm;

	eday = d + deltat/86400.;
	wlong = awlong + 15.*deltat*radsec;

	capt = eday/36524.220e0;
	capt2 = capt*capt;
	capt3 = capt*capt2;
	nutate();
	eday += .1;
	sun();
	srad = rad;
	xm = rad*cos(beta)*cos(lambda);
	ym = rad*cos(beta)*sin(lambda);
	zm = rad*sin(beta);
	eday -= .1;
	sun();
	xms = rad*cos(beta)*cos(lambda);
	yms = rad*cos(beta)*sin(lambda);
	zms = rad*sin(beta);
	x = .057756;
	xdot = x*(xm-xms);
	ydot = x*(ym-yms);
	zdot = x*(zm-zms);
}

void
setobj(Obj1 *op)
{
	Obj1 *p;

	p = op;
	p->ra = ra;
	p->decl2 = decl2;
	p->semi2 = semi2;
	p->az = az;
	p->el = el;
	p->mag = mag;
}

long	starsao = 0;

void
fstar(void)
{

	ra = ostar.point[0].ra;
	decl2 = ostar.point[0].decl2;
	semi2 = ostar.point[0].semi2;
	az = ostar.point[0].az;
	el = ostar.point[0].el;
	mag = ostar.point[0].mag;
}

void
fsun(void)
{

	beta = 0;
	rad = 0;
	lambda = 0;
	motion = 0;
	helio();
	geo();
	seday = eday;
	salph = alpha;
	sdelt = delta;
	mag = lmb2;
}

void
shad(void)
{

	if(seday != eday)
		fsun();
	if(meday != eday)
		moon();
	alpha = fmod(salph+pi, pipi);
	delta = -sdelt;
	hp = mhp;
	semi = 1.0183*mhp/radsec - 969.85/srad;
	geo();
}
