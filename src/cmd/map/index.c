#include <u.h>
#include <libc.h>
#include "map.h"

static proj Yaitoff(double p0, double p1){USED(p0); USED(p1); return aitoff();}
static proj Yalbers(double p0,double p1){USED(p0); USED(p1); return albers(p0,p1);}
static proj Yazequalarea(double p0, double p1){USED(p0); USED(p1); return azequalarea();}
static proj Yazequidistant(double p0, double p1){USED(p0); USED(p1); return azequidistant();}
static proj Ybicentric(double p0,double p1){USED(p0); USED(p1); return bicentric(p0);}
static proj Ybonne(double p0,double p1){USED(p0); USED(p1); return bonne(p0);}
static proj Yconic(double p0,double p1){USED(p0); USED(p1); return conic(p0);}
static proj Ycylequalarea(double p0,double p1){USED(p0); USED(p1); return cylequalarea(p0);}
static proj Ycylindrical(double p0, double p1){USED(p0); USED(p1); return cylindrical();}
static proj Yelliptic(double p0,double p1){USED(p0); USED(p1); return elliptic(p0);}
static proj Yfisheye(double p0,double p1){USED(p0); USED(p1); return fisheye(p0);}
static proj Ygall(double p0,double p1){USED(p0); USED(p1); return gall(p0);}
static proj Ygilbert(double p0, double p1){USED(p0); USED(p1); return gilbert();}
static proj Yglobular(double p0, double p1){USED(p0); USED(p1); return globular();}
static proj Ygnomonic(double p0, double p1){USED(p0); USED(p1); return gnomonic();}
static proj Yguyou(double p0, double p1){USED(p0); USED(p1); return guyou();}
static proj Yharrison(double p0,double p1){USED(p0); USED(p1); return harrison(p0,p1);}
static proj Yhex(double p0, double p1){USED(p0); USED(p1); return hex();}
static proj Yhoming(double p0,double p1){USED(p0); USED(p1); return homing(p0);}
static proj Ylagrange(double p0, double p1){USED(p0); USED(p1); return lagrange();}
static proj Ylambert(double p0,double p1){USED(p0); USED(p1); return lambert(p0,p1);}
static proj Ylaue(double p0, double p1){USED(p0); USED(p1); return laue();}
static proj Ylune(double p0,double p1){USED(p0); USED(p1); return lune(p0,p1);}
static proj Ymecca(double p0, double p1){USED(p0); USED(p1); return mecca(p0);}
static proj Ymercator(double p0, double p1){USED(p0); USED(p1); return mercator();}
static proj Ymollweide(double p0, double p1){USED(p0); USED(p1); return mollweide();}
static proj Ynewyorker(double p0,double p1){USED(p0); USED(p1); return newyorker(p0);}
static proj Yorthographic(double p0, double p1){USED(p0); USED(p1); return orthographic();}
static proj Yperspective(double p0,double p1){USED(p0); USED(p1); return perspective(p0);}
static proj Ypolyconic(double p0, double p1){USED(p0); USED(p1); return polyconic();}
static proj Yrectangular(double p0,double p1){USED(p0); USED(p1); return rectangular(p0);}
static proj Ysimpleconic(double p0,double p1){USED(p0); USED(p1); return simpleconic(p0,p1);}
static proj Ysinusoidal(double p0, double p1){USED(p0); USED(p1); return sinusoidal();}
static proj Ysp_albers(double p0,double p1){USED(p0); USED(p1); return sp_albers(p0,p1);}
static proj Ysp_mercator(double p0, double p1){USED(p0); USED(p1); return sp_mercator();}
static proj Ysquare(double p0, double p1){USED(p0); USED(p1); return square();}
static proj Ystereographic(double p0, double p1){USED(p0); USED(p1); return stereographic();}
static proj Ytetra(double p0, double p1){USED(p0); USED(p1); return tetra();}
static proj Ytrapezoidal(double p0,double p1){USED(p0); USED(p1); return trapezoidal(p0,p1);}
static proj Yvandergrinten(double p0, double p1){USED(p0); USED(p1); return vandergrinten();}

struct index index[] = {
	{"aitoff", Yaitoff, 0, picut, 0, 0, 0},
	{"albers", Yalbers, 2, picut, 3, 0, 0},
	{"azequalarea", Yazequalarea, 0, nocut, 1, 0, 0},
	{"azequidistant", Yazequidistant, 0, nocut, 1, 0, 0},
	{"bicentric", Ybicentric, 1, nocut, 0, 0, 0},
	{"bonne", Ybonne, 1, picut, 0, 0, 0},
	{"conic", Yconic, 1, picut, 0, 0, 0},
	{"cylequalarea", Ycylequalarea, 1, picut, 3, 0, 0},
	{"cylindrical", Ycylindrical, 0, picut, 0, 0, 0},
	{"elliptic", Yelliptic, 1, picut, 0, 0, 0},
	{"fisheye", Yfisheye, 1, nocut, 0, 0, 0},
	{"gall", Ygall, 1, picut, 3, 0, 0},
	{"gilbert", Ygilbert, 0, picut, 0, 0, 0},
	{"globular", Yglobular, 0, picut, 0, 0, 0},
	{"gnomonic", Ygnomonic, 0, nocut, 0, 0, plimb},
	{"guyou", Yguyou, 0, guycut, 0, 0, 0},
	{"harrison", Yharrison, 2, nocut, 0, 0, plimb},
	{"hex", Yhex, 0, hexcut, 0, 0, 0},
	{"homing", Yhoming, 1, nocut, 3, 0, hlimb},
	{"lagrange", Ylagrange,0,picut,0, 0, 0},
	{"lambert", Ylambert, 2, picut, 0, 0, 0},
	{"laue", Ylaue, 0, nocut, 0, 0, 0},
	{"lune", Ylune, 2, nocut, 0, 0, 0},
	{"mecca", Ymecca, 1, picut, 3, 0, mlimb},
	{"mercator", Ymercator, 0, picut, 3, 0, 0},
	{"mollweide", Ymollweide, 0, picut, 0, 0, 0},
	{"newyorker", Ynewyorker, 1, nocut, 0, 0, 0},
	{"orthographic", Yorthographic, 0, nocut, 0, 0, olimb},
	{"perspective", Yperspective, 1, nocut, 0, 0, plimb},
	{"polyconic", Ypolyconic, 0, picut, 0, 0, 0},
	{"rectangular", Yrectangular, 1, picut, 3, 0, 0},
	{"simpleconic", Ysimpleconic, 2, picut, 3, 0, 0},
	{"sinusoidal", Ysinusoidal, 0, picut, 0, 0, 0},
	{"sp_albers", Ysp_albers, 2, picut, 3, 1, 0},
	{"sp_mercator", Ysp_mercator, 0, picut, 0, 1, 0},
	{"square", Ysquare, 0, picut, 0, 0, 0},
	{"stereographic", Ystereographic, 0, nocut, 0, 0, 0},
	{"tetra", Ytetra, 0, tetracut, 0, 0, 0},
	{"trapezoidal", Ytrapezoidal, 2, picut, 3, 0, 0},
	{"vandergrinten", Yvandergrinten, 0, picut, 0, 0, 0},
	0
};
