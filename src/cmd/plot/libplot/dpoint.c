#include "mplot.h"
void dpoint(double x, double y){
	draw(screen, Rect(SCX(x), SCY(y), SCX(x)+1, SCY(y)+1), getcolor(e1->foregr),
		nil, ZP);
	move(x, y);
}
