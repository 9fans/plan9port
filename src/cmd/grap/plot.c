#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "grap.h"
#include "y.tab.h"

void line(int type, Point p1, Point p2, Attr *desc)	/* draw a line segment */
{
	fprintf(tfd, "%s %s from %s",
		type==LINE ? "line" : "arrow",  desc_str(desc), xyname(p1));
	fprintf(tfd, " to %s", xyname(p2));	/* 'cause xyname is botched */
	fprintf(tfd, "\n");
	range(p1);
	range(p2);
}

void circle(double r, Point pt)		/* draw a circle */
{
	if (r > 0.0)
		fprintf(tfd, "circle rad %g at %s\n", r, xyname(pt));
	else
		fprintf(tfd, "\"\\s-3\\(ob\\s0\" at %s\n", xyname(pt));
	range(pt);
}

char *xyname(Point pt)	/* generate xy name macro for point p */
{
	static char buf[200];
	Obj *p;

	p = pt.obj;
	if (p->log & XFLAG) {
		if (pt.x <= 0.0)
			ERROR "can't take log of x coord %g", pt.x FATAL;
		logit(pt.x);
	}
	if (p->log & YFLAG) {
		if (pt.y <= 0.0)
			ERROR "can't take log of y coord %g", pt.y FATAL;
		logit(pt.y);
	}
	sprintf(buf, "xy_%s(%g,%g)", p->name, pt.x, pt.y);
	return buf;	/* WATCH IT:  static */
}

void pic(char *s)	/* fire out pic stuff directly */
{
	while (*s == ' ')
		s++;
	fprintf(tfd, "%s\n", s);
}

int	auto_x	= 0;	/* counts abscissa if none provided */

void numlist(void)	/* print numbers in default way */
{
	Obj *p;
	Point pt;
	int i;
	static char *spot = "\\(bu";
	Attr *ap;

	p = pt.obj = lookup(curr_coord, 1);
	if (nnum == 1) {
		nnum = 2;
		num[1] = num[0];
		num[0] = ++auto_x;
	}
	pt.x = num[0];
	if (p->attr && p->attr->sval)
		spot = p->attr->sval;
	for (i = 1; i < nnum; i++) {
		pt.y = num[i];
		if (p->attr == 0 || p->attr->type == 0) {
			ap = makesattr(tostring(spot));
			plot(ap, pt);
		} else
			next(p, pt, p->attr);
	}
	nnum = 0;
}

void plot(Attr *sl, Point pt)	/* put stringlist sl at point pt */
{
	fprintf(tfd, "%s at %s\n", slprint(sl), xyname(pt));
	range(pt);
	freeattr(sl);
}

void plotnum(double f, char *fmt, Point pt)	/* plot value f at point */
{
	char buf[100];

	if (fmt) {
		sprintf(buf, fmt, f);
		free(fmt);
	} else if (f >= 0.0)
		sprintf(buf, "%g", f);
	else
		sprintf(buf, "\\-%g", -f);
	fprintf(tfd, "\"%s\" at %s\n", buf, xyname(pt));
	range(pt);
}

void drawdesc(int type, Obj *p, Attr *desc, char *s)	/* set line description for p */
{
	p->attr = desc;
	p->attr->sval = s;
	if (type == NEW) {
		p->first = 0;	/* so it really looks new */
		auto_x = 0;
	}
}

void next(Obj *p, Point pt, Attr *desc)	/* add component to a path */
{
	char *s;

	if (p->first == 0) {
		p->first++;
		fprintf(tfd, "L%s: %s\n", p->name, xyname(pt));
	} else {
		fprintf(tfd, "line %s from L%s to %s; L%s: Here\n",
			desc_str(desc->type ? desc : p->attr),
			p->name, xyname(pt), p->name);
	}
	if (p->attr && (s=p->attr->sval)) {
		/* BUG: should fix size here */
		fprintf(tfd, "\"%s\" at %s\n", s, xyname(pt));
	}
	range(pt);
}
