#include "mplot.h"
void range(double x0, double y0, double x1, double y1){
	e1->xmin = x0;
	e1->ymin = y0;
	e1->scalex = e1->sidex / (x1 - x0 );
	e1->scaley = e1->sidey / (y1 - y0 );
	e1->quantum=e0->quantum/sqrt(e1->scalex*e1->scalex +
		e1->scaley*e1->scaley);
	if(e1->quantum < .01)
		e1->quantum = .01;
}
