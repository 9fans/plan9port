#include "mplot.h"
void rvec(double xx, double yy){
	e1->copyx += xx;
	e1->copyy += yy;
	vec(e1->copyx, e1->copyy);
}
