#include "mplot.h"
void openpl(char *s){
	m_initialize(s);
	e0->left=mapminx;
	e0->bottom=mapmaxy;
	e0->sidex=mapmaxx-mapminx;
	e0->sidey=mapminy-mapmaxy;
	e0->scalex=e0->sidex;
	e0->scaley=e0->sidey;
	sscpy(e0, e1);
	move(0., 0.);
}
