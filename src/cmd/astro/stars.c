#include "astro.h"

char*	startab;

void
stars(void)
{
	double lomoon, himoon, sd;
	int wrap, f, i;
	char *saop;
	static char saoa[100];

	sd = 1000*radsec;
	lomoon = omoon.point[0].ra - sd;
	if(lomoon < 0)
		lomoon += pipi;
	himoon = omoon.point[NPTS+1].ra + sd;
	if(himoon > pipi)
		himoon -= pipi;
	lomoon *= 12/pi;
	himoon *= 12/pi;
	wrap = 0;
	if(lomoon > himoon)
		wrap++;

	f = open(startab, OREAD);
	if(f < 0) {
		fprint(2, "%s?\n", startab);
		return;
	}
	epoch = 1950.0;
	epoch = (epoch-1900.0) * 365.24220 + 0.313;
	saop = saoa;

/*
 *	read mean places of stars at epoch of star table
 */

loop:
	if(rline(f)) {
		close(f);
		return;
	}
	rah = atof(line+17);
	ram = atof(line+20);
	ras = atof(line+23);

	alpha = rah + ram/60 + ras/3600;
	if(wrap == 0) {
		if(alpha < lomoon || alpha > himoon)
			goto loop;
	} else
		if(alpha < lomoon && alpha > himoon)
			goto loop;

	sao = atof(line+0);
	sprint(saop, "%ld", sao);
	da = atof(line+30);
	dday = atof(line+37);
	dmin = atof(line+41);
	dsec = atof(line+44);
	dd = atof(line+50);
	px = atof(line+57);
	mag = atof(line+61);

/*
 *	convert rt ascension and declination to internal format
 */

	delta = fabs(dday) + dmin/60 + dsec/3600;
	if(dday < 0)
		delta = -delta;

	star();
/*
 *	if(fabs(beta) > 6.55*radian)
 *		goto loop;
 */
	sd = .0896833e0*cos(beta)*sin(lambda-1.3820+.00092422117*eday)
		 + 0.99597*sin(beta);
	if(fabs(sd) > .0183)
		goto loop;

	for(i=0; i<=NPTS+1; i++)
		setobj(&ostar.point[i]);

	occult(&omoon, &ostar, 0);
	if(occ.t1 >= 0 || occ.t5 >= 0) {
		i = PTIME;
		if(mag > 2)
			i |= DARK;
		if(mag < 5)
			i |= SIGNIF;
		if(occ.t1 >= 0 && occ.e1 >= 0)
			event("Occultation of SAO %s begins at ",
				saop, "", occ.t1, i);
		if(occ.t5 >= 0 && occ.e5 >= 0)
			event("Occultation of SAO %s ends at ",
				saop, "", occ.t5, i);
		while(*saop++)
			;
	}
	goto loop;
}
