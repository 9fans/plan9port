#include "mplot.h"
void rmove(double xx, double yy){
	e1->copyx += xx;
	e1->copyy += yy;
	move(e1->copyx, e1->copyy);
}
