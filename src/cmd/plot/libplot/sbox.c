#include "mplot.h"
void sbox(double xx0, double yy0, double xx1, double yy1){
	int x0=SCX(xx0), y0=SCY(yy0), x1=SCX(xx1), y1=SCY(yy1);
	int t;
	if(x1<x0){ t=x0; x0=x1; x1=t; }
	if(y1<y0){ t=y0; y0=y1; y1=t; }
	if(x0<clipminx) x0=clipminx;
	if(y0<clipminy) y0=clipminy;
	if(x1>clipmaxx) x1=clipmaxx;
	if(y1>clipmaxy) y1=clipmaxy;
	if(x1<x0 || y1<y0) return;
	m_clrwin(x0, y0, x1, y1, e1->backgr);
}
