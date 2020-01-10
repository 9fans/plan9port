#include "mplot.h"
void plotdisc(double xc, double yc, double r){
	Point p;
	int rad;
	p.x=SCX(xc);
	p.y=SCY(yc);
	if (r < 0)
		rad=SCR(-r);
	else
		rad=SCR(r);
	fillellipse(screen, p, rad, rad, getcolor(e1->foregr), ZP);
}
