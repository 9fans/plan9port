/* replacement for pltroff.c to produce a TeX file that makes a box */

#include <stdio.h>
#include <math.h>
#include "pic.h"

double rangex, rangey;  /* probably already available inside pic somewhere */
extern int dbg;
int frameno;

/*-----------copied from old version----------*/

void
arrow(double x0, double y0, double x1, double y1, double w, double h, double ang, int nhead) 	/* draw arrow (without shaft) */
	/* head wid w, len h, rotated ang */
	/* and drawn with nhead lines */
{
	double alpha, rot, drot, hyp;
	float dx, dy;
	int i;

	rot = atan2(w / 2, h);
	hyp = sqrt(w/2 * w/2 + h * h);
	alpha = atan2(y1-y0, x1-x0) + ang;
	if (nhead < 2)
		nhead = 2;
	for (i = nhead-1; i >= 0; i--) {
		drot = 2 * rot / (double) (nhead-1) * (double) i;
		dx = hyp * cos(alpha + PI - rot + drot);
		dy = hyp * sin(alpha + PI - rot + drot);
		line(x1+dx, y1+dy, x1, y1);
	}
}


/*-----------new code----------*/

void
printlf(int line, char *name)
{
}

void
fillstart(double v)	/* only choose black, light grey (.75), or white, for now */
{
	if (v<.05)
		fprintf(TEXFILE, "    \\special{bk}%%\n");
	else if (v>.95)
		fprintf(TEXFILE, "    \\special{wh}%%\n");
	else
		fprintf(TEXFILE, "    \\special{sh}%%\n");
}

void
fillend(void)
{
}

void
troff(char *s)
{
	int size;

	if (strncmp(s, ".ps", 3) == 0) {
	    if (sscanf(&s[3], " %d ", &size) > 0) {
		fprintf(TEXFILE, "    \\special{pn %d}%%\n", size);
		e1->pdiam = size;
	    } else fprintf(stderr, "Malformed .ps command: %s\n", s);
	}
}


void
space(double x0, double y0, double x1, double y1)	/* set limits of page */
{
	e0->sidex = e1->sidex = deltx*1000;
	e0->sidey = e1->sidey = e0->bottom = e1->bottom = delty*1000;
	range(x0, y0, x1, y1);
}

void
dot(void)
{
	/* use .005" radius at nominal 9pt pen size */
	disc(e1->copyx,e1->copyy,(e1->pdiam/9.0)*(4.3/e1->scalex));
}

void
label(char *s, int t, int nh)	/* text s of type t nh half-lines up */
{
	double nem;

	if (t & ABOVE)
		nh++;
	else if (t & BELOW)
		nh--;
	nem = .2 - nh*.6;
	fprintf(TEXFILE,"    \\rlap{\\kern %6.3fin\\lower%6.3fin\\hbox{\\lower%5.2fem\\hbox to 0pt{",
			INCHES(DTRX(e1->copyx)), INCHES(DTRY(e1->copyy)), nem);
	fprintf(TEXFILE,t&LJUST?"%s\\hss":(t&RJUST?"\\hss %s":"\\hss %s\\hss"),s);
	fprintf(TEXFILE,"}}}%%\n");
}

void
spline(double x, double y, double/*sic*/ n, float *p, int dashed, double ddval)
{
	int k, j;

	fprintf(TEXFILE,"    \\special{pa %d %d}%%\n",TRX(x),TRY(y));
	for(k=0, j=0; k<n; k++, j+=2){
		x += p[j];
		y += p[j+1];
		fprintf(TEXFILE,"    \\special{pa %d %d}%%\n",TRX(x),TRY(y));
	}
	fprintf(TEXFILE,"    \\special{sp}%%\n");
}

void
ellipse(double x, double y, double r1, double r2)
{
	fprintf(TEXFILE, "    \\special{ar %d %d %d %d 0.0 6.2832}%%\n",
		TRX(x), TRY(y), SCX(r1), -SCY(r2));
}

void
arc(double xc, double yc, double x0, double y0, double x1, double y1)	/* draw arc with center xc,yc */
{
	devarc(x0, y0, x1, y1, xc, yc, 1 );   /* radius=1 means counterclockwise */
}

/* If NOEXPANDDASH is defined, use this instead of the normal dotline
 * in print().  This dotline relies on vec() noticing that e1->pen
 * is not SOLIDPEN, and putting out a call to a different postscript
 * routine.
 */
#ifdef NOEXPANDDASH

void
dotline(double x0, double y0, double x1, double y1, int ddtype, double ddval)
{
	if (ddval != 0)
		e1->dashlen = ddval;
	e1->pen = (ddtype&DOTBIT)? DOTPEN : DASHPEN;
	move(x0, y0);
	vec(x1, y1);
	e1->pen = SOLIDPEN;
	e1->dashlen = e0->dashlen;
}
#endif
