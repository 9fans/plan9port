#include "astro.h"


char*	month[] =
{
	"January",
	"February",
	"March",
	"April",
	"May",
	"June",
	"July",
	"August",
	"September",
	"October",
	"November",
	"December",
};

double
dsrc(double d, Tim *t, int i)
{
	double y;

	do {
		t->ifa[i] += 1.;
		y = convdate(t);
	} while(d >= y);
	do {
		t->ifa[i] -= 1.;
		y = convdate(t);
	} while(d < y);
	return d - y;
}

void
dtsetup(double d, Tim *t)
{
	double v;

	t->ifa[0] = floor(1900 + d/365.24220);
	t->ifa[1] = 1;
	t->ifa[2] = 1;
	t->ifa[3] = 0;
	t->ifa[4] = 0;
	t->ifa[1] = floor(1 + dsrc(d, t, 0)/30);
	t->ifa[2] = floor(1 + dsrc(d, t, 1));
	dsrc(d, t, 2);

	v = (d - convdate(t)) * 24;
	t->ifa[3] = floor(v);
	t->ifa[4] = (v - t->ifa[3]) * 60;
	convdate(t);	/* to set timezone */
}

void
pdate(double d)
{
	int i;
	Tim t;

	dtsetup(d, &t);
	if(flags['s']) {
		i = t.ifa[1];
		print("%s ", month[i-1]);
		i = t.ifa[2];
		numb(i);
		print("...");
		return;
	}

	/* year month day */
	print("%4d %2d %2d",
		(int)t.ifa[0],
		(int)t.ifa[1],
		(int)t.ifa[2]);
}

void
ptime(double d)
{
	int h, m, s;
	char *mer;
	Tim t;

	if(flags['s']) {
		/* hour minute */
		dtsetup(d + .5/(24*60), &t);
		h = t.ifa[3];
		m = floor(t.ifa[4]);

		mer = "AM";
		if(h >= 12) {
			mer = "PM";
			h -= 12;
		}
		if(h == 0)
			h = 12;
		numb(h);
		if(m < 10) {
			if(m == 0) {
				print("%s exactly ...", mer);
				return;
			}
			print("O ");
		}
		numb(m);
		print("%s ...", mer);
		return;
	}
	/* hour minute second */
	dtsetup(d, &t);
	h = t.ifa[3];
	m = floor(t.ifa[4]);
	s = floor((t.ifa[4]-m) * 60);
	print("%.2d:%.2d:%.2d %.*s", h, m, s, utfnlen(t.tz, 3), t.tz);
}

char*	unit[] =
{
	"zero",
	"one",
	"two",
	"three",
	"four",
	"five",
	"six",
	"seven",
	"eight",
	"nine",
	"ten",
	"eleven",
	"twelve",
	"thirteen",
	"fourteen",
	"fifteen",
	"sixteen",
	"seventeen",
	"eighteen",
	"nineteen"
};
char*	decade[] =
{
	"twenty",
	"thirty",
	"forty",
	"fifty",
	"sixty",
	"seventy",
	"eighty",
	"ninety"
};

void
pstime(double d)
{

	setime(d);

	semi = 0;
	motion = 0;
	rad = 1.e9;
	lambda = 0;
	beta = 0;

// uses lambda, beta, rad, motion
// sets alpha, delta, rp

	helio();

// uses alpha, delta, rp
// sets ra, decl, lha, decl2, az, el

	geo();

	print(" %R %D %D %4.0f", lha, nlat, awlong, elev/3.28084);
}

void
numb(int n)
{

	if(n >= 100) {
		print("%d ", n);
		return;
	}
	if(n >= 20) {
		print("%s ", decade[n/10 - 2]);
		n %= 10;
		if(n == 0)
			return;
	}
	print("%s ", unit[n]);
}

double
tzone(double y, Tim *z)
{
	double t, l1, l2;
	Tm t1, t2;

	/*
	 * get a rough approximation to unix mean time
	 */
	t = (y - 25567.5) * 86400;

	/*
	 * if outside unix conversions,
	 * just call it GMT
	 */
	if(t < 0 || t > 2.1e9)
		return y;

	/*
	 * convert by both local and gmt
	 */
	t1 = *localtime((long)t);
	t2 = *gmtime((long)t);

	/*
	 * pick up year crossings
	 */
	if(t1.yday == 0 && t2.yday > 1)
		t1.yday = t2.yday+1;
	if(t2.yday == 0 && t1.yday > 1)
		t2.yday = t1.yday+1;

	/*
	 * convert times to days
	 */
	l1 = t1.yday + t1.hour/24. + t1.min/1440. + t1.sec/86400.;
	l2 = t2.yday + t2.hour/24. + t2.min/1440. + t2.sec/86400.;

	/*
	 * return difference
	 */
	strncpy(z->tz, t1.zone, sizeof(z->tz));
	return y + (l2 - l1);
}

int	dmo[12] =
{
	0,
	31,
	59,
	90,
	120,
	151,
	181,
	212,
	243,
	273,
	304,
	334
};

/*
 * input date conversion
 * output is done by zero crossing
 * on this input conversion.
 */
double
convdate(Tim *t)
{
	double y, d;
	int m;

	y = t->ifa[0];
	m = t->ifa[1];
	d = t->ifa[2];

	/*
	 * normalize the month
	 */
	while(m < 1) {
		m += 12;
		y -= 1;
	}
	while(m > 12) {
		m -= 12;
		y += 1;
	}

	/*
	 * bc correction
	 */
	if(y < 0)
		y += 1;

	/*
	 * normal conversion
	 */
	y += 4712;
	if(fmod(y, 4) == 0 && m > 2)
		d += 1;
	y = y*365 + floor((y+3)/4) + dmo[m-1] + d - 1;

	/*
	 * gregorian change
	 */
	if(y > 2361232)
		y -= floor((y-1794167)/36524.220) -
			floor((y-1721117)/146100);
	y += t->ifa[3]/24 + t->ifa[4]/1440 - 2415020.5;

	/*
	 * kitchen clock correction
	 */
	strncpy(t->tz, "GMT", sizeof(t->tz));
	if(flags['k'])
		y = tzone(y, t);
	return y;
}
