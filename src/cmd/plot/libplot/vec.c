#include "mplot.h"
#define	code(x, y)	((x<clipminx?1:x>clipmaxx?2:0)|(y<clipminy?4:y>clipmaxy?8:0))
void vec(double xx, double yy){
	int x0, y0, x1, y1, c0, c1, c, tx, ty;
	double t;
	t=SCX(e1->copyx); if(fabs(t)>BIGINT) return; x0=t;
	t=SCY(e1->copyy); if(fabs(t)>BIGINT) return; y0=t;
	t=SCX(xx); if(fabs(t)>BIGINT) return; x1=t;
	t=SCY(yy); if(fabs(t)>BIGINT) return; y1=t;
	e1->copyx=xx;
	e1->copyy=yy;
	/* clipping -- what a concept */
	c0=code(x0, y0);
	c1=code(x1, y1);
	while(c0|c1){
		if(c0&c1) return;
		c=c0?c0:c1;
		if(c&1)      ty=y0+(y1-y0)*(clipminx-x0)/(x1-x0), tx=clipminx;
		else if(c&2) ty=y0+(y1-y0)*(clipmaxx-x0)/(x1-x0), tx=clipmaxx;
		else if(c&4) tx=x0+(x1-x0)*(clipminy-y0)/(y1-y0), ty=clipminy;
		else         tx=x0+(x1-x0)*(clipmaxy-y0)/(y1-y0), ty=clipmaxy;
		if(c==c0) x0=tx, y0=ty, c0=code(x0, y0);
		else      x1=tx, y1=ty, c1=code(x1, y1);
	}
	m_vector(x0, y0, x1, y1, e1->foregr);
}
