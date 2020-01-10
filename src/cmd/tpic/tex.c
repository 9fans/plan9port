#include <math.h>
#include <stdio.h>
#include "tex.h"

void
devarc(double x1, double y1, double x2, double y2, double xc, double yc, int r)
{
	double t, start, stop;
	int rad;

	/* tpic arcs go clockwise, and angles are measured clockwise */
	start = atan2(y2-yc, x2-xc);
	stop = atan2(y1-yc, x1-xc);
	if (r<0) {
		t = start; start = stop; stop = t;
	}
	rad = SCX(sqrt((x1-xc)*(x1-xc)+(y1-yc)*(y1-yc)));
	fprintf(TEXFILE, "    \\special{ar %d %d %d %d %6.3f %6.3f}%%\n",
		TRX(xc), TRY(yc), rad, rad, -start, -stop);
}

void
box(double x0, double y0, double x1, double y1)
{
	fprintf(TEXFILE,"    \\special{pa %d %d}",TRX(x0),TRY(y0));
	fprintf(TEXFILE,"\\special{pa %d %d}",TRX(x1),TRY(y0));
	fprintf(TEXFILE,"\\special{pa %d %d}",TRX(x1),TRY(y1));
	fprintf(TEXFILE,"\\special{pa %d %d}",TRX(x0),TRY(y1));
	fprintf(TEXFILE,"\\special{pa %d %d}",TRX(x0),TRY(y0));
	switch(e1->pen){
	case DASHPEN:
		fprintf(TEXFILE,"\\special{da %6.3f}%%\n", e1->dashlen); break;
	case DOTPEN:
		fprintf(TEXFILE,"\\special{dt %6.3f}%%\n", e1->dashlen); break;
	case SOLIDPEN:
	default:
		fprintf(TEXFILE,"\\special{fp}%%\n"); break;
	}
}

void
circle(double xc, double yc, double r)
{
	int rad = SCX(r);

	fprintf(TEXFILE, "    \\special{ar %d %d %d %d 0.0 6.2832}%%\n",
		TRX(xc), TRY(yc), rad, rad);
}

void
closepl(void)
{
	fprintf(TEXFILE, "    \\kern %6.3fin\n  }\\vss}%%\n", INCHES(e1->sidex));
	fprintf(TEXFILE, "  \\kern %6.3fin\n}\n", INCHES(e1->sidey));
}

void
disc(double xc, double yc, double r)
{
	fprintf(TEXFILE, "    \\special{bk}%%\n");
	circle(xc, yc, r);
}

void
erase(void)
{
}

void
fill(int num[], double *ff[])
{
	double *xp, *yp, **fp, x0, y0;
	int i, *n;
	n = num;
	fp = ff;
	while((i = *n++)){
		xp = *fp++;
		yp = xp+1;
		x0 = *xp;
		y0 = *yp;
		move(x0, y0);
		while(--i){
			xp += 2;
			yp += 2;
			vec(*xp, *yp);
		}
		if (*(xp-2) != x0 || *(yp-2) != y0)
			vec(x0, y0);
	}
}

void
frame(double xs, double ys, double xf, double yf)
{
	double	osidex, osidey;
	osidex = e1->sidex;
	osidey = e1->sidey;
	e1->left = xs * (e0->left + e0->sidex);
	e1->bottom = ys* (e0->bottom +  e0->sidey);
	e1->sidex = (xf-xs) * e0->sidex;
	e1->sidey = (yf-ys) * e0->sidey;
	e1->scalex *= (e1->sidex / osidex);
	e1->scaley *= (e1->sidey / osidey);
}

void
line(double x0, double y0, double x1, double y1)
{
	move(x0, y0);
	vec(x1, y1);
}

void
move(double xx, double yy)
{
	e1->copyx = xx;
	e1->copyy = yy;
}

extern	double	xmin, ymin, xmax, ymax;

/* tpic TeX coord system uses millinches, printer's points for pensize */
/* positive y downward, origin at upper left */

#define pHEIGHT 5000.
#define pWIDTH  5000.
#define pPENSIZE 9
#define pPSIZE 10
#define pDLEN .05
struct penvir E[2] = {
{0.,pHEIGHT,0.,0.,1.,-1.,pWIDTH,pHEIGHT,0.,0.,0,pPSIZE,SOLIDPEN,pPENSIZE,pDLEN},
{0.,pHEIGHT,0.,0.,1.,-1.,pWIDTH,pHEIGHT,0.,0.,0,pPSIZE,SOLIDPEN,pPENSIZE,pDLEN}
};
struct penvir *e0 = E, *e1 = &E[1];
FILE *TEXFILE;

void
openpl(void)
{
	TEXFILE = stdout;

	space(xmin, ymin, xmax, ymax);
	fprintf(TEXFILE,"\\catcode`@=11\n");
	fprintf(TEXFILE, "\\expandafter\\ifx\\csname graph\\endcsname\\relax");
	fprintf(TEXFILE, " \\alloc@4\\box\\chardef\\insc@unt\\graph\\fi\n");
	fprintf(TEXFILE, "\\catcode`@=12\n");
	fprintf(TEXFILE, "\\setbox\\graph=\\vtop{%%\n");
	fprintf(TEXFILE, "  \\baselineskip=0pt \\lineskip=0pt ");
	fprintf(TEXFILE, "\\lineskiplimit=0pt\n");
	fprintf(TEXFILE, "  \\vbox to0pt{\\hbox{%%\n");
	fprintf(TEXFILE, "    \\special{pn %d}%%\n", e1->pdiam);
}

void
range(double x0, double y0, double x1, double y1)
{
	e1->xmin = x0;
	e1->ymin = y0;
	if (x1-x0 < .0000001*e1->sidex)
		x1=x0+.0000001;
	if (y1-y0 < .0000001*e1->sidey)
		y1=y0+.0000001;
	e1->scalex = e0->scalex*e1->sidex / (x1 - x0);
	e1->scaley = e0->scaley*e1->sidey / (y1 - y0);
}

void
rmove(double xx, double yy)
{
	e1->copyx += xx;
	e1->copyy += yy;
}

void
rvec(double xx, double yy)
{
	vec(xx+e1->copyx, yy+e1->copyy);
}

void
sbox(double x0, double y0, double x1, double y1)
{
	fprintf(TEXFILE,"    \\special{bk}%%\n");
	box(x0, y0, x1, y1);
}

void
vec(double xx, double yy)
{
	fprintf(TEXFILE,"    \\special{pa %d %d}",TRX(e1->copyx),TRY(e1->copyy));
	e1->copyx = xx;
	e1->copyy = yy;
	fprintf(TEXFILE,"\\special{pa %d %d}",TRX(xx),TRY(yy));
	switch(e1->pen){
	case DASHPEN:
		fprintf(TEXFILE,"\\special{da %6.3f}%%\n", e1->dashlen); break;
	case DOTPEN:
		fprintf(TEXFILE,"\\special{dt %6.3f}%%\n", e1->dashlen); break;
	case SOLIDPEN:
	default:
		fprintf(TEXFILE,"\\special{fp}%%\n"); break;
	}
}
