/*
#pragma	lib	"/sys/src/cmd/map/libmap/libmap.a$O"
#pragma	src	"/sys/src/cmd/map/libmap"
*/

#define index index0
#ifndef PI
#define PI	3.1415926535897932384626433832795028841971693993751
#endif

#define TWOPI (2*PI)
#define RAD (PI/180)
double	hypot(double, double);	/* sqrt(a*a+b*b) */
double	tan(double);		/* not in K&R library */

#define ECC .08227185422	/* eccentricity of earth */
#define EC2 .006768657997

#define FUZZ .0001
#define UNUSED 0.0		/* a dummy double parameter */

struct coord {
	double l;	/* lat or lon in radians*/
	double s;	/* sin */
	double c;	/* cos */
};
struct place {
	struct coord nlat;
	struct coord wlon;
};

typedef int (*proj)(struct place *, double *, double *);

struct index {		/* index of known projections */
	char *name;	/* name of projection */
	proj (*prog)(double, double);
			/* pointer to projection function */
	int npar;	/* number of params */
	int (*cut)(struct place *, struct place *, double *);
			/* function that handles cuts--eg longitude 180 */
	int poles;	/*1 S pole is a line, 2 N pole is, 3 both*/
	int spheroid;	/* poles must be at 90 deg if nonzero */
	int (*limb)(double *lat, double *lon, double resolution);
			/* get next place on limb */
			/* return -1 if done, 0 at gap, else 1 */
};


proj	aitoff(void);
proj	albers(double, double);
int	Xazequalarea(struct place *, double *, double *);
proj	azequalarea(void);
int	Xazequidistant(struct place *, double *, double *);
proj	azequidistant(void);
proj	bicentric(double);
proj	bonne(double);
proj	conic(double);
proj	cylequalarea(double);
int	Xcylindrical(struct place *, double *, double *);
proj	cylindrical(void);
proj	elliptic(double);
proj	fisheye(double);
proj	gall(double);
proj	gilbert(void);
proj	globular(void);
proj	gnomonic(void);
int	guycut(struct place *, struct place *, double *);
int	Xguyou(struct place *, double *, double *);
proj	guyou(void);
proj	harrison(double, double);
int	hexcut(struct place *, struct place *, double *);
proj	hex(void);
proj	homing(double);
int	hlimb(double*, double*, double resolution);
proj	lagrange(void);
proj	lambert(double, double);
proj	laue(void);
proj	lune(double, double);
proj	loxodromic(double);	/* not in library */
proj	mecca(double);
int	mlimb(double*, double*, double resolution);
proj	mercator(void);
proj	mollweide(void);
proj	newyorker(double);
proj	ortelius(double, double);	/* not in library */
int	Xorthographic(struct place *place, double *x, double *y);
proj	orthographic(void);
int	olimb(double*, double*, double);
proj	perspective(double);
int	plimb(double*, double*, double resolution);
int	Xpolyconic(struct place *, double *, double *);
proj	polyconic(void);
proj	rectangular(double);
proj	simpleconic(double, double);
int	Xsinusoidal(struct place *, double *, double *);
proj	sinusoidal(void);
proj	sp_albers(double, double);
proj	sp_mercator(void);
proj	square(void);
int	Xstereographic(struct place *, double *, double *);
proj	stereographic(void);
int	Xtetra(struct place *, double *, double *);
int	tetracut(struct place *, struct place *, double *);
proj	tetra(void);
proj	trapezoidal(double, double);
proj	vandergrinten(void);
proj	wreath(double, double);	/* not in library */

void	findxy(double, double *, double *);
void	albscale(double, double, double, double);
void	invalb(double, double, double *, double *);

#define csqrt map_csqrt	/* conflicts on FreeBSD 5 with gcc builtins */
#define cpow map_cpow
#define sincos map_sincos

void	cdiv(double, double, double, double, double *, double *);
void	cmul(double, double, double, double, double *, double *);
void	cpow(double, double, double *, double *, double);
void	csq(double, double, double *, double *);
void	csqrt(double, double, double *, double *);
void	ccubrt(double, double, double *, double *);
double	cubrt(double);
int	elco2(double, double, double, double, double, double *, double *);
void	cdiv2(double, double, double, double, double *, double *);
void	csqr(double, double, double *, double *);

void	orient(double, double, double);
void	latlon(double, double, struct place *);
void	deg2rad(double, struct coord *);
void	sincos(struct coord *);
void	normalize(struct place *);
void	invert(struct place *);
void	norm(struct place *, struct place *, struct coord *);
void	printp(struct place *);
void	copyplace(struct place *, struct place *);

int	picut(struct place *, struct place *, double *);
int	ckcut(struct place *, struct place *, double);
double	reduce(double);

void	getsyms(char *);
int	putsym(struct place *, char *, double, int);
void	filerror(char *s, char *f);
void	error(char *s);
int	doproj(struct place *, int *, int *);
int	cpoint(int, int, int);
int	plotpt(struct place *, int);
int	nocut(struct place *, struct place *, double *);

extern int (*projection)(struct place *, double *, double *);
