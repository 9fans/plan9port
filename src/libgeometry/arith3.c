#include <u.h>
#include <libc.h>
#include <draw.h>
#include <geometry.h>
/*
 * Routines whose names end in 3 work on points in Affine 3-space.
 * They ignore w in all arguments and produce w=1 in all results.
 * Routines whose names end in 4 work on points in Projective 3-space.
 */
Point3 add3(Point3 a, Point3 b){
	a.x+=b.x;
	a.y+=b.y;
	a.z+=b.z;
	a.w=1.;
	return a;
}
Point3 sub3(Point3 a, Point3 b){
	a.x-=b.x;
	a.y-=b.y;
	a.z-=b.z;
	a.w=1.;
	return a;
}
Point3 neg3(Point3 a){
	a.x=-a.x;
	a.y=-a.y;
	a.z=-a.z;
	a.w=1.;
	return a;
}
Point3 div3(Point3 a, double b){
	a.x/=b;
	a.y/=b;
	a.z/=b;
	a.w=1.;
	return a;
}
Point3 mul3(Point3 a, double b){
	a.x*=b;
	a.y*=b;
	a.z*=b;
	a.w=1.;
	return a;
}
int eqpt3(Point3 p, Point3 q){
	return p.x==q.x && p.y==q.y && p.z==q.z;
}
/*
 * Are these points closer than eps, in a relative sense
 */
int closept3(Point3 p, Point3 q, double eps){
	return 2.*dist3(p, q)<eps*(len3(p)+len3(q));
}
double dot3(Point3 p, Point3 q){
	return p.x*q.x+p.y*q.y+p.z*q.z;
}
Point3 cross3(Point3 p, Point3 q){
	Point3 r;
	r.x=p.y*q.z-p.z*q.y;
	r.y=p.z*q.x-p.x*q.z;
	r.z=p.x*q.y-p.y*q.x;
	r.w=1.;
	return r;
}
double len3(Point3 p){
	return sqrt(p.x*p.x+p.y*p.y+p.z*p.z);
}
double dist3(Point3 p, Point3 q){
	p.x-=q.x;
	p.y-=q.y;
	p.z-=q.z;
	return sqrt(p.x*p.x+p.y*p.y+p.z*p.z);
}
Point3 unit3(Point3 p){
	double len=sqrt(p.x*p.x+p.y*p.y+p.z*p.z);
	p.x/=len;
	p.y/=len;
	p.z/=len;
	p.w=1.;
	return p;
}
Point3 midpt3(Point3 p, Point3 q){
	p.x=.5*(p.x+q.x);
	p.y=.5*(p.y+q.y);
	p.z=.5*(p.z+q.z);
	p.w=1.;
	return p;
}
Point3 lerp3(Point3 p, Point3 q, double alpha){
	p.x+=(q.x-p.x)*alpha;
	p.y+=(q.y-p.y)*alpha;
	p.z+=(q.z-p.z)*alpha;
	p.w=1.;
	return p;
}
/*
 * Reflect point p in the line joining p0 and p1
 */
Point3 reflect3(Point3 p, Point3 p0, Point3 p1){
	Point3 a, b;
	a=sub3(p, p0);
	b=sub3(p1, p0);
	return add3(a, mul3(b, 2*dot3(a, b)/dot3(b, b)));
}
/*
 * Return the nearest point on segment [p0,p1] to point testp
 */
Point3 nearseg3(Point3 p0, Point3 p1, Point3 testp){
	double num, den;
	Point3 q, r;
	q=sub3(p1, p0);
	r=sub3(testp, p0);
	num=dot3(q, r);;
	if(num<=0) return p0;
	den=dot3(q, q);
	if(num>=den) return p1;
	return add3(p0, mul3(q, num/den));
}
/*
 * distance from point p to segment [p0,p1]
 */
#define	SMALL	1e-8	/* what should this value be? */
double pldist3(Point3 p, Point3 p0, Point3 p1){
	Point3 d, e;
	double dd, de, dsq;
	d=sub3(p1, p0);
	e=sub3(p, p0);
	dd=dot3(d, d);
	de=dot3(d, e);
	if(dd<SMALL*SMALL) return len3(e);
	dsq=dot3(e, e)-de*de/dd;
	if(dsq<SMALL*SMALL) return 0;
	return sqrt(dsq);
}
/*
 * vdiv3(a, b) is the magnitude of the projection of a onto b
 * measured in units of the length of b.
 * vrem3(a, b) is the component of a perpendicular to b.
 */
double vdiv3(Point3 a, Point3 b){
	return (a.x*b.x+a.y*b.y+a.z*b.z)/(b.x*b.x+b.y*b.y+b.z*b.z);
}
Point3 vrem3(Point3 a, Point3 b){
	double quo=(a.x*b.x+a.y*b.y+a.z*b.z)/(b.x*b.x+b.y*b.y+b.z*b.z);
	a.x-=b.x*quo;
	a.y-=b.y*quo;
	a.z-=b.z*quo;
	a.w=1.;
	return a;
}
/*
 * Compute face (plane) with given normal, containing a given point
 */
Point3 pn2f3(Point3 p, Point3 n){
	n.w=-dot3(p, n);
	return n;
}
/*
 * Compute face containing three points
 */
Point3 ppp2f3(Point3 p0, Point3 p1, Point3 p2){
	Point3 p01, p02;
	p01=sub3(p1, p0);
	p02=sub3(p2, p0);
	return pn2f3(p0, cross3(p01, p02));
}
/*
 * Compute point common to three faces.
 * Cramer's rule, yuk.
 */
Point3 fff2p3(Point3 f0, Point3 f1, Point3 f2){
	double det;
	Point3 p;
	det=dot3(f0, cross3(f1, f2));
	if(fabs(det)<SMALL){	/* parallel planes, bogus answer */
		p.x=0.;
		p.y=0.;
		p.z=0.;
		p.w=0.;
		return p;
	}
	p.x=(f0.w*(f2.y*f1.z-f1.y*f2.z)
		+f1.w*(f0.y*f2.z-f2.y*f0.z)+f2.w*(f1.y*f0.z-f0.y*f1.z))/det;
	p.y=(f0.w*(f2.z*f1.x-f1.z*f2.x)
		+f1.w*(f0.z*f2.x-f2.z*f0.x)+f2.w*(f1.z*f0.x-f0.z*f1.x))/det;
	p.z=(f0.w*(f2.x*f1.y-f1.x*f2.y)
		+f1.w*(f0.x*f2.y-f2.x*f0.y)+f2.w*(f1.x*f0.y-f0.x*f1.y))/det;
	p.w=1.;
	return p;
}
/*
 * pdiv4 does perspective division to convert a projective point to affine coordinates.
 */
Point3 pdiv4(Point3 a){
	if(a.w==0) return a;
	a.x/=a.w;
	a.y/=a.w;
	a.z/=a.w;
	a.w=1.;
	return a;
}
Point3 add4(Point3 a, Point3 b){
	a.x+=b.x;
	a.y+=b.y;
	a.z+=b.z;
	a.w+=b.w;
	return a;
}
Point3 sub4(Point3 a, Point3 b){
	a.x-=b.x;
	a.y-=b.y;
	a.z-=b.z;
	a.w-=b.w;
	return a;
}
