#include "astro.h"

double
dist(Obj1 *p, Obj1 *q)
{
	double a;

	a = sin(p->decl2)*sin(q->decl2) +
		cos(p->decl2)*cos(q->decl2)*cos(p->ra-q->ra);
	a = fabs(atan2(pyth(a), a)) / radsec;
	return a;
}

int
rline(int f)
{
	char *p;
	int c;
	static char buf[1024];
	static int bc, bn, bf;

	if(bf != f) {
		bf = f;
		bn = 0;
	}
	p = line;
	do {
		if(bn <= 0) {
			bn = read(bf, buf, sizeof(buf));
			if(bn <= 0)
				return 1;
			bc = 0;
		}
		c = buf[bc];
		bn--; bc++;
		*p++ = c;
	} while(c != '\n');
	return 0;
}

double
sunel(double t)
{
	int i;

	i = floor(t);
	if(i < 0 || i > NPTS+1)
		return -90;
	t = osun.point[i].el +
		(t-i)*(osun.point[i+1].el - osun.point[i].el);
	return t;
}

double
rise(Obj2 *op, double el)
{
	Obj2 *p;
	int i;
	double e1, e2;

	e2 = 0;
	p = op;
	for(i=0; i<=NPTS; i++) {
		e1 = e2;
		e2 = p->point[i].el;
		if(i >= 1 && e1 <= el && e2 > el)
			goto found;
	}
	return -1;

found:
	return i - 1 + (el-e1)/(e2-e1);
}

double
set(Obj2 *op, double el)
{
	Obj2 *p;
	int i;
	double e1, e2;

	e2 = 0;
	p = op;
	for(i=0; i<=NPTS; i++) {
		e1 = e2;
		e2 = p->point[i].el;
		if(i >= 1 && e1 > el && e2 <= el)
			goto found;
	}
	return -1;

found:
	return i - 1 + (el-e1)/(e2-e1);
}

double
solstice(int n)
{
	int i;
	double d1, d2, d3;

	d3 = (n*pi)/2 - pi;
	if(n == 0)
		d3 += pi;
	d2 = 0.;
	for(i=0; i<=NPTS; i++) {
		d1 = d2;
		d2 = osun.point[i].ra;
		if(n == 0) {
			d2 -= pi;
			if(d2 < -pi)
				d2 += pipi;
		}
		if(i >= 1 && d3 >= d1 && d3 < d2)
			goto found;
	}
	return -1;

found:
	return i - (d3-d2)/(d1-d2);
}

double
betcross(double b)
{
	int i;
	double d1, d2;

	d2 = 0;
	for(i=0; i<=NPTS; i++) {
		d1 = d2;
		d2 = osun.point[i].mag;
		if(i >= 1 && b >= d1 && b < d2)
			goto found;
	}
	return -1;

found:
	return i - (b-d2)/(d1-d2);
}

double
melong(Obj2 *op)
{
	Obj2 *p;
	int i;
	double d1, d2, d3;

	d2 = 0;
	d3 = 0;
	p = op;
	for(i=0; i<=NPTS; i++) {
		d1 = d2;
		d2 = d3;
		d3 = dist(&p->point[i], &osun.point[i]);
		if(i >= 2 && d2 >= d1 && d2 >= d3)
			goto found;
	}
	return -1;

found:
	return i - 2;
}

#define	NEVENT	100
Event	events[NEVENT];
Event*	eventp = 0;

void
event(char *format, char *arg1, char *arg2, double tim, int flag)
{
	Event *p;

	if(flag & DARK)
		if(sunel(tim) > -12)
			return;
	if(flag & LIGHT)
		if(sunel(tim) < 0)
			return;
	if(eventp == 0)
		eventp = events;
	p = eventp;
	if(p >= events+NEVENT) {
		fprint(2, "too many events\n");
		return;
	}
	eventp++;
	p->format = format;
	p->arg1 = arg1;
	p->arg2 = arg2;
	p->tim = tim;
	p->flag = flag;
}

int	evcomp(const void*, const void*);

void
evflush(void)
{
	Event *p;

	if(eventp == 0)
		return;
	qsort(events, eventp-events, sizeof *p, evcomp);
	for(p = events; p<eventp; p++) {
		if((p->flag&SIGNIF) && flags['s'])
			print("ding ding ding ");
		print(p->format, p->arg1, p->arg2);
		if(p->flag & PTIME)
			ptime(day + p->tim*deld);
		print("\n");
	}
	eventp = 0;
}

int
evcomp(const void *a1, const void *a2)
{
	double t1, t2;
	Event *p1, *p2;

	p1 = (Event*)a1;
	p2 = (Event*)a2;
	t1 = p1->tim;
	t2 = p2->tim;
	if(p1->flag & SIGNIF)
		t1 -= 1000.;
	if(p2->flag & SIGNIF)
		t2 -= 1000.;
	if(t1 > t2)
		return 1;
	if(t2 > t1)
		return -1;
	return 0;
}

double
pyth(double x)
{

	x *= x;
	if(x > 1)
		x = 1;
	return sqrt(1-x);
}

char*
skip(int n)
{
	int i;
	char *cp;

	cp = line;
	for(i=0; i<n; i++) {
		while(*cp == ' ' || *cp == '\t')
			cp++;
		while(*cp != '\n' && *cp != ' ' && *cp != '\t')
			cp++;
	}
	while(*cp == ' ' || *cp == '\t')
		cp++;
	return cp;
}
