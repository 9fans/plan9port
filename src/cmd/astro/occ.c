#include "astro.h"

Occ	 o1, o2;
Obj2	 xo1, xo2;

void
occult(Obj2 *p1, Obj2 *p2, double d)
{
	int i, i1, N;
	double d1, d2, d3, d4;
	double x, di, dx, x1;

	USED(d);

	d3 = 0;
	d2 = 0;
	occ.t1 = -100;
	occ.t2 = -100;
	occ.t3 = -100;
	occ.t4 = -100;
	occ.t5 = -100;
	for(i=0; i<=NPTS+1; i++) {
		d1 = d2;
		d2 = d3;
		d3 = dist(&p1->point[i], &p2->point[i]);
		if(i >= 2 && d2 <= d1 && d2 <= d3)
			goto found;
	}
	return;

found:
	N = 2880*PER/NPTS; /* 1 min steps */
	i -= 2;
	set3pt(p1, i, &o1);
	set3pt(p2, i, &o2);

	di = i;
	x = 0;
	dx = 2./N;
	for(i=0; i<=N; i++) {
		setpt(&o1, x);
		setpt(&o2, x);
		d1 = d2;
		d2 = d3;
		d3 = dist(&o1.act, &o2.act);
		if(i >= 2 && d2 <= d1 && d2 <= d3)
			goto found1;
		x += dx;
	}
	fprint(2, "bad 1 \n");
	return;

found1:
	if(d2 > o1.act.semi2+o2.act.semi2+50)
		return;
	di += x - 3*dx;
	x = 0;
	for(i=0; i<3; i++) {
		setime(day + deld*(di + x));
		(*p1->obj)();
		setobj(&xo1.point[i]);
		(*p2->obj)();
		setobj(&xo2.point[i]);
		x += 2*dx;
	}
	dx /= 60;
	x = 0;
	set3pt(&xo1, 0, &o1);
	set3pt(&xo2, 0, &o2);
	for(i=0; i<=240; i++) {
		setpt(&o1, x);
		setpt(&o2, x);
		d1 = d2;
		d2 = d3;
		d3 = dist(&o1.act, &o2.act);
		if(i >= 2 && d2 <= d1 && d2 <= d3)
			goto found2;
		x += 1./120.;
	}
	fprint(2, "bad 2 \n");
	return;

found2:
	if(d2 > o1.act.semi2 + o2.act.semi2)
		return;
	i1 = i-1;
	x1 = x - 1./120.;
	occ.t3 = di + i1 * dx;
	occ.e3 = o1.act.el;
	d3 = o1.act.semi2 - o2.act.semi2;
	if(d3 < 0)
		d3 = -d3;
	d4 = o1.act.semi2 + o2.act.semi2;
	for(i=i1,x=x1;; i++) {
		setpt(&o1, x);
		setpt(&o2, x);
		d1 = d2;
		d2 = dist(&o1.act, &o2.act);
		if(i != i1) {
			if(d1 <= d3 && d2 > d3) {
				occ.t4 = di + (i-.5) * dx;
				occ.e4 = o1.act.el;
			}
			if(d2 > d4) {
				if(d1 <= d4) {
					occ.t5 = di + (i-.5) * dx;
					occ.e5 = o1.act.el;
				}
				break;
			}
		}
		x += 1./120.;
	}
	for(i=i1,x=x1;; i--) {
		setpt(&o1, x);
		setpt(&o2, x);
		d1 = d2;
		d2 = dist(&o1.act, &o2.act);
		if(i != i1) {
			if(d1 <= d3 && d2 > d3) {
				occ.t2 = di + (i+.5) * dx;
				occ.e2 = o1.act.el;
			}
			if(d2 > d4) {
				if(d1 <= d4) {
					occ.t1 = di + (i+.5) * dx;
					occ.e1 = o1.act.el;
				}
				break;
			}
		}
		x -= 1./120.;
	}
}

void
setpt(Occ *o, double x)
{
	double y;

	y = x * (x-1);
	o->act.ra = o->del0.ra +
		x*o->del1.ra + y*o->del2.ra;
	o->act.decl2 = o->del0.decl2 +
		x*o->del1.decl2 + y*o->del2.decl2;
	o->act.semi2 = o->del0.semi2 +
		x*o->del1.semi2 + y*o->del2.semi2;
	o->act.el = o->del0.el +
		x*o->del1.el + y*o->del2.el;
}


double
pinorm(double a)
{

	while(a < -pi)
		a += pipi;
	while(a > pi)
		a -= pipi;
	return a;
}

void
set3pt(Obj2 *p, int i, Occ *o)
{
	Obj1 *p1, *p2, *p3;
	double a;

	p1 = p->point+i;
	p2 = p1+1;
	p3 = p2+1;

	o->del0.ra = p1->ra;
	o->del0.decl2 = p1->decl2;
	o->del0.semi2 = p1->semi2;
	o->del0.el = p1->el;

	a = p2->ra - p1->ra;
	o->del1.ra = pinorm(a);
	a = p2->decl2 - p1->decl2;
	o->del1.decl2 = pinorm(a);
	o->del1.semi2 = p2->semi2 - p1->semi2;
	o->del1.el = p2->el - p1->el;

	a = p1->ra + p3->ra - 2*p2->ra;
	o->del2.ra = pinorm(a)/2;
	a = p1->decl2 + p3->decl2 - 2*p2->decl2;
	o->del2.decl2 = pinorm(a)/2;
	o->del2.semi2 = (p1->semi2 + p3->semi2 - 2*p2->semi2) / 2;
	o->del2.el = (p1->el + p3->el - 2*p2->el) / 2;
}
