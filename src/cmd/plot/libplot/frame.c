#include "mplot.h"
void frame(double xs, double ys, double xf, double yf){
	register double	osidex, osidey;
	osidex = e1->sidex;
	osidey = e1->sidey;
	e1->left = e0->left + xs * e0->sidex;
	e1->bottom = e0->bottom + ys * e0->sidey;
	e1->sidex = (xf-xs)*e0->sidex;
	e1->sidey = (yf-ys)*e0->sidey;
	e1->scalex *= (e1->sidex / osidex);
	e1->scaley *= (e1->sidey / osidey);
	e1->quantum=e0->quantum/sqrt(e1->scalex*e1->scalex +
		e1->scaley*e1->scaley);
	if(e1->quantum < .01)
		e1->quantum = .01;
}
