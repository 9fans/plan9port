#include <u.h>
#include <libc.h>
#include <stdio.h>
#include "map.h"
#include "iplot.h"

#define NSYMBOL 20

enum flag { POINT,ENDSEG,ENDSYM };
struct symb {
	double x, y;
	char name[10+1];
	enum flag flag;
} *symbol[NSYMBOL];

static int nsymbol;
static double halfrange = 1;
extern int halfwidth;
extern int vflag;

static int	getrange(FILE *);
static int	getsymbol(FILE *, int);
static void	setrot(struct place *, double, int);
static void	dorot(struct symb *, double *, double *);


void
getsyms(char *file)
{
	FILE *sf = fopen(file,"r");
	if(sf==0)
		filerror("cannot open", file);
	while(nsymbol<NSYMBOL-1 && getsymbol(sf,nsymbol))
		nsymbol++;
	fclose(sf);
}

static int
getsymbol(FILE *sf, int n)
{
	double x,y;
	char s[2];
	int i;
	struct symb *sp;
	for(;;) {
		if(fscanf(sf,"%1s",s)==EOF)
			return 0;
		switch(s[0]) {
		case ':':
			break;
		case 'o':
		case 'c':	/* cl */
			fscanf(sf,"%*[^\n]");
			continue;
		case 'r':
			if(getrange(sf))
				continue;
		default:
			error("-y file syntax error");
		}
		break;
	}
	sp = (struct symb*)malloc(sizeof(struct symb));
	symbol[n] = sp;
	if(fscanf(sf,"%10s",sp->name)!=1)
		return 0;
	i = 0;
	while(fscanf(sf,"%1s",s)!=EOF) {
		switch(s[0]) {
		case 'r':
			if(!getrange(sf))
				break;
			continue;
		case 'm':
			if(i>0)
				symbol[n][i-1].flag = ENDSEG;
			continue;
		case ':':
			ungetc(s[0],sf);
			break;
		default:
			ungetc(s[0],sf);
		case 'v':
			if(fscanf(sf,"%lf %lf",&x,&y)!=2)
				break;
			sp[i].x = x*halfwidth/halfrange;
			sp[i].y = y*halfwidth/halfrange;
			sp[i].flag = POINT;
			i++;
			sp = symbol[n] = (struct symb*)realloc(symbol[n],
					(i+1)*sizeof(struct symb));
			continue;
		}
		break;
	}
	if(i>0)
		symbol[n][i-1].flag = ENDSYM;
	else
		symbol[n] = 0;
	return 1;
}

static int
getrange(FILE *sf)
{
	double x,y,xmin,ymin;
	if(fscanf(sf,"%*s %lf %lf %lf %lf",
		&xmin,&ymin,&x,&y)!=4)
		return 0;
	x -= xmin;
	y -= ymin;
	halfrange = (x>y? x: y)/2;
	if(halfrange<=0)
		error("bad ra command in -y file");
	return 1;
}

/* r=0 upright;=1 normal;=-1 reverse*/
int
putsym(struct place *p, char *name, double s, int r)
{
	int x,y,n;
	struct symb *sp;
	double dx,dy;
	int conn = 0;
	for(n=0; symbol[n]; n++)
		if(strcmp(name,symbol[n]->name)==0)
			break;
	sp = symbol[n];
	if(sp==0)
		return 0;
	if(doproj(p,&x,&y)*vflag <= 0)
		return 1;
	setrot(p,s,r);
	for(;;) {
		dorot(sp,&dx,&dy);
		conn = cpoint(x+(int)dx,y+(int)dy,conn);
		switch(sp->flag) {
		case ENDSEG:
			conn = 0;
		case POINT:
			sp++;
			continue;
		case ENDSYM:
			break;
		}
		break;
	}
	return 1;
}

static double rot[2][2];

static void
setrot(struct place *p, double s, int r)
{
	double x0,y0,x1,y1;
	struct place up;
	up = *p;
	up.nlat.l += .5*RAD;
	sincos(&up.nlat);
	if(r&&(*projection)(p,&x0,&y0)) {
		if((*projection)(&up,&x1,&y1)<=0) {
			up.nlat.l -= RAD;
			sincos(&up.nlat);
			if((*projection)(&up,&x1,&y1)<=0)
				goto unit;
			x1 = x0 - x1;
			y1 = y0 - y1;
		} else {
			x1 -= x0;
			y1 -= y0;
		}
		x1 = r*x1;
		s /= hypot(x1,y1);
		rot[0][0] = y1*s;
		rot[0][1] = x1*s;
		rot[1][0] = -x1*s;
		rot[1][1] = y1*s;
	} else {
unit:
		rot[0][0] = rot[1][1] = s;
		rot[0][1] = rot[1][0] = 0;
	}
}

static void
dorot(struct symb *sp, double *px, double *py)
{
	*px = rot[0][0]*sp->x + rot[0][1]*sp->y;
	*py = rot[1][0]*sp->x + rot[1][1]*sp->y;
}
