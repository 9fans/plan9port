#ifndef _GEOMETRY_H_
#define _GEOMETRY_H_ 1
#if defined(__cplusplus)
extern "C" { 
#endif


AUTOLIB(geometry)

typedef double Matrix[4][4];
typedef struct Point3 Point3;
typedef struct Quaternion Quaternion;
typedef struct Space Space;
struct Point3{
	double x, y, z, w;
};
struct Quaternion{
	double r, i, j, k;
};
struct Space{
	Matrix t;
	Matrix tinv;
	Space *next;
};
/*
 * 3-d point arithmetic
 */
Point3 add3(Point3 a, Point3 b);
Point3 sub3(Point3 a, Point3 b);
Point3 neg3(Point3 a);
Point3 div3(Point3 a, double b);
Point3 mul3(Point3 a, double b);
int eqpt3(Point3 p, Point3 q);
int closept3(Point3 p, Point3 q, double eps);
double dot3(Point3 p, Point3 q);
Point3 cross3(Point3 p, Point3 q);
double len3(Point3 p);
double dist3(Point3 p, Point3 q);
Point3 unit3(Point3 p);
Point3 midpt3(Point3 p, Point3 q);
Point3 lerp3(Point3 p, Point3 q, double alpha);
Point3 reflect3(Point3 p, Point3 p0, Point3 p1);
Point3 nearseg3(Point3 p0, Point3 p1, Point3 testp);
double pldist3(Point3 p, Point3 p0, Point3 p1);
double vdiv3(Point3 a, Point3 b);
Point3 vrem3(Point3 a, Point3 b);
Point3 pn2f3(Point3 p, Point3 n);
Point3 ppp2f3(Point3 p0, Point3 p1, Point3 p2);
Point3 fff2p3(Point3 f0, Point3 f1, Point3 f2);
Point3 pdiv4(Point3 a);
Point3 add4(Point3 a, Point3 b);
Point3 sub4(Point3 a, Point3 b);
/*
 * Quaternion arithmetic
 */
void qtom(Matrix, Quaternion);
Quaternion mtoq(Matrix);
Quaternion qadd(Quaternion, Quaternion);
Quaternion qsub(Quaternion, Quaternion);
Quaternion qneg(Quaternion);
Quaternion qmul(Quaternion, Quaternion);
Quaternion p9qdiv(Quaternion, Quaternion);
Quaternion qunit(Quaternion);
Quaternion qinv(Quaternion);
double qlen(Quaternion);
Quaternion slerp(Quaternion, Quaternion, double);
Quaternion qmid(Quaternion, Quaternion);
Quaternion qsqrt(Quaternion);
void qball(Rectangle, Mouse *, Quaternion *, void (*)(void), Quaternion *);
/*
 * Matrix arithmetic
 */
void ident(Matrix);
void matmul(Matrix, Matrix);
void matmulr(Matrix, Matrix);
double determinant(Matrix);
void adjoint(Matrix, Matrix);
double invertmat(Matrix, Matrix);
/*
 * Space stack routines
 */
Space *pushmat(Space *);
Space *popmat(Space *);
void rot(Space *, double, int);
void qrot(Space *, Quaternion);
void scale(Space *, double, double, double);
void move(Space *, double, double, double);
void xform(Space *, Matrix);
void ixform(Space *, Matrix, Matrix);
void look(Space *, Point3, Point3, Point3);
int persp(Space *, double, double, double);
void viewport(Space *, Rectangle, double);
Point3 xformpoint(Point3, Space *, Space *);
Point3 xformpointd(Point3, Space *, Space *);
Point3 xformplane(Point3, Space *, Space *);
#define	radians(d)	((d)*.01745329251994329572)

#ifndef NOPLAN9DEFINES
#define	qdiv	p9qdiv	/* for NetBSD */
#endif

#if defined(__cplusplus)
}
#endif
#endif
