#include "mplot.h"
void box(double x0, double y0, double x1, double y1){
	move(x0, y0);
	vec(x0, y1);
	vec(x1, y1);
	vec(x1, y0);
	vec(x0, y0);
}
