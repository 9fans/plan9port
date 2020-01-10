/*
 * Quaternion arithmetic:
 *	qadd(q, r)	returns q+r
 *	qsub(q, r)	returns q-r
 *	qneg(q)		returns -q
 *	qmul(q, r)	returns q*r
 *	qdiv(q, r)	returns q/r, can divide check.
 *	qinv(q)		returns 1/q, can divide check.
 *	double qlen(p)	returns modulus of p
 *	qunit(q)	returns a unit quaternion parallel to q
 * The following only work on unit quaternions and rotation matrices:
 *	slerp(q, r, a)	returns q*(r*q^-1)^a
 *	qmid(q, r)	slerp(q, r, .5)
 *	qsqrt(q)	qmid(q, (Quaternion){1,0,0,0})
 *	qtom(m, q)	converts a unit quaternion q into a rotation matrix m
 *	mtoq(m)		returns a quaternion equivalent to a rotation matrix m
 */
#include <u.h>
#include <libc.h>
#include <draw.h>
#include <geometry.h>
void qtom(Matrix m, Quaternion q){
#ifndef new
	m[0][0]=1-2*(q.j*q.j+q.k*q.k);
	m[0][1]=2*(q.i*q.j+q.r*q.k);
	m[0][2]=2*(q.i*q.k-q.r*q.j);
	m[0][3]=0;
	m[1][0]=2*(q.i*q.j-q.r*q.k);
	m[1][1]=1-2*(q.i*q.i+q.k*q.k);
	m[1][2]=2*(q.j*q.k+q.r*q.i);
	m[1][3]=0;
	m[2][0]=2*(q.i*q.k+q.r*q.j);
	m[2][1]=2*(q.j*q.k-q.r*q.i);
	m[2][2]=1-2*(q.i*q.i+q.j*q.j);
	m[2][3]=0;
	m[3][0]=0;
	m[3][1]=0;
	m[3][2]=0;
	m[3][3]=1;
#else
	/*
	 * Transcribed from Ken Shoemake's new code -- not known to work
	 */
	double Nq = q.r*q.r+q.i*q.i+q.j*q.j+q.k*q.k;
	double s = (Nq > 0.0) ? (2.0 / Nq) : 0.0;
	double xs = q.i*s,		ys = q.j*s,		zs = q.k*s;
	double wx = q.r*xs,		wy = q.r*ys,		wz = q.r*zs;
	double xx = q.i*xs,		xy = q.i*ys,		xz = q.i*zs;
	double yy = q.j*ys,		yz = q.j*zs,		zz = q.k*zs;
	m[0][0] = 1.0 - (yy + zz); m[1][0] = xy + wz;         m[2][0] = xz - wy;
	m[0][1] = xy - wz;         m[1][1] = 1.0 - (xx + zz); m[2][1] = yz + wx;
	m[0][2] = xz + wy;         m[1][2] = yz - wx;         m[2][2] = 1.0 - (xx + yy);
	m[0][3] = m[1][3] = m[2][3] = m[3][0] = m[3][1] = m[3][2] = 0.0;
	m[3][3] = 1.0;
#endif
}
Quaternion mtoq(Matrix mat){
#ifndef new
#define	EPS	1.387778780781445675529539585113525e-17	/* 2^-56 */
	double t;
	Quaternion q;
	q.r=0.;
	q.i=0.;
	q.j=0.;
	q.k=1.;
	if((t=.25*(1+mat[0][0]+mat[1][1]+mat[2][2]))>EPS){
		q.r=sqrt(t);
		t=4*q.r;
		q.i=(mat[1][2]-mat[2][1])/t;
		q.j=(mat[2][0]-mat[0][2])/t;
		q.k=(mat[0][1]-mat[1][0])/t;
	}
	else if((t=-.5*(mat[1][1]+mat[2][2]))>EPS){
		q.i=sqrt(t);
		t=2*q.i;
		q.j=mat[0][1]/t;
		q.k=mat[0][2]/t;
	}
	else if((t=.5*(1-mat[2][2]))>EPS){
		q.j=sqrt(t);
		q.k=mat[1][2]/(2*q.j);
	}
	return q;
#else
	/*
	 * Transcribed from Ken Shoemake's new code -- not known to work
	 */
	/* This algorithm avoids near-zero divides by looking for a large
	 * component -- first r, then i, j, or k.  When the trace is greater than zero,
	 * |r| is greater than 1/2, which is as small as a largest component can be.
	 * Otherwise, the largest diagonal entry corresponds to the largest of |i|,
	 * |j|, or |k|, one of which must be larger than |r|, and at least 1/2.
	 */
	Quaternion qu;
	double tr, s;

	tr = mat[0][0] + mat[1][1] + mat[2][2];
	if (tr >= 0.0) {
		s = sqrt(tr + mat[3][3]);
		qu.r = s*0.5;
		s = 0.5 / s;
		qu.i = (mat[2][1] - mat[1][2]) * s;
		qu.j = (mat[0][2] - mat[2][0]) * s;
		qu.k = (mat[1][0] - mat[0][1]) * s;
	}
	else {
		int i = 0;
		if (mat[1][1] > mat[0][0]) i = 1;
		if (mat[2][2] > mat[i][i]) i = 2;
		switch(i){
		case 0:
			s = sqrt( (mat[0][0] - (mat[1][1]+mat[2][2])) + mat[3][3] );
			qu.i = s*0.5;
			s = 0.5 / s;
			qu.j = (mat[0][1] + mat[1][0]) * s;
			qu.k = (mat[2][0] + mat[0][2]) * s;
			qu.r = (mat[2][1] - mat[1][2]) * s;
			break;
		case 1:
			s = sqrt( (mat[1][1] - (mat[2][2]+mat[0][0])) + mat[3][3] );
			qu.j = s*0.5;
			s = 0.5 / s;
			qu.k = (mat[1][2] + mat[2][1]) * s;
			qu.i = (mat[0][1] + mat[1][0]) * s;
			qu.r = (mat[0][2] - mat[2][0]) * s;
			break;
		case 2:
			s = sqrt( (mat[2][2] - (mat[0][0]+mat[1][1])) + mat[3][3] );
			qu.k = s*0.5;
			s = 0.5 / s;
			qu.i = (mat[2][0] + mat[0][2]) * s;
			qu.j = (mat[1][2] + mat[2][1]) * s;
			qu.r = (mat[1][0] - mat[0][1]) * s;
			break;
		}
	}
	if (mat[3][3] != 1.0){
		s=1/sqrt(mat[3][3]);
		qu.r*=s;
		qu.i*=s;
		qu.j*=s;
		qu.k*=s;
	}
	return (qu);
#endif
}
Quaternion qadd(Quaternion q, Quaternion r){
	q.r+=r.r;
	q.i+=r.i;
	q.j+=r.j;
	q.k+=r.k;
	return q;
}
Quaternion qsub(Quaternion q, Quaternion r){
	q.r-=r.r;
	q.i-=r.i;
	q.j-=r.j;
	q.k-=r.k;
	return q;
}
Quaternion qneg(Quaternion q){
	q.r=-q.r;
	q.i=-q.i;
	q.j=-q.j;
	q.k=-q.k;
	return q;
}
Quaternion qmul(Quaternion q, Quaternion r){
	Quaternion s;
	s.r=q.r*r.r-q.i*r.i-q.j*r.j-q.k*r.k;
	s.i=q.r*r.i+r.r*q.i+q.j*r.k-q.k*r.j;
	s.j=q.r*r.j+r.r*q.j+q.k*r.i-q.i*r.k;
	s.k=q.r*r.k+r.r*q.k+q.i*r.j-q.j*r.i;
	return s;
}
Quaternion qdiv(Quaternion q, Quaternion r){
	return qmul(q, qinv(r));
}
Quaternion qunit(Quaternion q){
	double l=qlen(q);
	q.r/=l;
	q.i/=l;
	q.j/=l;
	q.k/=l;
	return q;
}
/*
 * Bug?: takes no action on divide check
 */
Quaternion qinv(Quaternion q){
	double l=q.r*q.r+q.i*q.i+q.j*q.j+q.k*q.k;
	q.r/=l;
	q.i=-q.i/l;
	q.j=-q.j/l;
	q.k=-q.k/l;
	return q;
}
double qlen(Quaternion p){
	return sqrt(p.r*p.r+p.i*p.i+p.j*p.j+p.k*p.k);
}
Quaternion slerp(Quaternion q, Quaternion r, double a){
	double u, v, ang, s;
	double dot=q.r*r.r+q.i*r.i+q.j*r.j+q.k*r.k;
	ang=dot<-1?PI:dot>1?0:acos(dot); /* acos gives NaN for dot slightly out of range */
	s=sin(ang);
	if(s==0) return ang<PI/2?q:r;
	u=sin((1-a)*ang)/s;
	v=sin(a*ang)/s;
	q.r=u*q.r+v*r.r;
	q.i=u*q.i+v*r.i;
	q.j=u*q.j+v*r.j;
	q.k=u*q.k+v*r.k;
	return q;
}
/*
 * Only works if qlen(q)==qlen(r)==1
 */
Quaternion qmid(Quaternion q, Quaternion r){
	double l;
	q=qadd(q, r);
	l=qlen(q);
	if(l<1e-12){
		q.r=r.i;
		q.i=-r.r;
		q.j=r.k;
		q.k=-r.j;
	}
	else{
		q.r/=l;
		q.i/=l;
		q.j/=l;
		q.k/=l;
	}
	return q;
}
/*
 * Only works if qlen(q)==1
 */
static Quaternion qident={1,0,0,0};
Quaternion qsqrt(Quaternion q){
	return qmid(q, qident);
}
