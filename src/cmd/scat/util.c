#include <u.h>
#include <libc.h>
#include <bio.h>
#include "sky.h"

double	PI_180	= 0.0174532925199432957692369;
double	TWOPI	= 6.2831853071795864769252867665590057683943387987502;
double	LN2	= 0.69314718055994530941723212145817656807550013436025;
static double angledangle=(180./PI)*MILLIARCSEC;

#define rint scatrint

int
rint(char *p, int n)
{
	int i=0;

	while(*p==' ' && n)
		p++, --n;
	while(n--)
		i=i*10+*p++-'0';
	return i;
}

DAngle
dangle(Angle angle)
{
	return angle*angledangle;
}

Angle
angle(DAngle dangle)
{
	return dangle/angledangle;
}

double
rfloat(char *p, int n)
{
	double i, d=0;

	while(*p==' ' && n)
		p++, --n;
	if(*p == '+')
		return rfloat(p+1, n-1);
	if(*p == '-')
		return -rfloat(p+1, n-1);
	while(*p == ' ' && n)
		p++, --n;
	if(n == 0)
		return 0.0;
	while(n-- && *p!='.')
		d = d*10+*p++-'0';
	if(n <= 0)
		return d;
	p++;
	i = 1;
	while(n--)
		d+=(*p++-'0')/(i*=10.);
	return d;
}

int
sign(int c)
{
	if(c=='-')
		return -1;
	return 1;
}

char*
hms(Angle a)
{
	static char buf[20];
	double x;
	int h, m, s, ts;

	x=DEG(a)/15;
	x += 0.5/36000.;	/* round up half of 0.1 sec */
	h = floor(x);
	x -= h;
	x *= 60;
	m = floor(x);
	x -= m;
	x *= 60;
	s = floor(x);
	x -= s;
	ts = 10*x;
	sprint(buf, "%dh%.2dm%.2d.%ds", h, m, s, ts);
	return buf;
}

char*
dms(Angle a)
{
	static char buf[20];
	double x;
	int sign, d, m, s, ts;

	x = DEG(a);
	sign='+';
	if(a<0){
		sign='-';
		x=-x;
	}
	x += 0.5/36000.;	/* round up half of 0.1 arcsecond */
	d = floor(x);
	x -= d;
	x *= 60;
	m = floor(x);
	x -= m;
	x *= 60;
	s = floor(x);
	x -= s;
	ts = floor(10*x);
	sprint(buf, "%c%d°%.2d'%.2d.%d\"", sign, d, m, s, ts);
	return buf;
}

char*
ms(Angle a)
{
	static char buf[20];
	double x;
	int d, m, s, ts;

	x = DEG(a);
	x += 0.5/36000.;	/* round up half of 0.1 arcsecond */
	d = floor(x);
	x -= d;
	x *= 60;
	m = floor(x);
	x -= m;
	x *= 60;
	s = floor(x);
	x -= s;
	ts = floor(10*x);
	if(d != 0)
		sprint(buf, "%d°%.2d'%.2d.%d\"", d, m, s, ts);
	else
		sprint(buf, "%.2d'%.2d.%d\"", m, s, ts);
	return buf;
}

char*
hm(Angle a)
{
	static char buf[20];
	double x;
	int h, m, n;

	x = DEG(a)/15;
	x += 0.5/600.;	/* round up half of tenth of minute */
	h = floor(x);
	x -= h;
	x *= 60;
	m = floor(x);
	x -= m;
	x *= 10;
	n = floor(x);
	sprint(buf, "%dh%.2d.%1dm", h, m, n);
	return buf;
}

char*
hm5(Angle a)
{
	static char buf[20];
	double x;
	int h, m;

	x = DEG(a)/15;
	x += 2.5/60.;	/* round up 2.5m */
	h = floor(x);
	x -= h;
	x *= 60;
	m = floor(x);
	m -= m % 5;
	sprint(buf, "%dh%.2dm", h, m);
	return buf;
}

char*
dm(Angle a)
{
	static char buf[20];
	double x;
	int sign, d, m, n;

	x = DEG(a);
	sign='+';
	if(a<0){
		sign='-';
		x=-x;
	}
	x += 0.5/600.;	/* round up half of tenth of arcminute */
	d = floor(x);
	x -= d;
	x *= 60;
	m = floor(x);
	x -= m;
	x *= 10;
	n = floor(x);
	sprint(buf, "%c%d°%.2d.%.1d'", sign, d, m, n);
	return buf;
}

char*
deg(Angle a)
{
	static char buf[20];
	double x;
	int sign, d;

	x = DEG(a);
	sign='+';
	if(a<0){
		sign='-';
		x=-x;
	}
	x += 0.5;	/* round up half degree */
	d = floor(x);
	sprint(buf, "%c%d°", sign, d);
	return buf;
}

char*
getword(char *ou, char *in)
{
	int c;

	for(;;) {
		c = *in++;
		if(c == ' ' || c == '\t')
			continue;
		if(c == 0)
			return 0;
		break;
	}

	if(c == '\'')
		for(;;) {
			if(c >= 'A' && c <= 'Z')
				c += 'a' - 'A';
			*ou++ = c;
			c = *in++;
			if(c == 0)
				return 0;
			if(c == '\'') {
				*ou = 0;
				return in-1;
			}
		}
	for(;;) {
		if(c >= 'A' && c <= 'Z')
			c += 'a' - 'A';
		*ou++ = c;
		c = *in++;
		if(c == ' ' || c == '\t' || c == 0) {
			*ou = 0;
			return in-1;
		}
	}
}

/*
 * Read formatted angle.  Must contain no embedded blanks
 */
Angle
getra(char *p)
{
	Rune r;
	char *q;
	Angle f, d;
	int neg;

	neg = 0;
	d = 0;
	while(*p == ' ')
		p++;
	for(;;) {
		if(*p == ' ' || *p=='\0')
			goto Return;
		if(*p == '-') {
			neg = 1;
			p++;
		}
		if(*p == '+') {
			neg = 0;
			p++;
		}
		q = p;
		f = strtod(p, &q);
		if(q > p) {
			p = q;
		}
		p += chartorune(&r, p);
		switch(r) {
		default:
		Return:
			if(neg)
				d = -d;
			return RAD(d);
		case 'h':
			d += f*15;
			break;
		case 'm':
			d += f/4;
			break;
		case 's':
			d += f/240;
			break;
		case 0xB0:	/* ° */
			d += f;
			break;
		case '\'':
			d += f/60;
			break;
		case '\"':
			d += f/3600;
			break;
		}
	}
	return 0;
}

double
xsqrt(double a)
{

	if(a < 0)
		return 0;
	return sqrt(a);
}

Angle
dist(Angle ra1, Angle dec1, Angle ra2, Angle dec2)
{
	double a;

	a = sin(dec1) * sin(dec2) +
		cos(dec1) * cos(dec2) *
		cos(ra1 - ra2);
	a = atan2(xsqrt(1 - a*a), a);
	if(a < 0)
		a = -a;
	return a;
}

int
dogamma(Pix c)
{
	float f;

	f = c - gam.min;
	if(f < 1)
		f = 1;

	if(gam.absgamma == 1)
		c = f * gam.mult2;
	else
		c = exp(log(f*gam.mult1) * gam.absgamma) * 255;
	if(c > 255)
		c = 255;
	if(gam.neg)
		c = 255-c;
	return c;
}
