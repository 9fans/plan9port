#include "astro.h"

char*	satlst[] =
{
	0,
};

struct
{
	double	time;
	double	tilt;
	double	pnni;
	double	psi;
	double	ppi;
	double	d1pp;
	double	peri;
	double	d1per;
	double	e0;
	double	deo;
	double	rdp;
	double	st;
	double	ct;
	double	rot;
	double	semi;
} satl;

void
satels(void)
{
	double ifa[10], t, t1, t2, tinc;
	char **satp;
	int flag, f, i, n;

	satp = satlst;

loop:
	if(*satp == 0)
		return;
	f = open(*satp, 0);
	if(f < 0) {
		fprint(2, "cannot open %s\n", *satp);
		satp += 2;
		goto loop;
	}
	satp++;
	rline(f);
	tinc = atof(skip(6));
	rline(f);
	rline(f);
	for(i=0; i<9; i++)
		ifa[i] = atof(skip(i));
	n = ifa[0];
	i = ifa[1];
	t = dmo[i-1] + ifa[2] - 1.;
	if(n%4 == 0 && i > 2)
		t += 1.;
	for(i=1970; i<n; i++) {
		t += 365.;
		if(i%4 == 0)
			t += 1.;
	}
	t = (t * 24. + ifa[3]) * 60. + ifa[4];
	satl.time = t * 60.;
	satl.tilt = ifa[5] * radian;
	satl.pnni = ifa[6] * radian;
	satl.psi = ifa[7];
	satl.ppi = ifa[8] * radian;
	rline(f);
	for(i=0; i<5; i++)
		ifa[i] = atof(skip(i));
	satl.d1pp = ifa[0] * radian;
	satl.peri = ifa[1];
	satl.d1per = ifa[2];
	satl.e0 = ifa[3];
	satl.deo = 0;
	satl.rdp = ifa[4];

	satl.st = sin(satl.tilt);
	satl.ct = cos(satl.tilt);
	satl.rot = pipi / (1440. + satl.psi);
	satl.semi = satl.rdp * (1. + satl.e0);

	n = PER*288.; /* 5 min steps */
	t = day;
	for(i=0; i<n; i++) {
		if(sunel((t-day)/deld) > 0.)
			goto out;
		satel(t);
		if(el > 0) {
			t1 = t;
			flag = 0;
			do {
				if(el > 30.)
					flag++;
				t -= tinc/(24.*3600.);
				satel(t);
			} while(el > 0.);
			t2 = (t - day)/deld;
			t = t1;
			do {
				t += tinc/(24.*3600.);
				satel(t);
				if(el > 30.)
					flag++;
			} while(el > 0.);
			if(flag)
				if((*satp)[0] == '-')
					event("%s pass at ", (*satp)+1, "",
						t2, SIGNIF+PTIME+DARK); else
					event("%s pass at ", *satp, "",
						t2, PTIME+DARK);
		}
	out:
		t += 5./(24.*60.);
	}
	close(f);
	satp++;
	goto loop;
}

void
satel(double time)
{
	int i;
	double amean, an, coc, csl, d, de, enom, eo;
	double pp, q, rdp, slong, ssl, t, tp;

	i = 500;
	el = -1;
	time = (time-25567.5) * 86400;
	t = (time - satl.time)/60;
	if(t < 0)
		return; /* too early for satelites */
	an = floor(t/satl.peri);
	while(an*satl.peri + an*an*satl.d1per/2. <= t) {
		an += 1;
		if(--i == 0)
			return;
	}
	while((tp = an*satl.peri + an*an*satl.d1per/2.) > t) {
		an -= 1;
		if(--i == 0)
			return;
	}
	amean = (t-tp)/(satl.peri+(an+.5)*satl.d1per);
	pp = satl.ppi+(an+amean)*satl.d1pp;
	amean *= pipi;
	eo = satl.e0+satl.deo*an;
	rdp = satl.semi/(1+eo);
	enom = amean+eo*sin(amean);
	do {
		de = (amean-enom+eo*sin(enom))/(1.0-eo*cos(enom));
		enom += de;
		if(--i == 0)
			return;
	} while(fabs(de) >= 1.0e-6);
	q = 3963.35*erad/(rdp*(1-eo*cos(enom))/(1-eo));
	d = pp + 2*atan2(sqrt((1+eo)/(1-eo))*sin(enom/2),cos(enom/2));
	slong = satl.pnni + t*satl.rot -
		atan2(satl.ct*sin(d), cos(d));
	ssl = satl.st*sin(d);
	csl = pyth(ssl);
	if(vis(time, atan2(ssl,csl), slong, q)) {
		coc = ssl*sin(glat) + csl*cos(glat)*cos(wlong-slong);
		el = atan2(coc-q, pyth(coc));
		el /= radian;
	}
}

int
vis(double t, double slat, double slong, double q)
{
	double t0, t1, t2, d;

	d = t/86400 - .005375 + 3653;
	t0 = 6.238030674 + .01720196977*d;
	t2 = t0 + .0167253303*sin(t0);
	do {
		t1 = (t0 - t2 + .0167259152*sin(t2)) /
			(1 - .0167259152*cos(t2));
		t2 = t2 + t1;
	} while(fabs(t1) >= 1.e-4);
	t0 = 2*atan2(1.01686816*sin(t2/2), cos(t2/2));
	t0 = 4.926234925 + 8.214985538e-7*d + t0;
	t1 = 0.91744599 * sin(t0);
	t0 = atan2(t1, cos(t0));
	if(t0 < -pi/2)
		t0 = t0 + pipi;
	d = 4.88097876 + 6.30038809*d - t0;
	t0 = 0.43366079 * t1;
	t1 = pyth(t0);
	t2 = t1*cos(slat)*cos(d-slong) - t0*sin(slat);
	if(t2 > 0.46949322e-2) {
		if(0.46949322e-2*t2 + 0.999988979*pyth(t2) < q)
			return 0;
	}
	t2 = t1*cos(glat)*cos(d-wlong) - t0*sin(glat);
	if(t2 < .1)
		return 0;
	return 1;
}
