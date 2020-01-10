#include <stdio.h>
#include <math.h>
#include <string.h>
#include "pic.h"
extern int dbg;

#define	abs(n)	(n >= 0 ? n : -(n))
#define	max(x,y)	((x)>(y) ? (x) : (y))

char	*textshift = "\\v'.2m'";	/* move text this far down */

/* scaling stuff defined by s command as X0,Y0 to X1,Y1 */
/* output dimensions set by -l,-w options to 0,0 to hmax, vmax */
/* default output is 6x6 inches */


double	xscale;
double	yscale;

double	hpos	= 0;	/* current horizontal position in output coordinate system */
double	vpos	= 0;	/* current vertical position; 0 is top of page */

double	htrue	= 0;	/* where we really are */
double	vtrue	= 0;

double	X0, Y0;		/* left bottom of input */
double	X1, Y1;		/* right top of input */

double	hmax;		/* right end of output */
double	vmax;		/* top of output (down is positive) */

extern	double	deltx;
extern	double	delty;
extern	double	xmin, ymin, xmax, ymax;

double	xconv(double), yconv(double), xsc(double), ysc(double);
void	space(double, double, double, double);
void	hgoto(double), vgoto(double), hmot(double), vmot(double);
void	move(double, double), movehv(double, double);

char	svgfill[40] = "transparent";
char	svgstroke[40] = "black";

void openpl(char *s)	/* initialize device; s is residue of .PS invocation line */
{
	double maxw, maxh, ratio = 1;
	double odeltx = deltx, odelty = delty;

	hpos = vpos = 0;
	maxw = getfval("maxpswid");
	maxh = getfval("maxpsht");
	if (deltx > maxw) {	/* shrink horizontal */
		ratio = maxw / deltx;
		deltx *= ratio;
		delty *= ratio;
	}
	if (delty > maxh) {	/* shrink vertical */
		ratio = maxh / delty;
		deltx *= ratio;
		delty *= ratio;
	}
	if (ratio != 1) {
		fprintf(stderr, "pic: %g X %g picture shrunk to", odeltx, odelty);
		fprintf(stderr, " %g X %g\n", deltx, delty);
	}
	space(xmin, ymin, xmax, ymax);

	printf("<svg height=\"%.3f\" width=\"%.3f\"\n", yconv(ymin)+10, xconv(xmax)+10);
	printf("     xmlns=\"http://www.w3.org/2000/svg\">\n");
	printf("<g transform=\"translate(5 5)\">\n");

	/*
	printf("... %g %g %g %g\n", xmin, ymin, xmax, ymax);
	printf("... %.3fi %.3fi %.3fi %.3fi\n",
		xconv(xmin), yconv(ymin), xconv(xmax), yconv(ymax));
	printf(".nr 00 \\n(.u\n");
	printf(".nf\n");
	printf(".PS %.3fi %.3fi %s", yconv(ymin), xconv(xmax), s);
	*/
}

void space(double x0, double y0, double x1, double y1)	/* set limits of page */
{
	X0 = x0;
	Y0 = y0;
	X1 = x1;
	Y1 = y1;
	xscale = deltx == 0.0 ? 1.0 : deltx / (X1-X0);
	yscale = delty == 0.0 ? 1.0 : delty / (Y1-Y0);

	xscale *= 144;
	yscale *= 144;
}

double xconv(double x)	/* convert x from external to internal form */
{
	return (x-X0) * xscale;
}

double xsc(double x)	/* convert x from external to internal form, scaling only */
{

	return (x) * xscale;
}

double yconv(double y)	/* convert y from external to internal form */
{
	return (Y1-y) * yscale;
}

double ysc(double y)	/* convert y from external to internal form, scaling only */
{
	return (y) * yscale;
}

void closepl(char *PEline)	/* clean up after finished */
{
	printf("</g>\n");
	printf("</svg>\n");
}

void move(double x, double y)	/* go to position x, y in external coords */
{
	hgoto(xconv(x));
	vgoto(yconv(y));
}

void movehv(double h, double v)	/* go to internal position h, v */
{
	hgoto(h);
	vgoto(v);
}

void hmot(double n)	/* generate n units of horizontal motion */
{
	hpos += n;
}

void vmot(double n)	/* generate n units of vertical motion */
{
	vpos += n;
}

void hgoto(double n)
{
	hpos = n;
}

void vgoto(double n)
{
	vpos = n;
}

void hvflush(void)	/* get to proper point for output */
{
/*
	if (fabs(hpos-htrue) >= 0.0005) {
		printf("\\h'%.3fi'", hpos - htrue);
		htrue = hpos;
	}
	if (fabs(vpos-vtrue) >= 0.0005) {
		printf("\\v'%.3fi'", vpos - vtrue);
		vtrue = vpos;
	}
*/
}

void printlf(int n, char *f)
{
}

void troff(char *s)	/* output troff right here */
{
	printf("%s\n", s);
}

void label(char *s, int t, int nh)	/* text s of type t nh half-lines up */
{
	char *anchor;

	if (!s)
		return;

	if (t & ABOVE)
		nh++;
	else if (t & BELOW)
		nh--;
	t &= ~(ABOVE|BELOW);
	anchor = 0;
	if (t & LJUST) {
		// default
	} else if (t & RJUST) {
		anchor = "end";
	} else {	/* CENTER */
		anchor = "middle";
	}
	printf("<text x=\"%.3f\" y=\"%.3f\"", hpos, vpos-(double)(nh-0.4)*(12.0/72)/2*144);
	if(anchor)
		printf(" text-anchor=\"%s\"", anchor);
	printf(">%s</text>\n", s);
}

void line(double x0, double y0, double x1, double y1, int attr, double ddval)	/* draw line from x0,y0 to x1,y1 */
{
	printf("<path d=\"M %.3f %.3f L %.3f %.3f\" fill=\"transparent\" stroke=\"black\"", xconv(x0), yconv(y0), xconv(x1), yconv(y1));
	if(attr & DASHBIT)
		printf(" stroke-dasharray=\"%.3f, %.3f\"", xsc(ddval), xsc(ddval));
	else if(attr & DOTBIT)
		printf(" stroke-dasharray=\"1, %.3f\"", xsc(ddval));
	printf("/>\n");
}

void arrow(double x0, double y0, double x1, double y1, double w, double h,
	 double ang, int nhead) 	/* draw arrow (without shaft) */
{
	double alpha, rot, drot, hyp;
	double dx, dy;
	int i;

	rot = atan2(w / 2, h);
	hyp = sqrt(w/2 * w/2 + h * h);
	alpha = atan2(y1-y0, x1-x0) + ang;
	if (nhead < 2)
		nhead = 2;
	dprintf("rot=%g, hyp=%g, alpha=%g\n", rot, hyp, alpha);
	printf("<path d=\"");
	for (i = 1; i >= 0; i--) {
		drot = 2 * rot / (double) (2-1) * (double) i;
		dx = hyp * cos(alpha + PI - rot + drot);
		dy = hyp * sin(alpha + PI - rot + drot);
		dprintf("dx,dy = %g,%g\n", dx, dy);
		if(i == 1)
			printf("M %.3f %.3f L %.3f %.3f", xconv(x1+dx), yconv(y1+dy), xconv(x1), yconv(y1));
		else
			printf(" L %.3f %.3f", xconv(x1+dx), yconv(y1+dy));
	}
	if (nhead > 2)
		printf(" Z");
	printf("\"");
	if(nhead > 3)
		printf(" fill=\"black\" stroke=\"black\"");
	else if(nhead == 3)
		printf(" fill=\"white\" stroke=\"black\"");
	else
		printf(" fill=\"transparent\" stroke=\"black\"");
	printf("/>\n");
}

double lastgray = 0;

void fillstart(double v, int vis, int fill)
{
	int x;

	if(fill) {
		x = (int)(v*255.0);
		sprintf(svgfill, "#%02x%02x%02x", x, x, x);
	} else
		strcpy(svgfill, "transparent");
	if(vis)
		strcpy(svgstroke, "black");
	else
		strcpy(svgstroke, "transparent");
}

void fillend(void)
{
	strcpy(svgfill, "transparent");
	strcpy(svgstroke, "black");
}

void box(double x0, double y0, double x1, double y1, int attr, double ddval)
{
	printf("<path d=\"M %.3f %.3f V %.3f H %.3f V %.3f Z\" fill=\"transparent\" stroke=\"black\"", xconv(x0), yconv(y0), yconv(y1), xconv(x1), yconv(y0));
	if(attr & DASHBIT)
		printf(" stroke-dasharray=\"%.3f, %.3f\"", xsc(ddval), xsc(ddval));
	else if(attr & DOTBIT)
		printf(" stroke-dasharray=\"1, %.3f\"", xsc(ddval));
	printf("/>\n");

}

void circle(double x, double y, double r)
{
	printf("<circle cx=\"%.3f\" cy=\"%.3f\" r=\"%.3f\" fill=\"%s\" stroke=\"%s\"/>\n", xconv(x), yconv(y), xsc(r), svgfill, svgstroke);
}

void spline(double x, double y, double n, ofloat *p, int attr, double ddval)
{
	int i;
	double x1, y1;

	printf("<path d=\"M %.3f %.3f", xconv(x), yconv(y));
	x1 = 0;
	y1 = 0;
	for (i = 0; i < 2 * n; i += 2) {
		x1 = x;
		y1 = y;
		x += p[i];
		y += p[i+1];
		if(i == 0)
			printf(" L %.3f %.3f", xconv((x+x1)/2), yconv((y+y1)/2));
		else
			printf(" Q %.3f %.3f %.3f %.3f", xconv(x1), yconv(y1), xconv((x+x1)/2), yconv((y+y1)/2));
	}
	printf(" L %.3f %.3f", xconv(x), yconv(y));
	printf("\" fill=\"%s\" stroke=\"%s\"", svgfill, svgstroke);
	if(attr & DASHBIT)
		printf(" stroke-dasharray=\"%.3f, %.3f\"", xsc(ddval), xsc(ddval));
	else if(attr & DOTBIT)
		printf(" stroke-dasharray=\"1, %.3f\"", xsc(ddval));
	printf("/>\n");
}

void ellipse(double x, double y, double r1, double r2)
{
	printf("<ellipse cx=\"%.3f\" cy=\"%.3f\" rx=\"%.3f\" ry=\"%.3f\" fill=\"%s\" stroke=\"%s\"/>\n", xconv(x), yconv(y), xsc(r1), ysc(r2), svgfill, svgstroke);
}

void arc(double x, double y, double x0, double y0, double x1, double y1, double r)	/* draw arc with center x,y */
{
	printf("<path d=\"M %.3f %.3f A %.3f %.3f %d %d %d %.3f %.3f\" fill=\"%s\" stroke=\"%s\"/>\n",
		xconv(x0), yconv(y0),
		xsc(r), ysc(r), 0, 0, 0, xconv(x1), yconv(y1),
		svgfill, svgstroke);
}
