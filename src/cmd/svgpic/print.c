#include <stdio.h>
#include <math.h>
#include "pic.h"
#include "y.tab.h"

void dotext(obj *);
void ellipse(double, double, double, double);
void circle(double, double, double);
void arc(double, double, double, double, double, double, double);
void arrow(double, double, double, double, double, double, double, int);
void line(double, double, double, double, int, double);
void box(double, double, double, double, int, double);
void spline(double x, double y, double n, ofloat *p, int dashed, double ddval);
void move(double, double);
void troff(char *);
void dot(void);
void fillstart(double, int, int), fillend(void);

void print(void)
{
	obj *p;
	int i, j, k, m;
	int fill, vis, invis;
	double x0, y0, x1, y1, ox, oy, dx, dy, ndx, ndy;

	x1 = y1 = 0.0; /* Botch? (gcc) */

	for (i = 0; i < nobj; i++) {
		p = objlist[i];
		ox = p->o_x;
		oy = p->o_y;
		if (p->o_count >= 1)
			x1 = p->o_val[0];
		if (p->o_count >= 2)
			y1 = p->o_val[1];
		m = p->o_mode;
		fill = p->o_attr & FILLBIT;
		invis = p->o_attr & INVIS;
		vis = !invis;
		switch (p->o_type) {
		case TROFF:
			troff(text[p->o_nt1].t_val);
			break;
		case BOX:
		case BLOCK:
			x0 = ox - x1 / 2;
			y0 = oy - y1 / 2;
			x1 = ox + x1 / 2;
			y1 = oy + y1 / 2;
			if (fill) {
				move(x0, y0);
				fillstart(p->o_fillval, vis, fill);
			}
			if (p->o_type == BLOCK)
				;	/* nothing at all */
			else if (invis && !fill)
				;	/* nothing at all */
			else
				box(x0, y0, x1, y1, p->o_attr & (DOTBIT|DASHBIT), p->o_ddval);
			if (fill)
				fillend();
			move(ox, oy);
			dotext(p);	/* if there are any text strings */
			if (ishor(m))
				move(isright(m) ? x1 : x0, oy);	/* right side */
			else
				move(ox, isdown(m) ? y0 : y1);	/* bottom */
			break;
		case BLOCKEND:
			break;
		case CIRCLE:
			if (fill)
				fillstart(p->o_fillval, vis, fill);
			if (vis || fill)
				circle(ox, oy, x1);
			if (fill)
				fillend();
			move(ox, oy);
			dotext(p);
			if (ishor(m))
				move(ox + isright(m) ? x1 : -x1, oy);
			else
				move(ox, oy + isup(m) ? x1 : -x1);
			break;
		case ELLIPSE:
			if (fill)
				fillstart(p->o_fillval, vis, fill);
			if (vis || fill)
				ellipse(ox, oy, x1, y1);
			if (fill)
				fillend();
			move(ox, oy);
			dotext(p);
			if (ishor(m))
				move(ox + isright(m) ? x1 : -x1, oy);
			else
				move(ox, oy - isdown(m) ? y1 : -y1);
			break;
		case ARC:
			if (fill) {
				move(ox, oy);
				fillstart(p->o_fillval, vis, fill);
			}
			if (p->o_attr & HEAD1)
				arrow(x1 - (y1 - oy), y1 + (x1 - ox),
				      x1, y1, p->o_val[4], p->o_val[5], p->o_val[5]/p->o_val[6]/2, p->o_nhead);
			if (invis && !fill)
				/* probably wrong when it's cw */
				move(x1, y1);
			else
				arc(ox, oy, x1, y1, p->o_val[2], p->o_val[3], p->o_val[6]);
			if (p->o_attr & HEAD2)
				arrow(p->o_val[2] + p->o_val[3] - oy, p->o_val[3] - (p->o_val[2] - ox),
				      p->o_val[2], p->o_val[3], p->o_val[4], p->o_val[5], -p->o_val[5]/p->o_val[6]/2, p->o_nhead);
			if (fill)
				fillend();
			if (p->o_attr & CW_ARC)
				move(x1, y1);	/* because drawn backwards */
			move(ox, oy);
			dotext(p);
			break;
		case LINE:
		case ARROW:
		case SPLINE:
			if (fill) {
				move(ox, oy);
				fillstart(p->o_fillval, vis, fill);
			}
			if (vis && p->o_attr & HEAD1)
				arrow(ox + p->o_val[5], oy + p->o_val[6], ox, oy, p->o_val[2], p->o_val[3], 0.0, p->o_nhead);
			if (invis && !fill)
				move(x1, y1);
			else if (p->o_type == SPLINE)
				spline(ox, oy, p->o_val[4], &p->o_val[5], p->o_attr & (DOTBIT|DASHBIT), p->o_ddval);
			else {
				dx = ox;
				dy = oy;
				for (k=0, j=5; k < p->o_val[4]; k++, j += 2) {
					ndx = dx + p->o_val[j];
					ndy = dy + p->o_val[j+1];
					line(dx, dy, ndx, ndy, p->o_attr & (DOTBIT|DASHBIT), p->o_ddval);
					dx = ndx;
					dy = ndy;
				}
			}
			if (vis && p->o_attr & HEAD2) {
				dx = ox;
				dy = oy;
				for (k = 0, j = 5; k < p->o_val[4] - 1; k++, j += 2) {
					dx += p->o_val[j];
					dy += p->o_val[j+1];
				}
				arrow(dx, dy, x1, y1, p->o_val[2], p->o_val[3], 0.0, p->o_nhead);
			}
			if (fill)
				fillend();
			move((ox + x1)/2, (oy + y1)/2);	/* center */
			dotext(p);
			break;
		case MOVE:
			move(ox, oy);
			break;
		case TEXT:
			move(ox, oy);
                        if (vis)
				dotext(p);
			break;
		}
	}
}

void dotext(obj *p)	/* print text strings of p in proper vertical spacing */
{
	int i, nhalf;
	void label(char *, int, int);

	nhalf = p->o_nt2 - p->o_nt1 - 1;
	for (i = p->o_nt1; i < p->o_nt2; i++) {
		label(text[i].t_val, text[i].t_type, nhalf);
		nhalf -= 2;
	}
}
