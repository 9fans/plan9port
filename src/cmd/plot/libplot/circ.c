#include "mplot.h"
void circ(double xc, double yc, double r){
	Point p;
	int rad;
	p.x=SCX(xc);
	p.y=SCY(yc);
	if (r < 0)
		rad=SCR(-r);
	else
		rad=SCR(r);
	ellipse(screen, p, rad, rad, 0, getcolor(e1->foregr), ZP);
}
